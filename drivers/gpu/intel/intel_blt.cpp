/**
 * @file intel_blt.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 16.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */

#include "intel_blt.h"

#include <filesystem/devfs.h>
#include <klib/string.h>
#include <pci/msix.h>
#include <pci/pci.h>
#include <pci/pci_device.h>
#include <pci/pci_host_bridge.h>
#include <vespera/devices/device_manager.h>
#include <vespera/graphics/colors.h>
#include <vespera/interrupts.h>
#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_types.h>
#include <vespera/time.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "blt_commands.h"
#include "error_regs.h"
#include "gt_interrupt_regs.h"
#include "interrupt_regs.h"
#include "pci_config_regs.h"

namespace blt {
    IntelBlt::IntelBlt(const pci::pci_device& igpu_dev)
        : igp_cfg_(reinterpret_cast<volatile INTEL_IGP_PCI_CONFIG*>(igpu_dev.header))
        , ring_size_(RING_BUFFER_SIZE)
        , sequence_number_(0) {
        const phys_addr_t bar0 =
            make_phys(static_cast<u64>(igp_cfg_->gttmmadr_hi) << 32 | (igp_cfg_->gttmmadr_lo & GTTMMADR_ADDR_MASK));

        kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE, (1ULL << CacheDisabled));

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
        bcs_regs_ = reinterpret_cast<volatile u32*>(mmio_base_ + BCS_RING_BASE);

        enable_force_wake();
        init_gtt(igpu_dev);
        enable_bcs_power();
        reset_bcs();

