// intel_blt.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 16.12.25.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "intel_blt.h"

#include <filesystem/devfs.h>
#include <klib/string.h>
#include <pci/msix.h>
#include <pci/pci.h>
#include <pci/pci_device.h>
#include <vespera/devices/device_manager.h>
#include <vespera/graphics/colors.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_types.h>
#include <vespera/time.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "bcs_regs.h"
#include "blt_commands.h"
#include "error_regs.h"
#include "gt_interrupt_regs.h"
#include "gt_reset_regs.h"
#include "interrupt_regs.h"
#include "pci_config_regs.h"

namespace blt {

    // =========================================================================
    // Constructor / Init
    // =========================================================================

    IntelBlt::IntelBlt(const pci::pci_device& igpu_dev)
        : igp_cfg_(reinterpret_cast<volatile INTEL_IGP_PCI_CONFIG*>(igpu_dev.header))
        , pci_id_(igpu_dev.id) {
        const phys_addr_t bar0 =
            make_phys(static_cast<u64>(igp_cfg_->gttmmadr_hi) << 32 | (igp_cfg_->gttmmadr_lo & GTTMMADR_ADDR_MASK));

        kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE, (1ULL << CacheDisabled));

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
        bcs_regs_ = reinterpret_cast<volatile BCS_REGS*>(mmio_base_ + BCS_RING_BASE);