        const u8 vector = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vector, reinterpret_cast<irq_handler_t>(bcs_interrupt_handler), this);
        if (!pci::try_enable_msi_or_msix(reinterpret_cast<volatile pci::PCI_HEADER0*>(igp_cfg_), vector, 1)) {
            Log::log_dbc("BLT: MSI enable failed");
        }

        // completion_sem_.init(1, 0);
        completion_flag_.init(false);

        enable_bcs_interrupts();
        init_bcs_error_reporting();

        auto hwsp = alloc_and_map_to_ggtt(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        hwsp_cpu_addr_ = hwsp.cpu_addr;
        hwsp_gfx_addr_ = hwsp.gfx_addr;
        memset(hwsp_cpu_addr_, 0, PAGE_SIZE);
        bcs_regs_[BCS_HWSP / 4] = static_cast<u32>(gfx_raw(hwsp_gfx_addr_));

        const u32 ring_pages = ring_size_ / PAGE_SIZE;
        auto [cpu_addr, gfx_addr] = alloc_and_map_to_ggtt(ring_pages);

        ring_cpu_addr_ = cpu_addr;
        ring_gfx_addr_ = gfx_addr;
        memset(ring_cpu_addr_, 0, ring_size_);
        setup_ring_buffer();

        bcs_regs_[BCS_RING_START / 4] = static_cast<u32>(gfx_raw(ring_gfx_addr_) & GTT_PHYS_ADDR_MASK);

        const u32 ring_ctl = ((ring_size_ - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
        bcs_regs_[BCS_RING_CTL / 4] = ring_ctl;

        bcs_regs_[BCS_RING_HEAD / 4] = 0;
        bcs_regs_[BCS_RING_TAIL / 4] = 0;

        const u32 head = bcs_regs_[BCS_RING_HEAD / 4];
        const u32 tail = bcs_regs_[BCS_RING_TAIL / 4];
        const u32 ctl = bcs_regs_[BCS_RING_CTL / 4];

        init_text_buffer(system_font, target_framebuffer->width);

        if ((ctl & RING_CTL_ENABLED) && (head == tail)) {
            Log::log_dbc("BCS is READY!");
        } else {
            Log::log_dbc("BCS initialization failed!");
            return;
        }

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
    }

    void IntelBlt::start_blt_worker(const u8 cpu_id) {
        blt_queue_.init();

        const UnitConfig cfg = {
            .name = "intel-blt",
            .cpu_id = cpu_id,
            .priority = 4,
            .stack_size = 0x10000,
            .is_user = false,
        };

        UnitManager::create(kernel::realm::REALM_DRIVER, blt_worker_entry, this, &cfg);
    }

    void IntelBlt::blt_worker_entry(void* arg) {
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
            }

            if (req->done) req->done->set();
        }
    }

    u32 IntelBlt::next_seqno() {
        ++sequence_number_;
        if (sequence_number_ & SEQNO_BIT5_MASK) {
            // Round up to the next block of 64 where bit 5 is clear.
            // Example: 0x20–0x3F → 0x40,  0x60–0x7F → 0x80
            sequence_number_ = (sequence_number_ & ~u32{0x3F}) + 0x40u;
        }
        return sequence_number_;
    }

    void IntelBlt::init_scratch_buffer() {
        const usize row_bytes = static_cast<usize>(fb_.width) * BYTES_PER_PIXEL;
        const usize pitch = ((row_bytes + 63) / 64) * 64;
        const usize buf_size = pitch * fb_.height;
        const u32 num_pages = static_cast<u32>((buf_size + PAGE_SIZE - 1) / PAGE_SIZE);

        const GgttAllocation alloc = alloc_and_map_to_ggtt(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
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
            "intel-blt: scratch buffer: %u pages (%lu MB) for %ux%u @ pitch=%u",
            num_pages,
            (num_pages * PAGE_SIZE) >> 20,
            fb_.width,
            fb_.height,
            scratch_.pitch
        );
    }

    void IntelBlt::init_bcs_error_reporting() const {
        auto* eir = reinterpret_cast<volatile EIR_REG*>(mmio_base_ + BCS_EIR);

        EIR_REG v{};
        v.error_bits.instruction_error = 0;
        v.error_bits.privilege_violation = 0;

        v.mask = 0xFFFFu;

        eir->raw = v.raw;

        MMIO_POST_WRITE(eir);

        auto* emr = reinterpret_cast<volatile EMR_REG*>(mmio_base_ + BCS_EMR);

        EMR_REG emr_reg{};
        emr_reg.error_mask = 0x00;
        emr_reg.reserved = 0xFFFFFFu;

        emr->raw = emr_reg.raw;

        MMIO_POST_WRITE(emr);
    }

    void IntelBlt::enable_bcs_interrupts() const {
        auto* ring_imr = reinterpret_cast<volatile BCS_IMR_REG*>(mmio_base_ + BCS_IMR);

        auto* master = reinterpret_cast<volatile MASTER_INT_CTL*>(mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);

        auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(mmio_base_ + GEN8_GT0_INTR_BASE);

        //
        // BCS local interrupt mask
        //
        // Allow ONLY MI_USER_INTERRUPT through the BCS engine IMR.
        BCS_IMR_REG bcs_imr{};
        bcs_imr.bits.user_irq = 0;  // 0 = allowed
        bcs_imr.bits.master_error = 1;
        bcs_imr.bits.mi_flush_dw = 1;
        bcs_imr.bits.timeout = 1;
        bcs_imr.bits.ctx_switch = 1;
        bcs_imr.bits.wait_sem = 1;

        ring_imr->raw = bcs_imr.raw;
        MMIO_POST_WRITE(ring_imr);

        //
        // Disable master interrupt routing during setup
        //
        MASTER_INT_CTL master_disable{};
        master_disable.raw = 0;

        master->raw = master_disable.raw;
        MMIO_POST_WRITE(master);

        //
        // GT0 IMR
        //
        GT0_IMR_REG gt_imr{};
        gt_imr.raw = 0xFFFFFFFFu;

        // Allow ONLY BCS MI_USER_INTERRUPT
        gt_imr.bits.user_irq = 0;

        gt0->imr.raw = gt_imr.raw;
        MMIO_POST_WRITE(&gt0->imr);

        //
        // Clear stale pending interrupt state
        //
        GT0_IIR_REG clear{};
        clear.bits.user_irq = 1;

        gt0->iir.raw = clear.raw;
        MMIO_POST_WRITE(&gt0->iir);

        //
        // Enable interrupt generation
        //
        GT0_IER_REG enable{};
        enable.bits.user_irq = 1;

        gt0->ier.raw = enable.raw;
        MMIO_POST_WRITE(&gt0->ier);

        //
        // Enable global GT interrupt routing
        //
        MASTER_INT_CTL arm{};
        arm.master_enable = 1;

        master->raw = arm.raw;
        MMIO_POST_WRITE(master);
    }

    void IntelBlt::start_device(u32 screen_width, u32 screen_height) {
        alloc_framebuffer(screen_width, screen_height, TileMode::Linear);
        init_scratch_buffer();
        start_blt_worker(1);
        set_display_framebuffer();
    }

    u32 IntelBlt::tile_mode_to_blt_flag(TileMode mode) {
        switch (mode) {
            case TileMode::X:
                return TILING_X;
            case TileMode::Y:
                return TILING_Y;
            default:
                return TILING_LINEAR;
        }
    }

    void IntelBlt::init_text_buffer(const PsfFont* font, const u32 screen_width) {
        if (!font) {
            // Log::log_dbc("Invalid font for text buffer initialization");
            return;
        }

        const u32 max_chars = screen_width / font->width;
        const u32 max_width = max_chars * font->width;
        const u32 max_height = font->height;

        usize stride = ((max_width + 7) / 8);
        stride = ((stride + 1) / 2) * 2;
        const usize total_size = stride * max_height;
        const usize num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

        Log::log_dbc(
            "Allocating text buffer: font=%ux%u, screen=%u, max_chars=%u, buffer=%ux%u, stride=%lu, size=%zu",
            font->width,
            font->height,
            screen_width,
            max_chars,
            max_width,
            max_height,
            stride,
            total_size
        );

        auto allocation = alloc_and_map_to_ggtt(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);

        text_buffer_.cpu_addr = allocation.cpu_addr;
        text_buffer_.gfx_addr = allocation.gfx_addr;
        text_buffer_.width = max_width;
        text_buffer_.height = max_height;
        text_buffer_.total_size = total_size;

        memset(text_buffer_.cpu_addr, 0, total_size);
    }

    template <typename T>
    void IntelBlt::write_command_struct(const T& cmd) {
        const auto* dwords = reinterpret_cast<const u32*>(&cmd);
        const usize count = sizeof(T) / sizeof(u32);
        for (usize i = 0; i < count; i++) {
            write_command(dwords[i]);
        }
    }

    void IntelBlt::write_command(u32 cmd) {
        u32 head = bcs_regs_[BCS_RING_HEAD / 4];
        u32 available_space = 0;

        if (ring_tail_ >= head) {
            available_space = (ring_size_ - ring_tail_) + head;
        } else {
            available_space = head - ring_tail_;
        }

        // 4 bytes + 64 bytes margin
        if (available_space < 68) {
            // // Log::log_dbc("Ring buffer full! HEAD=0x%x TAIL=0x%x", head, ring_tail_);
            error_count_++;
            if (!wait_for_ring_space(68, 100000)) {
                // 100ms timeout
                // // Log::log_dbc("Ring buffer deadlock!");
                error_count_++;
                return;
            }
        }

        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        ring[ring_tail_ / 4] = cmd;

        asm volatile("sfence" ::: "memory");

        ring_tail_ += 4;
        if (ring_tail_ >= ring_size_) {
            ring_tail_ = 0;
        }
    }

    bool IntelBlt::wait_for_ring_space(u32 required_bytes, u32 timeout_us) const {
        for (u32 i = 0; i < timeout_us; i++) {
            const u32 head = bcs_regs_[BCS_RING_HEAD / 4];
            u32 available = 0;

            if (ring_tail_ >= head) {
                available = (ring_size_ - ring_tail_) + head;
            } else {
                available = head - ring_tail_;
            }

            if (available >= required_bytes) {
                return true;
            }

            // Micro-delay (1µs)
            for (int j = 0; j < IDLE_CHECK_DELAY; j++);
        }
        return false;
    }

    bool IntelBlt::validate_blt_params(const BltRect& rect) const {
        if (virt_null(fb_.cpu_addr)) {
            // Log::log_dbc("Invalid framebuffer");
            return false;
        }

        // Check dimensions
        if (rect.width == 0 || rect.height == 0) {
            // Log::log_dbc("Invalid rect dimensions: %dx%d", rect.width, rect.height);
            return false;
        }

        if (rect.width > 8192 || rect.height > 4096) {
            // Log::log_dbc("Rect too large: %dx%d (max 8192x4096)", rect.width, rect.height);
            return false;
        }

        // Check bounds
        if (rect.x + rect.width > fb_.width || rect.y + rect.height > fb_.height) {
            /* Log::log_dbc(
                "Rect out of bounds: (%d,%d)-(%d,%d) in %dx%d FB",
                rect.x,
                rect.y,
                rect.x + rect.width,
                rect.y + rect.height,
                fb_.width,
                fb_.height
            );*/
            return false;
        }

        // Check alignment
        if (gfx_raw(fb_.gfx_addr) & 0x3F) {
            // 64-byte alignment
            // Log::log_dbc("Framebuffer not aligned: 0x%llx", gfx_raw(fb_.gfx_addr));
            return false;
        }

        // Check pitch
        if (fb_.pitch & 0x3F) {
            // 64-byte alignment
            // Log::log_dbc("Pitch not aligned: %d", fb_.pitch);
            return false;
        }

        return true;
    }

    void IntelBlt::xy_color_blt(gfx_addr_t dest_addr, u32 dest_pitch, u32 x1, u32 y1, u32 x2, u32 y2, u32 color) {
        XY_COLOR_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_COLOR_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.dword_len = XY_COLOR_BLT_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = ROP_PATCOPY;
        cmd.dw1.dest_pitch = dest_pitch & COORD_MASK;

        cmd.dw2.x1 = x1;
        cmd.dw2.y1 = y1;

        cmd.dw3.x2 = x2;
        cmd.dw3.y2 = y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest_addr) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest_addr) >> 32);

        cmd.solid_color = color;

        write_command_struct(cmd);
    }

    void IntelBlt::emergency_reset_bcs() {
        // Log::log_dbc("Emergency BCS reset initiated!");

        bcs_regs_[BCS_RING_CTL / 4] &= ~RING_CTL_ENABLED;

        reset_bcs();

        memset(ring_cpu_addr_, 0, ring_size_);
        setup_ring_buffer();

        const u32 ring_ctl = ((ring_size_ - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
        bcs_regs_[BCS_RING_CTL / 4] = ring_ctl;
        bcs_regs_[BCS_RING_HEAD / 4] = 0;
        bcs_regs_[BCS_RING_TAIL / 4] = 0;
        ring_tail_ = 0;

        sequence_number_ = 0;

        // Log::log_dbc("Emergency reset complete");
    }

    void IntelBlt::check_gpu_health() {
        const u32 head = bcs_regs_[BCS_RING_HEAD / 4];
        const u32 tail = bcs_regs_[BCS_RING_TAIL / 4];

        // Check if ring is still enabled
        if (const u32 ctl = bcs_regs_[BCS_RING_CTL / 4]; !(ctl & RING_CTL_ENABLED)) {
            // // Log::log_dbc("BCS ring disabled unexpectedly!");
            error_count_++;
            emergency_reset_bcs();
            return;
        }

        // Check for hung command
        static u32 last_head = 0;
        static u32 hang_counter = 0;

        if (head == last_head && head != tail) {
            hang_counter++;
            if (hang_counter > 1000) {
                //  // Log::log_dbc("GPU hang detected! HEAD stuck at 0x%x", head);
                error_count_++;
                emergency_reset_bcs();
                hang_counter = 0;
            }
        } else {
            hang_counter = 0;
        }
        last_head = head;
    }

    void IntelBlt::alloc_framebuffer(u32 width, u32 height, TileMode tile_mode) {
        fb_.width = width;
        fb_.height = height;
        fb_.bpp = 4;  // 32-bit ARGB

        if (tile_mode == TileMode::X) {
            fb_.pitch = ((width * fb_.bpp + 511) / 512) * 512;
        } else {
            fb_.pitch = ((width * fb_.bpp + 63) / 64) * 64;
        }

        fb_.pitch = ((width * fb_.bpp + 63) / 64) * 64;

        const usize total_size = static_cast<usize>(fb_.pitch) * height;
        const usize num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

        Log::log_dbc(
            "Allocating framebuffer: %dx%d, pitch=%d, size=%zu bytes, pages=%zu, tile_mode=%u",
            width,
            height,
            fb_.pitch,
            total_size,
            num_pages,
            static_cast<u32>(tile_mode)
        );

        auto allocation = alloc_and_map_to_ggtt(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);

        fb_.gfx_addr = allocation.gfx_addr;
        fb_.cpu_addr = allocation.cpu_addr;

        memset(fb_.cpu_addr, 0, total_size);
    }

    void IntelBlt::mi_flush(u32 seqno) {
        MI_FLUSH_DW_CMD cmd{};

        cmd.dw0.client = CLIENT_MI;
        cmd.dw0.opcode = OPCODE_MI_FLUSH_DW;
        cmd.dw0.store_index = 1;
        cmd.dw0.post_sync = 1;
        cmd.dw0.dword_len = MI_FLUSH_DW_LEN;

        cmd.dw1.addr_lo = HWSP_SEQNO_OFFSET >> 2;
        cmd.dw2.addr_hi = 0;
        cmd.immediate_data = seqno;

        // IHD-OS-KBL-Vol 2a (Page 993)
        if (cmd.immediate_data & (1u << 5)) {
            return;  // ERR_INVALID_ADDR; TODO add error codes or something like this
        }

        write_command_struct(cmd);

        MI_USER_INTERRUPT_CMD user_int_cmd{};
        user_int_cmd.opcode = OPCODE_MI_USER_INTERRUPT;
        user_int_cmd.client = CLIENT_MI;
        write_command_struct(user_int_cmd);
    }

    Irqreturn IntelBlt::bcs_interrupt_handler(IntelBlt* self) {
        auto* master = reinterpret_cast<volatile MASTER_INT_CTL*>(self->mmio_base_ + GEN8_MASTER_INT_CTL_OFFSET);

        auto* gt0 = reinterpret_cast<volatile GT_INTR_REGS*>(self->mmio_base_ + GEN8_GT0_INTR_BASE);

        //
        // Read pending GT0 interrupt sources
        //
        GT0_IIR_REG pending{};
        pending.raw = gt0->iir.raw;

        //
        // Not our interrupt.
        //
        if (!pending.bits.user_irq) {
            MASTER_INT_CTL arm{};
            arm.master_enable = 1;

            master->raw = arm.raw;
            MMIO_POST_WRITE(master);
            return IRQ_NONE;
        }

        //
        // Acknowledge interrupt (W1C)
        //
        GT0_IIR_REG clear{};
        clear.bits.user_irq = 1;

        gt0->iir.raw = clear.raw;
        MMIO_POST_WRITE(&gt0->iir);

        //
        // Re-arm master interrupt delivery
        //
        // Pending bits are RO/sticky status indicators.
        // Only bit31 is writable.
        //
        MASTER_INT_CTL arm{};
        arm.master_enable = 1;

        master->raw = arm.raw;
        MMIO_POST_WRITE(master);

        //
        // Signal completion
        //
        // self->completion_sem_.signal();
        self->completion_flag_.set();

        return IRQ_HANDLED;
    }

    bool IntelBlt::wait_for_sequence(u32 target_seqno, u32 timeout_us) {
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

    void IntelBlt::flush_commands() {
        // The TAIL ring must be 8-byte aligned (bits [2:0] = MBZ)
        while (ring_tail_ & 0x7) {
            volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
            ring[ring_tail_ / 4] = MI_NOOP;
            ring_tail_ += 4;
            if (ring_tail_ >= ring_size_) ring_tail_ = 0;
        }

        asm volatile("mfence" ::: "memory");
        bcs_regs_[BCS_RING_TAIL / 4] = ring_tail_;
        /// Log::log_dbc("flush_commands: ring_tail_=0x%x (aligned)", ring_tail_);
    }

    void IntelBlt::setup_ring_buffer() {
        ring_tail_ = 0;

        Log::log_dbc("Ring Buffer: CPU=%p GFX=0x%llx", virt_ptr(ring_cpu_addr_), gfx_raw(ring_gfx_addr_));

        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        for (u32 i = 0; i < ring_size_ / 4; i++) {
            ring[i] = MI_NOOP;
        }

        auto* hwstam = reinterpret_cast<volatile u32*>(mmio_base_ + BCS_HWSTAM);
        *hwstam = ~0u;
        asm volatile("" ::: "memory");
        (void)*hwstam;

        Log::log_dbc("Ring Buffer cleared with NOOPs");
    }

    void IntelBlt::enable_force_wake() const {
        volatile auto* forcewake_mt = reinterpret_cast<volatile u32*>(mmio_base_ + FORCEWAKE_MT);
        const volatile auto* forcewake_ack = reinterpret_cast<volatile u32*>(mmio_base_ + FORCEWAKE_ACK);

        *forcewake_mt = FORCEWAKE_ENABLE;

        // Wait for acknowledgment
        int timeout = FORCE_WAKE_TIMEOUT;
        while (timeout-- > 0) {
            if (*forcewake_ack & GTT_VALID) {
                Log::log_dbc("Force Wake ACK received");
                break;
            }
            for (int i = 0; i < IDLE_CHECK_DELAY; i++);
        }

        if (timeout <= 0) {
            Log::log_dbc("Force Wake timeout!");
        }
    }

    void IntelBlt::enable_bcs_power() const {
        const auto bcs_swctrl = reinterpret_cast<volatile u32*>(mmio_base_ + BCS_SWCTRL);
        *bcs_swctrl |= BCS_SWCTRL_WAKEUP;
        Log::log_dbc("BCS Power enabled");
    }

    void IntelBlt::reset_bcs() const {
        const auto reset_ctl = reinterpret_cast<volatile u32*>(mmio_base_ + RESET_CTL);

        // Request BCS reset
        *reset_ctl |= RESET_BCS_BIT;
        for (int i = 0; i < RESET_DELAY; i++) {
        }

        // Release reset
        *reset_ctl &= ~RESET_BCS_BIT;
        for (int i = 0; i < RESET_DELAY; i++) {
        }

        Log::log_dbc("BCS Reset completed");
    }

    [[nodiscard]] static GmadrInfo read_gmadr(const volatile INTEL_IGP_PCI_CONFIG* cfg) {
        GMADR_0_2_0_PCI gmadr;
        gmadr.dwords.lo = cfg->gmadr_lo;
        gmadr.dwords.hi = cfg->gmadr_hi;

        if (gmadr.mem_io_space != 0 || gmadr.mem_type != 2 || gmadr.prefetchable != 1) {
            Log::error(
                "intel-blt: GMADR BAR2 flags unexpected "
                "(mem_io=%u type=%u prefetch=%u) - not a valid 64-bit memory BAR",
                gmadr.mem_io_space,
                gmadr.mem_type,
                gmadr.prefetchable
            );
            return {.base = 0, .size = 0, .valid = false};
        }

        const u64 base = gmadr.base_address();
        if (base == 0) {
            Log::error("intel-blt: GMADR base is 0 - firmware did not configure BAR2");
            return {.base = 0, .size = 0, .valid = false};
        }

        MSAC_0_2_0_PCI msac;
        msac.raw = cfg->msac.raw;

        const u64 size = msac.aperture_size_bytes();

        Log::debug("intel-blt: GMADR base=0x%016llx  MSAC raw=0x%02x  aperture=%llu MB", base, msac.raw, size >> 20);

        return {.base = base, .size = size, .valid = true};
    }

    void IntelBlt::init_gtt(const pci::pci_device& igpu_dev) {
        const volatile pci::INTEL_HB_PCI_CONFIG* hb = pci::get_host_bridge(igpu_dev.id.domain);
        if (!hb) {
            Log::error("intel-blt: host bridge not found for domain %u - cannot read GGC", igpu_dev.id.domain);
            return;
        }

        if (hb->header.header.vendor_id != 0x8086) {
            Log::error("intel-blt: unexpected host bridge vendor 0x%04x", hb->header.header.vendor_id);
            return;
        }

        GGC_0_0_0_PCI ggc;
        ggc.raw = hb->ggc;

        Log::debug(
            "intel-blt: GGC raw=0x%04x  gms=0x%02x  ggms=0x%x  lock=%u  ivd=%u",
            ggc.raw,
            ggc.gms,
            ggc.ggms,
            ggc.lock,
            ggc.ivd
        );

        const u64 dsm_bytes = ggc.dsm_size_bytes();
        const u64 gsm_bytes = ggc.gsm_size_bytes();
        const u32 gsm_entries = ggc.ggtt_entry_count();

        Log::info(
            "intel-blt: DSM=%llu MB  GSM=%llu MB  GTT capacity=%u entries",
            dsm_bytes >> 20,
            gsm_bytes >> 20,
            gsm_entries
        );

        if (gsm_entries == 0) {
            Log::error("intel-blt: GGMS=0, BIOS preallocated no GTT stolen memory - aborting");
            return;
        }

        const GmadrInfo gmadr = read_gmadr(igp_cfg_);

        if (!gmadr.valid) {
            Log::error("intel-blt: could not determine aperture from GMADR/MSAC - aborting");
            return;
        }

        Log::info("intel-blt: GMADR base=0x%016llx  aperture=%llu MB", gmadr.base, gmadr.size >> 20);

        // GSM limits the number of physically available PTEs.
        // Aperture limits the number of PTEs that the CPU can see per window.
        // The smaller of the two values applies.
        const u32 aperture_entries = static_cast<u32>(gmadr.size / 4096u);
        const u32 total_entries = (gsm_entries < aperture_entries) ? gsm_entries : aperture_entries;

        Log::debug(
            "intel-blt: GSM entries=%u  aperture entries=%u  → using %u", gsm_entries, aperture_entries, total_entries
        );

        const u32 dsm_pages = static_cast<u32>(ggc.dsm_size_bytes() / PAGE_SIZE);

        // Cross-check via BDSM/BGSM vom Host Bridge
        const u32 bdsm  = hb->bdsm.address();
        const u32 bgsm  = hb->bgsm.address();
        const u32 tolud = hb->tolud.address();

        u32 firmware_reserved = dsm_pages;

        if (bdsm != 0 && tolud != 0 && tolud > bdsm) {
            const u32 dsm_from_bars = (tolud - bdsm) / PAGE_SIZE;

            const u32 gsm_from_bars = (bgsm != 0 && bdsm > bgsm) ? (bdsm - bgsm) / PAGE_SIZE : 0;

            if (dsm_from_bars != dsm_pages) {
                Log::warning(
                    "intel-blt: DSM mismatch - GGC=%u pages, TOLUD-BDSM=%u pages - using larger",
                    dsm_pages,
                    dsm_from_bars
                );
                firmware_reserved = (dsm_from_bars > dsm_pages) ? dsm_from_bars : dsm_pages;
            }

            Log::debug(
                "intel-blt: TOLUD=0x%08x BDSM=0x%08x BGSM=0x%08x  DSM=%u pages  GSM=%u pages",
                tolud,
                bdsm,
                bgsm,
                firmware_reserved,
                gsm_from_bars
            );
        } else {
            Log::warning("intel-blt: BDSM/TOLUD not set - using GGC reservation (%u pages)", firmware_reserved);
        }

        Log::info(
            "intel-blt: firmware reserved=%u entries (%lu MB)", firmware_reserved, (firmware_reserved * PAGE_SIZE) >> 20
        );

        if (total_entries <= firmware_reserved) {
            Log::error(
                "intel-blt: GGTT too small (%u) for firmware reservation (%u)", total_entries, firmware_reserved
            );
            return;
        }

        gtt_entries_ = reinterpret_cast<volatile u64*>(mmio_base_ + GTT_OFFSET);
        ggtt_alloc_.init(total_entries, firmware_reserved);

        Log::info(
            "intel-blt: GGTT ready - total=%u  reserved=%u  usable=%u entries (%lu MB)",
            total_entries,
            firmware_reserved,
            total_entries - firmware_reserved,
            ((total_entries - firmware_reserved) * PAGE_SIZE) >> 20
        );
    }

    void IntelBlt::map_to_ggtt_at(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const {
        for (usize i = 0; i < num_pages; i++) {
            u64 page_phys = phys_raw(phys_add(phys_addr, i * PAGE_SIZE));
            u64 gtt_entry = page_phys & GTT_PHYS_ADDR_MASK;
            gtt_entry |= (static_cast<u64>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
            gtt_entry |= GTT_VALID;
            gtt_entries_[gtt_index + i] = gtt_entry;
            asm volatile("mfence" ::: "memory");
        }
    }

    void IntelBlt::unmap_from_ggtt(u32 gtt_index, usize num_pages) const {
        for (usize i = 0; i < num_pages; i++) {
            gtt_entries_[gtt_index + i] = 0;  // clear GTT_VALID
            asm volatile("mfence" ::: "memory");
        }
    }

    GgttAllocation IntelBlt::alloc_and_map_to_ggtt(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu = phys_to_virt(phys);
        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

        const u32 gtt_index = ggtt_alloc_.alloc_persistent(static_cast<u32>(num_pages));
        if (gtt_index == U32_MAX) {
            Log::log_dbc("alloc_and_map_to_ggtt: persistent zone exhausted");
            error_count_++;
            // Physical pages are intentionally not freed here: persistent
            // allocations failing is a fatal driver initialisation error.
            return {};
        }

        map_to_ggtt_at(gtt_index, phys, num_pages, pat_index);
        return {cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE)};
    }

    GgttAllocation IntelBlt::alloc_and_map_to_ggtt_transient(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu = phys_to_virt(phys);
        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

        const u32 gtt_index = ggtt_alloc_.alloc_transient(static_cast<u32>(num_pages));
        if (gtt_index == U32_MAX) {
            draw_str("alloc_and_map_to_ggtt_transient: transient zone exhausted", 600, 100, 0xffffffff, 0x00000000);
            Log::log_dbc("alloc_and_map_to_ggtt_transient: transient zone exhausted");
            kernel::memory::free_pages_phys(phys, num_pages);
            return {};
        }

        map_to_ggtt_at(gtt_index, phys, num_pages, pat_index);
        return {cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE)};
    }

    void IntelBlt::free_ggtt_transient(const GgttAllocation& alloc, usize num_pages) {
        if (virt_null(alloc.cpu_addr)) {
            return;
        }

        const u32 gtt_index = static_cast<u32>(gfx_raw(alloc.gfx_addr) / PAGE_SIZE);

        // Invalidate GTT entries before returning the index to the allocator so
        // the GPU cannot access the pages after the physical memory is freed.
        //   draw_str("before unmap_from_ggtt", 600, 220, 0xffffffff, 0x00000000);

        unmap_from_ggtt(gtt_index, num_pages);
        //    draw_str("before free_transient", 600, 240, 0xffffffff, 0x00000000);

        ggtt_alloc_.free_transient(gtt_index);
        //    draw_str("after free_transient", 600, 260, 0xffffffff, 0x00000000);

        // Return physical pages to the physical memory allocator.
        const phys_addr_t phys = virt_to_phys(alloc.cpu_addr);
        kernel::memory::free_pages_phys(phys, num_pages);
    }

    gfx_addr_t IntelBlt::map_to_ggtt(phys_addr_t phys_addr, usize num_pages, u8 pat_index) {
        if (gtt_next_free_ + num_pages > gtt_total_entries_) {
            // // Log::log_dbc("GGTT out of space!");
            error_count_++;
            return make_gfx(0);
        }

        u32 gtt_index = gtt_next_free_;

        for (usize i = 0; i < num_pages; i++) {
            u64 page_phys = phys_raw(phys_add(phys_addr, i * PAGE_SIZE));

            u64 gtt_entry = page_phys & GTT_PHYS_ADDR_MASK;
            gtt_entry |= (static_cast<u64>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
            gtt_entry |= GTT_VALID;

            gtt_entries_[gtt_index + i] = gtt_entry;
            asm volatile("mfence" ::: "memory");
        }

        gtt_next_free_ += num_pages;
        return make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE);
    }

    // https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02c-commandreference-registers-part2_0.pdf
    // (page 604)
    void IntelBlt::set_display_framebuffer() const {
        auto plane_regs = reinterpret_cast<volatile u32*>(mmio_base_);

        // Disable plane
        u32 plane_ctl = plane_regs[PLANE_CTL_1_A / 4];
        plane_regs[PLANE_CTL_1_A / 4] = plane_ctl & ~PLANE_CTL_ENABLE;

        for (int i = 0; i < 1000; i++) {
        }

        u32 stride_value = 0;
        switch (fb_.tile_mode) {
            case TileMode::Linear:
                // Linear: stride in chunks of 64 bytes
                stride_value = fb_.pitch / 64;
                plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888;
                break;

            case TileMode::X:
                stride_value = fb_.pitch / 512;
                plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888 | (0b001 << 10);
                break;

            case TileMode::Y:
                stride_value = fb_.pitch / 128;
                plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888 | PLANE_CTL_TILE_Y;
                break;

            default:
                // Log::log_dbc("Unsupported tile mode!");
                return;
        }

        plane_regs[PLANE_STRIDE_1_A / 4] = stride_value & 0x3FF;

        // Set size (width-1, height-1)
        u32 size = ((fb_.height - 1) << 16) | (fb_.width - 1);
        plane_regs[PLANE_SIZE_1_A / 4] = size;

        // Set position (0, 0 for fullscreen)
        plane_regs[PLANE_POS_1_A / 4] = 0;
        plane_regs[PLANE_OFFSET_1_A / 4] = 0;

        plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888;

        plane_regs[PLANE_CTL_1_A / 4] = plane_ctl;
        plane_regs[PLANE_SURF_1_A / 4] = static_cast<u32>(gfx_raw(fb_.gfx_addr));
        plane_regs[PLANE_CTL_1_A / 4] = plane_ctl | PLANE_CTL_ENABLE;

        asm volatile("mfence" ::: "memory");

        Log::log_dbc(
            "Display framebuffer updated: addr=0x%llx, %dx%d, pitch=%d",
            gfx_raw(fb_.gfx_addr),
            fb_.width,
            fb_.height,
            fb_.pitch
        );
    }

    void IntelBlt::xy_mono_src_copy_blt(
        gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2,
        gfx_addr_t mono_src_addr, u32 src_bit_position, bool transparency_enabled, u32 bg_color, u32 fg_color
    ) {
        XY_MONO_SRC_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_MONO_SRC_COPY_BLT;
        cmd.dw0.write_alpha = 1;
        cmd.dw0.write_rgb = 1;
        cmd.dw0.mono_src_bit_pos = src_bit_position & 0x7;
        cmd.dw0.dword_len = XY_MONO_SRC_COPY_LEN;

        cmd.dw1.color_depth = COLOR_DEPTH_32BPP;
        cmd.dw1.rop = SRCCOPY;
        cmd.dw1.transparency = transparency_enabled ? 1u : 0u;
        cmd.dw1.dest_pitch = dest_pitch & 0xFFFF;

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;

        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest_addr) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest_addr) >> 32);

        cmd.mono_src_addr_lo = static_cast<u32>(gfx_raw(mono_src_addr) & 0xFFFFFFFF);
        cmd.mono_src_addr_hi = static_cast<u32>(gfx_raw(mono_src_addr) >> 32);

        cmd.bg_color = bg_color;

        cmd.fg_color = fg_color;

        cmd.trailing = 0;

        write_command_struct(cmd);
    }

    void IntelBlt::build_text_scanline(const char* text, usize length, PsfFont* font, u8* buffer, usize buffer_stride) {
        const usize glyph_width = font->width;
        const usize glyph_height = font->height;
        const usize glyph_stride = (glyph_width + 7) / 8;

        auto glyphs = static_cast<const u8*>(font->glyph_buffer);

        // Hintergrund transparent
        memset(buffer, 0, buffer_stride * glyph_height);

        for (usize char_idx = 0; char_idx < length; char_idx++) {
            const u8 c = static_cast<u8>(text[char_idx]);
            const usize glyph_offset = static_cast<usize>(c) * static_cast<usize>(font->charsize);
            const u8* glyph = glyphs + glyph_offset;

            // Glyph beginnt exakt an seiner Zelle
            const u32 char_x = char_idx * glyph_width;

            for (u32 y = 0; y < glyph_height; y++) {
                u8* dst_row = buffer + y * buffer_stride;
                const u8* src_row = glyph + y * glyph_stride;

                for (u32 src_byte = 0; src_byte < glyph_stride; src_byte++) {
                    const u8 bits = src_row[src_byte];  // PSF2: MSB-first

                    const u32 glyph_bit_base = src_byte * 8;
                    if (glyph_bit_base >= glyph_width) break;

                    u32 bits_to_process = glyph_width - glyph_bit_base;
                    if (bits_to_process > 8) bits_to_process = 8;

                    for (u32 bit = 0; bit < bits_to_process; bit++) {
                        if (bits & (0x80 >> bit)) {
                            const u32 dst_pixel = char_x + glyph_bit_base + bit;
                            const u32 dst_byte = dst_pixel / 8;
                            const u32 dst_bit = dst_pixel % 8;
                            dst_row[dst_byte] |= (0x80 >> dst_bit);
                        }
                    }
                }
            }
        }

        asm volatile("sfence" ::: "memory");
    }

    bool IntelBlt::draw_str(const char* text, u32 x, u32 y, u32 fg_color, u32 bg_color) {
        if (!text || !system_font || virt_null(text_buffer_.cpu_addr)) {
            // Log::log_dbc("Invalid parameters for draw_string");
            return false;
        }

        usize text_len = strlen(text);
        if (text_len == 0) {
            return true;
        }

        // Calculate text dimensions
        u32 text_width = text_len * system_font->width;
        u32 text_height = system_font->height;

        usize text_stride = ((text_width + 7) / 8);
        text_stride = ((text_stride + 1) / 2) * 2;

        if (text_stride * text_height > text_buffer_.total_size) {
            /* Log::log_dbc(
                "Text too large for buffer: stride=%u, height=%u (max size=%zu)",
                text_stride,
                text_height,
                text_buffer_.total_size
            );*/
            return false;
        }

        check_gpu_health();

        if (u32 required = 12 * 4 + 64; !wait_for_ring_space(required, 1000000)) {
            // Log::log_dbc("Ring buffer full!");
            return false;
        }

        build_text_scanline(text, text_len, system_font, virt_as<u8>(text_buffer_.cpu_addr), text_stride);

        asm volatile("mfence" ::: "memory");

        xy_mono_src_copy_blt(
            fb_.gfx_addr,
            fb_.pitch,
            x,
            y,
            x + text_width,
            y + text_height,
            text_buffer_.gfx_addr,
            0,
            false,
            bg_color,
            fg_color
        );

        u32 target_seqno = next_seqno();
        mi_flush(target_seqno);
        flush_commands();

        if (!wait_for_sequence(target_seqno, 1000000)) {
            /* // Log::log_dbc(
                 "Timeout waiting for text! HEAD=0x%x TAIL=0x%x",
                 bcs_regs_[BCS_RING_HEAD / 4],
                 bcs_regs_[BCS_RING_TAIL / 4]
             );*/
            error_count_++;
            return false;
        }

        return true;
    }

    void IntelBlt::xy_src_copy_blt(
        gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src_addr,
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

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest_addr) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest_addr) >> 32);

        cmd.dw6.src_x1 = src_x1;
        cmd.dw6.src_y1 = src_y1;

        cmd.dw7.src_pitch = src_pitch & 0xFFFF;

        cmd.src_addr_lo = static_cast<u32>(gfx_raw(src_addr) & 0xFFFFFFFF);
        cmd.src_addr_hi = static_cast<u32>(gfx_raw(src_addr) >> 32);

        write_command_struct(cmd);
    }

    void IntelBlt::xy_fast_copy_blt(
        gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2, gfx_addr_t src_addr,
        u32 src_pitch, u32 src_x1, u32 src_y1
    ) {
        XY_FAST_COPY_BLT_CMD cmd{};

        cmd.dw0.client = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode = OPCODE_XY_FAST_COPY_BLT;
        cmd.dw0.src_tiling = tile_mode_to_blt_flag(fb_.tile_mode);
        cmd.dw0.dst_tiling = TILING_LINEAR;
        cmd.dw0.dword_len = XY_FAST_COPY_BLT_LEN;

        cmd.dw1.color_depth = FAST_COLOR_DEPTH_32BPP;
        cmd.dw1.dest_pitch = dest_pitch & 0x7FFF;  // bit[15] must be 0 (positive)

        cmd.dw2.x1 = dest_x1;
        cmd.dw2.y1 = dest_y1;

        cmd.dw3.x2 = dest_x2;
        cmd.dw3.y2 = dest_y2;

        cmd.dest_addr_lo = static_cast<u32>(gfx_raw(dest_addr) & 0xFFFFFFFF);
        cmd.dest_addr_hi = static_cast<u32>(gfx_raw(dest_addr) >> 32);

        cmd.dw6.src_x1 = src_x1;
        cmd.dw6.src_y1 = src_y1;

        cmd.dw7.src_pitch = src_pitch & 0x7FFF;

        cmd.src_addr_lo = static_cast<u32>(gfx_raw(src_addr) & 0xFFFFFFFF);
        cmd.src_addr_hi = static_cast<u32>(gfx_raw(src_addr) >> 32);

        write_command_struct(cmd);
    }

    bool IntelBlt::blit_buffer(const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y) {
        if (!pixels) return false;

        u32 max_w = buffer_width;
        u32 max_h = buffer_height;
        if (dst_x >= fb_.width || dst_y >= fb_.height) return false;
        if (dst_x + buffer_width > fb_.width) max_w = fb_.width - dst_x;
        if (dst_y + buffer_height > fb_.height) max_h = fb_.height - dst_y;

        // Allocate temporary GPU buffer
        constexpr usize bytes_per_pixel = 4;
        const usize width_bytes = static_cast<usize>(buffer_width) * bytes_per_pixel;
        const usize src_pitch = ((width_bytes + 63) / 64) * 64;
        const usize buffer_size = src_pitch * max_h;
        const usize num_pages = (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

        const auto temp_buffer = alloc_and_map_to_ggtt_transient(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(temp_buffer.cpu_addr)) {
            return false;
        }

        const auto* src = static_cast<const u8*>(pixels);
        u8* dst = virt_as<u8>(temp_buffer.cpu_addr);

        for (u32 y = 0; y < max_h; y++) {
            memcpy(dst + y * src_pitch, src + y * width_bytes, width_bytes);
        }

        check_gpu_health();
        if (constexpr u32 required = 30 * 4 + 64; !wait_for_ring_space(required, 1'000'000)) {
            free_ggtt_transient(temp_buffer, num_pages);
            return false;
        }

        xy_src_copy_blt(
            fb_.gfx_addr, fb_.pitch, dst_x, dst_y, dst_x + max_w, dst_y + max_h, temp_buffer.gfx_addr, src_pitch, 0, 0
        );

        const u32 target_seqno = next_seqno();
        mi_flush(target_seqno);
        flush_commands();
        //    draw_str("flushed", 600, 180, 0xffffffff, 0x00000000);

        const bool success = wait_for_sequence(target_seqno, 500'000);
        //   draw_str("done waiting", 600, 200, 0xffffffff, 0x00000000);

        free_ggtt_transient(temp_buffer, num_pages);

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

        // Pixel-Kopie für den Worker - flach, pitch = max_w * 4
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
        req->src_stride = max_w;  // flache Kopie, pitch = max_w
        req->src_x = 0;
        req->src_y = 0;
        req->dst_x = dst_x;
        req->dst_y = dst_y;
        req->w = max_w;
        req->h = max_h;
        req->done = nullptr;  // fire-and-forget

        blt_queue_.submit(req);
        return true;
    }

    bool IntelBlt::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
        auto* req = new GpuBltRequest();
        req->op = GpuBltOp::FillRect;
        req->dst_x = px;
        req->dst_y = py;
        req->w = w;
        req->h = h;
        req->color = colour;
        req->done = nullptr;  // fire-and-forget

        blt_queue_.submit(req);
        return true;
    }

    bool IntelBlt::execute_blit_region(const GpuBltRequest& req) {
        const u32 max_w = (req.dst_x + req.w > fb_.width) ? fb_.width - req.dst_x : req.w;
        const u32 max_h = (req.dst_y + req.h > fb_.height) ? fb_.height - req.dst_y : req.h;
        if (max_w == 0 || max_h == 0) return true;

        if (!scratch_.valid) {
            Log::error("intel-blt: scratch buffer not initialized");
            return false;
        }
        if (max_w > scratch_.max_w || max_h > scratch_.max_h) {
            Log::error("intel-blt: blit %ux%u exceeds scratch %ux%u", max_w, max_h, scratch_.max_w, scratch_.max_h);
            return false;
        }

        const usize row_bytes = static_cast<usize>(max_w) * BYTES_PER_PIXEL;

        const u8* src = reinterpret_cast<const u8*>(req.owned_pixels);
        u8* dst = virt_as<u8>(scratch_.alloc.cpu_addr);

        for (u32 y = 0; y < max_h; y++) {
            memcpy(dst + static_cast<usize>(y) * scratch_.pitch, src + static_cast<usize>(y) * row_bytes, row_bytes);
        }

        asm volatile("mfence" ::: "memory");

        check_gpu_health();
        if (constexpr u32 REQUIRED_BYTES = 30 * 4 + 64; !wait_for_ring_space(REQUIRED_BYTES, 1'000'000)) {
            return false;
        }

        xy_src_copy_blt(
            fb_.gfx_addr,
            fb_.pitch,
            req.dst_x,
            req.dst_y,
            req.dst_x + max_w,
            req.dst_y + max_h,
            scratch_.alloc.gfx_addr,
            scratch_.pitch,
            0,
            0
        );

        const u32 target_seqno = next_seqno();
        mi_flush(target_seqno);
        flush_commands();

        return wait_for_sequence(target_seqno, 500'000);
    }

    bool IntelBlt::execute_fill_rect(const GpuBltRequest& req) {
        const BltRect rect{.x = req.dst_x, .y = req.dst_y, .width = req.w, .height = req.h};
        if (!validate_blt_params(rect)) return false;

        check_gpu_health();
        if (constexpr u32 required = 12 * 4 + 64; !wait_for_ring_space(required, 1'000'000)) return false;

        xy_color_blt(fb_.gfx_addr, fb_.pitch, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height, req.color);

        const u32 target_seqno = next_seqno();
        mi_flush(target_seqno);
        flush_commands();

        return wait_for_sequence(target_seqno, 5'000'000);
    }

    bool IntelBlt::scroll_pixels(const int dy) {
        if (dy == 0) return true;

        if (dy >= static_cast<int>(fb_.height)) {
            xy_color_blt(fb_.gfx_addr, fb_.pitch, 0, 0, fb_.width, fb_.height, BLACK);
            return true;
        }

        check_gpu_health();

        const u32 copy_height = fb_.height - dy;

        if (constexpr u32 required = 30 * 4 + 64; !wait_for_ring_space(required, 1000000)) return false;

        xy_src_copy_blt(fb_.gfx_addr, fb_.pitch, 0, 0, fb_.width, copy_height, fb_.gfx_addr, fb_.pitch, 0, dy);

        xy_color_blt(fb_.gfx_addr, fb_.pitch, 0, copy_height, fb_.width, fb_.height, BLACK);

        const u32 target_seqno = next_seqno();
        mi_flush(target_seqno);
        flush_commands();

        if (!wait_for_sequence(target_seqno, 2'000'000)) {
            error_count_++;
            return false;
        }

        return true;
    }

    u32 IntelBlt::screen_height_px() const {
        return fb_.height;
    }

    u32 IntelBlt::screen_width_px() const {
        return fb_.width;
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
}  // namespace blt