        irq_vector_ = kernel::interrupts::get_free_vector();
    }

    bool IntelBlt::init_device() {
        if (!force_wake_enable()) return false;
        ggtt_init();
        bcs_power_enable();
        if (!bcs_reset()) return false;

        kernel::interrupts::allocate_vector(irq_vector_, reinterpret_cast<irq_handler_t>(bcs_irq_handler), this);
        if (!pci::try_enable_msi_or_msix(reinterpret_cast<volatile pci::PCI_HEADER0*>(igp_cfg_), irq_vector_, 1)) {
            Log::log_dbc("BLT: MSI enable failed");
            return false;
        }

        completion_flag_.init(false);
        bcs_interrupts_enable();
        bcs_error_reporting_init();

        auto hwsp = ggtt_alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        hwsp_cpu_addr_ = hwsp.cpu_addr;
        hwsp_gfx_addr_ = hwsp.gfx_addr;
        memset(hwsp_cpu_addr_, 0, PAGE_SIZE);

        HWS_PGA reg{};
        reg.set_address_bytes(gfx_raw(hwsp_gfx_addr_));
        bcs_regs_->hwsp.raw = reg.raw;

        const u32 ring_pages = ring_size_ / PAGE_SIZE;
        auto [cpu_addr, gfx_addr] = ggtt_alloc_persistent(ring_pages);
        ring_cpu_addr_ = cpu_addr;
        ring_gfx_addr_ = gfx_addr;
        memset(ring_cpu_addr_, 0, ring_size_);
        ring_init();

        RING_BUFFER_START start{};
        start.set_start_addr_bytes(gfx_raw(ring_gfx_addr_));

        RING_BUFFER_CTL ctl{};
        ctl.ring_enable = 1;
        ctl.set_ring_size_bytes(ring_size_);

        RING_BUFFER_HEAD head{};
        head.set_head_offset_bytes(0);

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(0);

        bcs_regs_->ring_start.raw = start.raw;
        bcs_regs_->ring_ctl.raw = ctl.raw;
        bcs_regs_->ring_head.raw = head.raw;
        bcs_regs_->ring_tail.raw = tail.raw;

        char name[16];
        DeviceManager::alloc_unique_device_name("intel_blt", name, sizeof(name));
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name)
                .set_type(DeviceType::Gpu)
                .set_class(DeviceClass::Graphics)
                .set_bus(BusType::Pci)
                .set_controller(ControllerType::IntelGpu)
                .with_gpu(this)
                .with_info(this)
        );

        DevFs::register_device(kd_);

        return true;
    }

    void IntelBlt::start_device(u32 screen_width, u32 screen_height) {
        fb_alloc(screen_width, screen_height, TileMode::Linear);
        scratch_init();
        worker_start(1);
        fb_set_display();
    }

    // =========================================================================
    // Worker
    // =========================================================================

    void IntelBlt::worker_start(const u8 cpu_id) {
        blt_queue_.init();

        const UnitConfig cfg = {
            .name = "intel-blt",
            .cpu_id = cpu_id,
            .priority = 4,
            .stack_size = 0x10000,
            .is_user = false,
        };

        UnitManager::create(kernel::realm::REALM_DRIVER, worker_entry, this, &cfg);
    }

    void IntelBlt::worker_entry(void* arg) {
        auto* blt = static_cast<IntelBlt*>(arg);

        while (true) {
            GpuBltRequest* req = blt->blt_queue_.dequeue_blocking();
            if (!req) break;

            switch (req->op) {
                case GpuBltOp::BlitRegion:
                    req->result = blt->execute_blit_region(*req) ? 0 : -1;
                    kernel::memory::free(req->owned_pixels);
                    req->owned_pixels = nullptr;
                    break;
                case GpuBltOp::FillRect:
                    req->result = blt->execute_fill_rect(*req) ? 0 : -1;
                    break;
                default:
                    break;
            }

            if (req->done) req->done->set();
        }
    }

    // =========================================================================
    // Sequence Numbers
    // =========================================================================

    u32 IntelBlt::seqno_next() {
        ++sequence_number_;
        if (sequence_number_ & SEQNO_BIT5_MASK) {
            // Round up past the bit-5 block: 0x20–0x3F → 0x40, 0x60–0x7F → 0x80, etc.
            sequence_number_ = (sequence_number_ & ~u32{0x3F}) + 0x40u;
        }
        return sequence_number_;
    }

    bool IntelBlt::seqno_wait(u32 target_seqno, u32 timeout_us) {
        auto* hwsp = virt_as<u32>(hwsp_cpu_addr_);
        const u32* seqno_ptr = &hwsp[HWSP_SEQNO_OFFSET_DWORDS];

        asm volatile("lfence" ::: "memory");
        if (static_cast<i32>(*seqno_ptr - target_seqno) >= 0) return true;

        const u64 deadline_ms = kernel::time::get_uptime_ms() + (timeout_us + 999) / 1000;

        while (true) {
            if (completion_flag_.consume()) {
                asm volatile("lfence" ::: "memory");
                if (static_cast<i32>(*seqno_ptr - target_seqno) >= 0) return true;
            }

            if (kernel::time::get_uptime_ms() >= deadline_ms) {
                error_count_++;
                return false;
            }

            asm volatile("pause" ::: "memory");
        }
    }

    // =========================================================================
    // Ring Buffer
    // =========================================================================

    void IntelBlt::ring_init() {
        ring_tail_ = 0;

        Log::log_dbc("Ring Buffer: CPU=%p GFX=0x%llx", virt_ptr(ring_cpu_addr_), gfx_raw(ring_gfx_addr_));

        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        for (u32 i = 0; i < ring_size_ / 4; i++) {
            ring[i] = MI_NOOP;
        }

        HWSTAM reg{};
        reg.raw = 0xFFFFFFFF;
        bcs_regs_->hwstam.raw = reg.raw;
    }

    void IntelBlt::ring_write(u32 cmd) {
        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        ring[ring_tail_ / 4] = cmd;
        asm volatile("sfence" ::: "memory");

        ring_tail_ += 4;
        if (ring_tail_ >= ring_size_) ring_tail_ = 0;
    }

    template <typename T>
    void IntelBlt::ring_write_cmd(const T& cmd) {
        static_assert(sizeof(T) % sizeof(u32) == 0, "Command size must be DWORD-aligned");

        const auto* dwords = reinterpret_cast<const u32*>(&cmd);
        const usize count = sizeof(T) / sizeof(u32);
        for (usize i = 0; i < count; i++) {
            ring_write(dwords[i]);
        }
    }

    void IntelBlt::ring_flush() {
        // TAIL must be 8-byte aligned (bits [2:0] = MBZ)
        while (ring_tail_ & 0x7) {
            volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
            ring[ring_tail_ / 4] = MI_NOOP;
            ring_tail_ += 4;
            if (ring_tail_ >= ring_size_) ring_tail_ = 0;
        }

        asm volatile("mfence" ::: "memory");

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(ring_tail_);
        bcs_regs_->ring_tail.raw = tail.raw;
    }

    bool IntelBlt::ring_wait_space(u32 required_bytes, u32 timeout_us) const {
        const u64 start = kernel::time::get_uptime_us();

        while ((kernel::time::get_uptime_us() - start) < timeout_us) {
            const u32 head = bcs_regs_->ring_head.head_offset_bytes();

            const u32 avail = (ring_tail_ >= head) ? (ring_size_ - ring_tail_) + head : head - ring_tail_;

            if (avail >= required_bytes) {
                return true;
            }

            kernel::time::sleep_us(1);
        }

        return false;
    }

    // =========================================================================
    // BCS Command Emission
    // =========================================================================

    void IntelBlt::emit_xy_color_blt(gfx_addr_t dest, u32 pitch, u32 x1, u32 y1, u32 x2, u32 y2, u32 color) {
        XY_COLOR_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_COLOR_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.dword_len = XY_COLOR_BLT_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = ROP_PATCOPY;
        cmd.dw1.dest_pitch = pitch & 0xFFFF;

        cmd.dw2.x1 = x1;
        cmd.dw2.y1 = y1;
        cmd.dw3.x2 = x2;
        cmd.dw3.y2 = y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest) >> 32);
        cmd.solid_color = color;

        ring_write_cmd(cmd);
    }

    void IntelBlt::emit_xy_src_copy_blt(
        gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src,
        u32 src_pitch, u32 src_x1, u32 src_y1
    ) {
        XY_SRC_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_SRC_COPY_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.dword_len = XY_SRC_COPY_BLT_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = SRCCOPY;
        cmd.dw1.dest_pitch = dest_pitch & 0xFFFF;

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;
        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest) >> 32);

        cmd.dw6.src_x1 = src_x1;
        cmd.dw6.src_y1 = src_y1;
        cmd.dw7.src_pitch = src_pitch & 0xFFFF;

        cmd.src_addr_lo = static_cast<u32>(gfx_raw(src) & 0xFFFFFFFF);
        cmd.src_addr_hi = static_cast<u32>(gfx_raw(src) >> 32);

        ring_write_cmd(cmd);
    }

    void IntelBlt::emit_xy_fast_copy_blt(
        gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src,
        u32 src_pitch, u32 src_x1, u32 src_y1
    ) {
        XY_FAST_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_FAST_COPY_BLT;
        cmd.dw0.src_tiling = tile_mode_to_tiling(fb_.tile_mode);
        cmd.dw0.dst_tiling = TILING_LINEAR;
        cmd.dw0.dword_len = XY_FAST_COPY_BLT_LEN;

        cmd.dw1.color_depth = FAST_COLOR_DEPTH_32BPP;
        cmd.dw1.dest_pitch = dest_pitch & 0x7FFF;  // bit[15] MBZ

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;
        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest) >> 32);

        cmd.dw6.src_x1 = src_x1;
        cmd.dw6.src_y1 = src_y1;
        cmd.dw7.src_pitch = src_pitch & 0x7FFF;

        cmd.src_addr_lo = static_cast<u32>(gfx_raw(src) & 0xFFFFFFFF);
        cmd.src_addr_hi = static_cast<u32>(gfx_raw(src) >> 32);

        ring_write_cmd(cmd);
    }

    void IntelBlt::emit_xy_mono_src_copy_blt(
        gfx_addr_t dest, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t mono_src,
        u32 src_bit_pos, bool transparency, u32 bg_color, u32 fg_color
    ) {
        XY_MONO_SRC_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_MONO_SRC_COPY_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.mono_src_bit_pos = src_bit_pos & 0x7;
        cmd.dw0.dword_len = XY_MONO_SRC_COPY_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = SRCCOPY;
        cmd.dw1.transparency = transparency ? 1u : 0u;
        cmd.dw1.dest_pitch = dest_pitch & 0xFFFF;

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;
        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest) >> 32);
        cmd.mono_src_addr_lo = static_cast<u32>(gfx_raw(mono_src) & 0xFFFFFFFF);
        cmd.mono_src_addr_hi = static_cast<u32>(gfx_raw(mono_src) >> 32);
        cmd.bg_color = bg_color;
        cmd.fg_color = fg_color;
        cmd.trailing = 0;

        ring_write_cmd(cmd);
    }

    void IntelBlt::emit_mi_flush(u32 seqno) {
        MI_FLUSH_DW_CMD cmd{};

        cmd.dw0.client = CLIENT_MI;
        cmd.dw0.opcode = OPCODE_MI_FLUSH_DW;
        cmd.dw0.store_index = 1;
        cmd.dw0.post_sync = 1;
        cmd.dw0.dword_len = MI_FLUSH_DW_LEN;

        cmd.dw1.addr_lo = HWSP_SEQNO_OFFSET >> 2;
        cmd.dw2.addr_hi = 0;
        cmd.immediate_data = seqno;

        // IHD-OS-KBL-Vol 2a (p. 993): bit 5 of the write data must be clear.
        if (cmd.immediate_data & (1u << 5)) {
            kernel::SystemManager::system_panic("intel-blt: MI_FLUSH_DW seqno has bit 5 set", DRVERR);
        };

        ring_write_cmd(cmd);

        MI_USER_INTERRUPT_CMD ui{};
        ui.opcode = OPCODE_MI_USER_INTERRUPT;
        ui.client = CLIENT_MI;
        ring_write_cmd(ui);
    }

    // =========================================================================
    // IRQ
    // =========================================================================

    Irqreturn IntelBlt::bcs_irq_handler(IntelBlt* self) {
        auto* master = reinterpret_cast<volatile MASTER_INT_CTL*>(self->mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);
        auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(self->mmio_base_ + GEN8_GT0_INTR_BASE);

        GT0_IIR_REG pending{};
        pending.raw = gt0->iir.raw;

        if (!pending.bits.user_irq) {
            MASTER_INT_CTL arm{};
            arm.master_enable = 1;
            master->raw = arm.raw;
            MMIO_POST_WRITE_PTR(master);
            return IRQ_NONE;
        }

        // Acknowledge (W1C)
        GT0_IIR_REG clear{};
        clear.bits.user_irq = 1;
        gt0->iir.raw = clear.raw;
        MMIO_POST_WRITE(gt0->iir);

        // Re-arm master interrupt delivery
        MASTER_INT_CTL arm{};
        arm.master_enable = 1;
        master->raw = arm.raw;
        MMIO_POST_WRITE_PTR(master);

        self->completion_flag_.set();

        return IRQ_HANDLED;
    }

    // =========================================================================
    // HW Init / Power
    // =========================================================================

    bool IntelBlt::force_wake_enable() const {
        volatile auto* fw_mt = reinterpret_cast<volatile u32*>(mmio_base_ + FORCEWAKE_MT);
        const volatile auto* fw_ack = reinterpret_cast<volatile u32*>(mmio_base_ + FORCEWAKE_ACK);

        *fw_mt = FORCEWAKE_ENABLE;

        u32 timeout = FORCE_WAKE_TIMEOUT;

        while (timeout--) {
            if (*fw_ack & GTT_VALID) {
                return true;
            }

            kernel::time::sleep_us(1);
        }

        Log::log_dbc("BLT: ForceWake timeout");
        return false;
    }

    void IntelBlt::bcs_power_enable() const {
        BCS_SWCTRL swctrl;
        swctrl.raw = bcs_regs_->swctrl.raw;
        swctrl.tile_y_src = 1;
        bcs_regs_->swctrl.raw = swctrl.raw;
        MMIO_POST_WRITE(bcs_regs_->swctrl);
    }

    bool IntelBlt::bcs_reset() const {
        auto* reg = reinterpret_cast<volatile GDRST*>(mmio_base_ + GDRST_MMIO);

        reg->blitter = 1;

        u32 timeout = BCS_RESET_TIMEOUT;

        while (timeout--) {
            if (!(reg->raw & RESET_BCS_BIT)) return true;

            kernel::time::sleep_us(1);
        }

        Log::error("intel-blt: BCS reset timed out, aborting driver initialization");
        return false;
    }

    void IntelBlt::bcs_interrupts_enable() const {
        volatile BCS_IMR_REG& ring_imr = bcs_regs_->imr;
        auto* master = reinterpret_cast<volatile MASTER_INT_CTL*>(mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);
        auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(mmio_base_ + GEN8_GT0_INTR_BASE);

        // BCS local IMR — allow only MI_USER_INTERRUPT
        BCS_IMR_REG bcs_imr{};
        bcs_imr.bits.user_irq = 0;
        bcs_imr.bits.master_error = 1;
        bcs_imr.bits.mi_flush_dw = 1;
        bcs_imr.bits.timeout = 1;
        bcs_imr.bits.ctx_switch = 1;
        bcs_imr.bits.wait_sem = 1;
        ring_imr.raw = bcs_imr.raw;
        MMIO_POST_WRITE(ring_imr);

        // Disable master routing during setup
        master->raw = 0;
        MMIO_POST_WRITE_PTR(master);

        // GT0 IMR — allow only BCS MI_USER_INTERRUPT
        GT0_IMR_REG gt_imr{};
        gt_imr.raw = 0xFFFFFFFFu;
        gt_imr.bits.user_irq = 0;
        gt0->imr.raw = gt_imr.raw;
        MMIO_POST_WRITE(gt0->imr);

        // Clear stale pending state
        GT0_IIR_REG clear{};
        clear.bits.user_irq = 1;
        gt0->iir.raw = clear.raw;
        MMIO_POST_WRITE(gt0->iir);

        // Enable interrupt generation
        GT0_IER_REG enable{};
        enable.bits.user_irq = 1;
        gt0->ier.raw = enable.raw;
        MMIO_POST_WRITE(gt0->ier);

        // Enable global GT routing
        MASTER_INT_CTL arm{};
        arm.master_enable = 1;
        master->raw = arm.raw;
        MMIO_POST_WRITE_PTR(master);
    }

    void IntelBlt::bcs_error_reporting_init() const {
        volatile EIR_REG& eir = bcs_regs_->eir;
        EIR_REG v{};
        v.error_bits.instruction_error = 0;
        v.error_bits.privilege_violation = 0;
        v.mask = 0xFFFFu;
        eir.raw = v.raw;
        MMIO_POST_WRITE(eir);

        volatile EMR_REG& emr = bcs_regs_->emr;
        EMR_REG emr_val{};
        emr_val.error_mask = 0x00;
        emr_val.reserved = 0xFFFFFFu;
        emr.raw = emr_val.raw;
        MMIO_POST_WRITE(emr);
    }

    bool IntelBlt::bcs_emergency_reset() {
        // disable ring
        volatile RING_BUFFER_CTL& ctl = bcs_regs_->ring_ctl;
        ctl.ring_enable = 0;
        bcs_regs_->ring_ctl.raw = ctl.raw;

        if (!bcs_reset()) return false;

        memset(ring_cpu_addr_, 0, ring_size_);
        ring_init();

        // re-enable ring with fresh config
        RING_BUFFER_CTL new_ctl{};
        new_ctl.set_ring_size_bytes(ring_size_);
        new_ctl.ring_enable = 1;

        bcs_regs_->ring_ctl.raw = new_ctl.raw;

        // reset head/tail
        RING_BUFFER_HEAD head{};
        head.set_head_offset_bytes(0);
        bcs_regs_->ring_head.raw = head.raw;

        RING_BUFFER_TAIL tail{};
        tail.set_tail_offset_bytes(0);
        bcs_regs_->ring_tail.raw = tail.raw;

        ring_tail_ = 0;
        sequence_number_ = 0;

        return true;
    }

    void IntelBlt::gpu_health_check() {
        const u32 head = bcs_regs_->ring_head.head_offset_bytes();
        const u32 tail = bcs_regs_->ring_tail.tail_offset_bytes();
        const u32 ctl = bcs_regs_->ring_ctl.raw;

        if (!(ctl & RING_CTL_ENABLED)) {
            error_count_++;
            if (!bcs_emergency_reset()) {
                Log::error("intel-blt: emergency reset failed - GPU unresponsive");
            }
            return;
        }

        if (head == last_head_ && head != tail) {
            if (++hang_counter_ > 1000) {
                error_count_++;
                if (!bcs_emergency_reset()) {
                    Log::error("intel-blt: emergency reset failed - GPU unresponsive");
                }
                hang_counter_ = 0;
            }
        } else {
            hang_counter_ = 0;
        }
        last_head_ = head;
    }

    // =========================================================================
    // Framebuffer / Scratch
    // =========================================================================

    void IntelBlt::fb_alloc(u32 width, u32 height, TileMode tile_mode) {
        fb_.width = width;
        fb_.height = height;
        fb_.bpp = BYTES_PER_PIXEL;
        fb_.tile_mode = tile_mode;

        if (tile_mode == TileMode::X) {
            fb_.pitch = ((width * fb_.bpp + 511) / 512) * 512;
        } else {
            fb_.pitch = ((width * fb_.bpp + 63) / 64) * 64;
        }

        const usize total_size = static_cast<usize>(fb_.pitch) * height;
        const usize num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

        Log::log_dbc(
            "intel-blt: FB %dx%d pitch=%d size=%zu pages=%zu tiling=%u",
            width,
            height,
            fb_.pitch,
            total_size,
            num_pages,
            static_cast<u32>(tile_mode)
        );

        auto alloc = ggtt_alloc_persistent(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        fb_.gfx_addr = alloc.gfx_addr;
        fb_.cpu_addr = alloc.cpu_addr;

        memset(fb_.cpu_addr, 0, total_size);
    }

    void IntelBlt::scratch_init() {
        const usize row_bytes = static_cast<usize>(fb_.width) * BYTES_PER_PIXEL;
        const usize pitch = ((row_bytes + 63) / 64) * 64;
        const usize buf_size = pitch * fb_.height;
        const u32 num_pages = static_cast<u32>((buf_size + PAGE_SIZE - 1) / PAGE_SIZE);

        const GgttAllocation alloc = ggtt_alloc_persistent(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-blt: failed to alloc scratch buffer (%u pages)", num_pages);
            return;
        }

        memset(virt_ptr(alloc.cpu_addr), 0, num_pages * PAGE_SIZE);

        scratch_.alloc = alloc;
        scratch_.num_pages = num_pages;
        scratch_.max_w = fb_.width;
        scratch_.max_h = fb_.height;
        scratch_.pitch = static_cast<u32>(pitch);
        scratch_.valid = true;

        Log::info(
            "intel-blt: scratch %u pages (%lu MB) for %ux%u @ pitch=%u",
            num_pages,
            (num_pages * PAGE_SIZE) >> 20,
            fb_.width,
            fb_.height,
            scratch_.pitch
        );
    }

    // https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02c-commandreference-registers-part2_0.pdf
    // (p. 604)
    void IntelBlt::fb_set_display() const {
        auto* plane = reinterpret_cast<volatile u32*>(mmio_base_);

        // Disable plane before reconfiguring
        plane[PLANE_CTL_1_A / 4] &= ~PLANE_CTL_ENABLE;
        kernel::time::sleep_us(1);

        u32 stride_value = 0;
        u32 plane_ctl = 0;

        switch (fb_.tile_mode) {
            case TileMode::Linear:
                stride_value = fb_.pitch / 64;
                plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888;
                break;
            case TileMode::X:
                stride_value = fb_.pitch / 512;
                plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888 | PLANE_CTL_TILE_X;
                break;
            case TileMode::Y:
                stride_value = fb_.pitch / 128;
                plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888 | PLANE_CTL_TILE_Y;
                break;
            default:
                return;
        }

        plane[PLANE_STRIDE_1_A / 4] = stride_value & 0x3FF;
        plane[PLANE_SIZE_1_A / 4] = ((fb_.height - 1) << 16) | (fb_.width - 1);
        plane[PLANE_POS_1_A / 4] = 0;
        plane[PLANE_OFFSET_1_A / 4] = 0;
        plane[PLANE_CTL_1_A / 4] = plane_ctl;
        plane[PLANE_SURF_1_A / 4] = static_cast<u32>(gfx_raw(fb_.gfx_addr));
        plane[PLANE_CTL_1_A / 4] = plane_ctl | PLANE_CTL_ENABLE;

        asm volatile("mfence" ::: "memory");

        Log::log_dbc(
            "intel-blt: display surface addr=0x%llx %dx%d pitch=%d",
            gfx_raw(fb_.gfx_addr),
            fb_.width,
            fb_.height,
            fb_.pitch
        );
    }

    // =========================================================================
    // Validation / Health
    // =========================================================================

    bool IntelBlt::validate_rect(const BltRect& rect) const {
        if (virt_null(fb_.cpu_addr)) return false;
        if (rect.width == 0 || rect.height == 0) return false;
        if (rect.width > 8192 || rect.height > 4096) return false;
        if (rect.x + rect.width > fb_.width) return false;
        if (rect.y + rect.height > fb_.height) return false;
        if (gfx_raw(fb_.gfx_addr) & 0x3F) return false;
        if (fb_.pitch & 0x3F) return false;
        return true;
    }

    // =========================================================================
    // Public API — IRenderDriver
    // =========================================================================

    bool IntelBlt::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
        auto* req = new GpuBltRequest();
        req->op = GpuBltOp::FillRect;
        req->dst_x = px;
        req->dst_y = py;
        req->w = w;
        req->h = h;
        req->color = colour;
        req->done = nullptr;
        blt_queue_.submit(req);
        return true;
    }

    bool IntelBlt::blit_buffer(const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y) {
        if (!pixels) return false;
        if (dst_x >= fb_.width || dst_y >= fb_.height) return false;

        const u32 max_w = (dst_x + buffer_width > fb_.width) ? fb_.width - dst_x : buffer_width;
        const u32 max_h = (dst_y + buffer_height > fb_.height) ? fb_.height - dst_y : buffer_height;

        const usize width_bytes = static_cast<usize>(buffer_width) * BYTES_PER_PIXEL;
        const usize src_pitch = ((width_bytes + 63) / 64) * 64;
        const usize buffer_size = src_pitch * max_h;
        const usize num_pages = (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

        const auto temp = ggtt_alloc_transient(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(temp.cpu_addr)) return false;

        const auto* src = static_cast<const u8*>(pixels);
        u8* dst = virt_as<u8>(temp.cpu_addr);
        for (u32 y = 0; y < max_h; y++) {
            memcpy(dst + y * src_pitch, src + y * width_bytes, width_bytes);
        }

        gpu_health_check();
        if (!ring_wait_space(RING_SPACE_FOR_BLIT, 1'000'000)) {
            ggtt_free_transient(temp, num_pages);
            return false;
        }

        emit_xy_src_copy_blt(
            fb_.gfx_addr, fb_.pitch, dst_x, dst_y, dst_x + max_w, dst_y + max_h, temp.gfx_addr, src_pitch, 0, 0
        );

        const u32 target_seqno = seqno_next();
        emit_mi_flush(target_seqno);
        ring_flush();

        const bool success = seqno_wait(target_seqno, 500'000);
        ggtt_free_transient(temp, num_pages);
        return success;
    }

    bool IntelBlt::blit_region(
        const u32* pixels, const u32 src_stride, const u32 src_x, const u32 src_y, const u32 w, const u32 h,
        const u32 dst_x, const u32 dst_y
    ) {
        if (!pixels) return false;
        if (dst_x >= fb_.width || dst_y >= fb_.height) return false;

        const u32 max_w = (dst_x + w > fb_.width) ? fb_.width - dst_x : w;
        const u32 max_h = (dst_y + h > fb_.height) ? fb_.height - dst_y : h;
        if (max_w == 0 || max_h == 0) return true;

        const usize row_bytes = static_cast<usize>(max_w) * BYTES_PER_PIXEL;
        auto* copy = static_cast<u8*>(kernel::memory::malloc(row_bytes * max_h));
        if (!copy) return false;

        const usize src_stride_bytes = static_cast<usize>(src_stride) * BYTES_PER_PIXEL;
        const u8* src_base = reinterpret_cast<const u8*>(pixels) + static_cast<usize>(src_y) * src_stride_bytes +
                             static_cast<usize>(src_x) * BYTES_PER_PIXEL;

        for (u32 y = 0; y < max_h; y++) {
            memcpy(copy + y * row_bytes, src_base + y * src_stride_bytes, row_bytes);
        }

        auto* req = new GpuBltRequest();
        req->op = GpuBltOp::BlitRegion;
        req->owned_pixels = reinterpret_cast<u32*>(copy);
        req->src_stride = max_w;
        req->src_x = 0;
        req->src_y = 0;
        req->dst_x = dst_x;
        req->dst_y = dst_y;
        req->w = max_w;
        req->h = max_h;
        req->done = nullptr;

        blt_queue_.submit(req);
        return true;
    }

    // =========================================================================
    // Worker Execute Paths
    // =========================================================================

    bool IntelBlt::execute_blit_region(const GpuBltRequest& req) {
        if (req.w == 0 || req.h == 0) return true;

        if (!scratch_.valid) {
            Log::error("intel-blt: scratch buffer not initialized");
            return false;
        }

        const usize row_bytes = static_cast<usize>(req.w) * BYTES_PER_PIXEL;
        const u8* src = reinterpret_cast<const u8*>(req.owned_pixels);
        u8* dst = virt_as<u8>(scratch_.alloc.cpu_addr);

        for (u32 y = 0; y < req.h; y++) {
            memcpy(dst + static_cast<usize>(y) * scratch_.pitch, src + static_cast<usize>(y) * row_bytes, row_bytes);
        }
        asm volatile("mfence" ::: "memory");

        gpu_health_check();
        if (!ring_wait_space(RING_SPACE_FOR_BLIT, 1'000'000)) return false;

        emit_xy_src_copy_blt(
            fb_.gfx_addr,
            fb_.pitch,
            req.dst_x,
            req.dst_y,
            req.dst_x + req.w,
            req.dst_y + req.h,
            scratch_.alloc.gfx_addr,
            scratch_.pitch,
            0,
            0
        );

        const u32 target_seqno = seqno_next();
        emit_mi_flush(target_seqno);
        ring_flush();

        return seqno_wait(target_seqno, 500'000);
    }

    bool IntelBlt::execute_fill_rect(const GpuBltRequest& req) {
        const BltRect rect{.x = req.dst_x, .y = req.dst_y, .width = req.w, .height = req.h};
        if (!validate_rect(rect)) return false;

        gpu_health_check();
        if (!ring_wait_space(RING_SPACE_FOR_FILL, 1'000'000)) return false;

        emit_xy_color_blt(
            fb_.gfx_addr, fb_.pitch, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height, req.color
        );

        const u32 target_seqno = seqno_next();
        emit_mi_flush(target_seqno);
        ring_flush();

        return seqno_wait(target_seqno, 5'000'000);
    }

    // =========================================================================
    // IRenderDriver Accessors
    // =========================================================================

    u32 IntelBlt::screen_width_px() const {
        return fb_.width;
    }
    u32 IntelBlt::screen_height_px() const {
        return fb_.height;
    }
    u32 IntelBlt::bytes_per_scanline() const {
        return fb_.pitch;
    }

    bool IntelBlt::get_vendor(char* out, const usize len) {
        strncpy(out, pci::get_vendor_name(igp_cfg_->vendor_id), len);
        out[len - 1] = '\0';
        return true;
    }

    bool IntelBlt::get_model(char* out, const usize len) {
        strncpy(out, pci::get_device_name(igp_cfg_->vendor_id, igp_cfg_->device_id), len);
        out[len - 1] = '\0';
        return true;
    }

    // =========================================================================
    // Utility
    // =========================================================================

    u32 IntelBlt::tile_mode_to_tiling(TileMode mode) {
        switch (mode) {
            case TileMode::X:
                return TILING_X;
            case TileMode::Y:
                return TILING_Y;
            default:
                return TILING_LINEAR;
        }
    }

}  // namespace blt