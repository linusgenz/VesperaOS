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

#include <klib/string.h>
#include <vespera/devices/device_manager.h>
#include <vespera/graphics/colors.h>
#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include <filesystem/devfs.h>
#include "blt_commands.h"

namespace blt {
    IntelBlt::IntelBlt(pci::PCI_DEVICE_HEADER* header)
        : pci_header_(reinterpret_cast<pci::PCI_HEADER0*>(header)), ring_size_(RING_BUFFER_SIZE)
        , sequence_number_(0) {

        const phys_addr_t bar0 = make_phys(pci_header_->bar0 & BAR0_ADDR_MASK);
        kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE, (1ULL << CacheDisabled));

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
        bcs_regs_ = reinterpret_cast<volatile u32*>(mmio_base_ + BCS_RING_BASE);

        enable_force_wake();
        init_gtt();
        enable_bcs_power();
        reset_bcs();

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
            Log::debug("BCS is READY!");
        } else {
            Log::error("BCS initialization failed!");
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

    void IntelBlt::start_device(u32 screen_width, u32 screen_height) {
        alloc_framebuffer(screen_width, screen_height, TileMode::Linear);
        set_display_framebuffer();
    }

    u32 IntelBlt::tile_mode_to_blt_flag(TileMode mode) {
        switch (mode) {
            case TileMode::X: return TILING_X;
            case TileMode::Y: return TILING_Y;
            default:          return TILING_LINEAR;
        }
    }

    void IntelBlt::init_text_buffer(const PsfFont* font, const u32 screen_width) {
        if (!font) {
            Log::error("Invalid font for text buffer initialization");
            return;
        }

        const u32 max_chars = screen_width / font->width;
        const u32 max_width = max_chars * font->width;
        const u32 max_height = font->height;

        usize stride = ((max_width + 7) / 8);
        stride = ((stride + 1) / 2) * 2;
        const usize total_size = stride * max_height;
        const usize num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

        Log::debug(
            "Allocating text buffer: font=%ux%u, screen=%u, max_chars=%u, buffer=%ux%u, stride=%u, size=%zu",
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
            Log::error("Ring buffer full! HEAD=0x%x TAIL=0x%x", head, ring_tail_);
            if (!wait_for_ring_space(68, 100000)) {
                // 100ms timeout
                Log::error("Ring buffer deadlock!");
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
            Log::error("Invalid framebuffer");
            return false;
        }

        // Check dimensions
        if (rect.width == 0 || rect.height == 0) {
            Log::error("Invalid rect dimensions: %dx%d", rect.width, rect.height);
            return false;
        }

        if (rect.width > 8192 || rect.height > 4096) {
            Log::error("Rect too large: %dx%d (max 8192x4096)", rect.width, rect.height);
            return false;
        }

        // Check bounds
        if (rect.x + rect.width > fb_.width || rect.y + rect.height > fb_.height) {
            Log::error(
                "Rect out of bounds: (%d,%d)-(%d,%d) in %dx%d FB",
                rect.x,
                rect.y,
                rect.x + rect.width,
                rect.y + rect.height,
                fb_.width,
                fb_.height
            );
            return false;
        }

        // Check alignment
        if (gfx_raw(fb_.gfx_addr) & 0x3F) {
            // 64-byte alignment
            Log::error("Framebuffer not aligned: 0x%llx", gfx_raw(fb_.gfx_addr));
            return false;
        }

        // Check pitch
        if (fb_.pitch & 0x3F) {
            // 64-byte alignment
            Log::error("Pitch not aligned: %d", fb_.pitch);
            return false;
        }

        return true;
    }

    void IntelBlt::xy_color_blt(
        gfx_addr_t dest_addr, u32 dest_pitch, u32 x1, u32 y1, u32 x2, u32 y2, u32 color
    ) {
        XY_COLOR_BLT_CMD cmd{};

        cmd.dw0.client       = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode       = OPCODE_XY_COLOR_BLT;
        cmd.dw0.write_alpha  = 1;
        cmd.dw0.write_rgb    = 1;
        cmd.dw0.dword_len    = XY_COLOR_BLT_LEN;

        cmd.dw1.color_depth  = COLOR_DEPTH_32BPP;
        cmd.dw1.rop          = ROP_PATCOPY;
        cmd.dw1.dest_pitch   = dest_pitch & COORD_MASK;

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
        Log::error("Emergency BCS reset initiated!");

        bcs_regs_[BCS_RING_CTL / 4] &= ~RING_CTL_ENABLED;

        reset_bcs();

        memset(ring_cpu_addr_, 0, ring_size_);
        setup_ring_buffer();

        const u32 ring_ctl = ((ring_size_ - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
        bcs_regs_[BCS_RING_CTL / 4] = ring_ctl;
        bcs_regs_[BCS_RING_HEAD / 4] = 0;
        bcs_regs_[BCS_RING_TAIL / 4] = 0;
        ring_tail_ = 0;

        Log::error("Emergency reset complete");
    }

    void IntelBlt::check_gpu_health() {
        const u32 head = bcs_regs_[BCS_RING_HEAD / 4];
        const u32 tail = bcs_regs_[BCS_RING_TAIL / 4];

        // Check if ring is still enabled
        if (const u32 ctl = bcs_regs_[BCS_RING_CTL / 4]; !(ctl & RING_CTL_ENABLED)) {
            Log::error("BCS ring disabled unexpectedly!");
            emergency_reset_bcs();
            return;
        }

        // Check for hung command
        static u32 last_head = 0;
        static u32 hang_counter = 0;

        if (head == last_head && head != tail) {
            hang_counter++;
            if (hang_counter > 1000) {
                Log::error("GPU hang detected! HEAD stuck at 0x%x", head);
                emergency_reset_bcs();
                hang_counter = 0;
            }
        } else {
            hang_counter = 0;
        }
        last_head = head;
    }

    bool IntelBlt::fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) {
        const BltRect rect{.x = px, .y = py, .width = w, .height = h};

        if (!validate_blt_params(rect)) {
            return false;
        }

        check_gpu_health();

        if (constexpr u32 required = 12 * 4 + 64; !wait_for_ring_space(required, 1'000'000)) {
            Log::error("Ring buffer full!");
            return false;
        }

        const u32 x2 = rect.x + rect.width;
        const u32 y2 = rect.y + rect.height;

        xy_color_blt(fb_.gfx_addr, fb_.pitch, rect.x, rect.y, x2, y2, colour);

        sequence_number_++;
        const u32 target_seqno = sequence_number_;

        mi_flush(target_seqno);
        flush_commands();

        if (!wait_for_sequence(target_seqno, 5'000'000)) {
            Log::error("Timeout for sequence %u!", target_seqno);
            return false;
        }

        return true;
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

        Log::debug(
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

        cmd.dw0.client      = CLIENT_MI;
        cmd.dw0.opcode      = OPCODE_MI_FLUSH_DW;
        cmd.dw0.store_index = 1;
        cmd.dw0.post_sync   = 1;
        cmd.dw0.dword_len   = MI_FLUSH_DW_LEN;

        cmd.address_or_offset = HWSP_SEQNO_OFFSET;
        cmd.address_hi        = 0;
        cmd.immediate_data    = seqno;
        cmd.reserved          = 0;

        write_command_struct(cmd);
    }

    bool IntelBlt::wait_for_sequence(u32 target_seqno, u32 timeout_us) const {
        auto* hwsp = virt_as<u32>(hwsp_cpu_addr_);
        u32* seqno_ptr = &hwsp[HWSP_SEQNO_OFFSET_DWORDS];

        for (u32 i = 0; i < timeout_us; i++) {
            asm volatile("lfence" ::: "memory");

            if (u32 current_seqno = *seqno_ptr; static_cast<i32>(current_seqno - target_seqno) >= 0) {
                asm volatile("lfence" ::: "memory");
                return true;
            }

            // 1µs delay
            for (int j = 0; j < IDLE_CHECK_DELAY; j++);
        }

        asm volatile("lfence" ::: "memory");
        u32 final_seqno = *seqno_ptr;
        Log::error("Sequence timeout! Expected %u, got %u", target_seqno, final_seqno);

        return false;
    }

    void IntelBlt::flush_commands() const {
        asm volatile("mfence" ::: "memory");
        bcs_regs_[BCS_RING_TAIL / 4] = ring_tail_;
    }

    void IntelBlt::setup_ring_buffer() {
        ring_tail_ = 0;

        Log::debug("Ring Buffer: CPU=%p GFX=0x%llx", virt_ptr(ring_cpu_addr_), gfx_raw(ring_gfx_addr_));

        volatile auto* ring = virt_as<u32>(ring_cpu_addr_);
        for (u32 i = 0; i < ring_size_ / 4; i++) {
            ring[i] = MI_NOOP;
        }

        Log::debug("Ring Buffer cleared with NOOPs");
    }

    void IntelBlt::enable_force_wake() const {
        volatile auto* forcewake_mt = reinterpret_cast<volatile u32*>(mmio_base_ + FORCEWAKE_MT);
        const volatile auto* forcewake_ack = reinterpret_cast<volatile u32*>(mmio_base_ + FORCEWAKE_ACK);

        *forcewake_mt = FORCEWAKE_ENABLE;

        // Wait for acknowledgment
        int timeout = FORCE_WAKE_TIMEOUT;
        while (timeout-- > 0) {
            if (*forcewake_ack & GTT_VALID) {
                Log::debug("Force Wake ACK received");
                break;
            }
            for (int i = 0; i < IDLE_CHECK_DELAY; i++);
        }

        if (timeout <= 0) {
            Log::error("Force Wake timeout!");
        }
    }

    void IntelBlt::enable_bcs_power() const {
        const auto bcs_swctrl = reinterpret_cast<volatile u32*>(mmio_base_ + BCS_SWCTRL);
        *bcs_swctrl |= BCS_SWCTRL_WAKEUP;
        Log::debug("BCS Power enabled");
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

        Log::debug("BCS Reset completed");
    }

    void IntelBlt::init_gtt() {
        gtt_entries_ = reinterpret_cast<volatile u64*>(mmio_base_ + GTT_OFFSET);
        ggtt_alloc_.init(GTT_TOTAL_ENTRIES, GTT_START_INDEX);
    }

    void IntelBlt::map_to_ggtt_at(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const {
        for (usize i = 0; i < num_pages; i++) {
            u64 page_phys  = phys_raw(phys_add(phys_addr, i * PAGE_SIZE));
            u64 gtt_entry  = page_phys & GTT_PHYS_ADDR_MASK;
            gtt_entry     |= (static_cast<u64>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
            gtt_entry     |= GTT_VALID;
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
        const virt_addr_t cpu  = phys_to_virt(phys);
        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

        const u32 gtt_index = ggtt_alloc_.alloc_persistent(static_cast<u32>(num_pages));
        if (gtt_index == U32_MAX) {
            Log::error("alloc_and_map_to_ggtt: persistent zone exhausted");
            // Physical pages are intentionally not freed here: persistent
            // allocations failing is a fatal driver initialisation error.
            return {};
        }

        map_to_ggtt_at(gtt_index, phys, num_pages, pat_index);
        return { cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE) };
    }

    GgttAllocation IntelBlt::alloc_and_map_to_ggtt_transient(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu  = phys_to_virt(phys);
        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

        const u32 gtt_index = ggtt_alloc_.alloc_transient(static_cast<u32>(num_pages));
        if (gtt_index == U32_MAX) {
            Log::error("alloc_and_map_to_ggtt_transient: transient zone exhausted");
            kernel::memory::free_pages_phys(phys, num_pages);
            return {};
        }

        map_to_ggtt_at(gtt_index, phys, num_pages, pat_index);
        return { cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE) };
    }

    // ── free_ggtt_transient() — new ───────────────────────────────────────────────
    // Unmaps GTT entries, returns the GTT index range to the allocator, and frees
    // the underlying physical pages.
    // @param alloc      The GgttAllocation returned by alloc_and_map_to_ggtt_transient.
    // @param num_pages  Must match the num_pages passed to the original alloc call.
    void IntelBlt::free_ggtt_transient(const GgttAllocation& alloc, usize num_pages) {
        if (virt_null(alloc.cpu_addr)) {
            return;
        }

        const u32 gtt_index = static_cast<u32>(gfx_raw(alloc.gfx_addr) / PAGE_SIZE);

        // Invalidate GTT entries before returning the index to the allocator so
        // the GPU cannot access the pages after the physical memory is freed.
        unmap_from_ggtt(gtt_index, num_pages);
        ggtt_alloc_.free_transient(gtt_index);

        // Return physical pages to the physical memory allocator.
        const phys_addr_t phys = virt_to_phys(alloc.cpu_addr);
        kernel::memory::free_pages_phys(phys, num_pages);
    }

    gfx_addr_t IntelBlt::map_to_ggtt(phys_addr_t phys_addr, usize num_pages, u8 pat_index) {
        if (gtt_next_free_ + num_pages > gtt_total_entries_) {
            Log::error("GGTT out of space!");
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
                Log::error("Unsupported tile mode!");
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

        Log::debug(
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
            Log::error("Invalid parameters for draw_string");
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
            Log::error(
                "Text too large for buffer: stride=%u, height=%u (max size=%zu)",
                text_stride,
                text_height,
                text_buffer_.total_size
            );
            return false;
        }

        check_gpu_health();

        if (u32 required = 12 * 4 + 64; !wait_for_ring_space(required, 1000000)) {
            Log::error("Ring buffer full!");
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

        sequence_number_++;
        u32 target_seqno = sequence_number_;
        mi_flush(target_seqno);
        flush_commands();

        if (!wait_for_sequence(target_seqno, 1000000)) {
            Log::error(
                "Timeout waiting for text! HEAD=0x%x TAIL=0x%x",
                bcs_regs_[BCS_RING_HEAD / 4],
                bcs_regs_[BCS_RING_TAIL / 4]
            );
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
        gfx_addr_t dest_addr, u32 dest_pitch, u32 dest_x1, u32 dest_y1, u32 dest_x2, u32 dest_y2,
        gfx_addr_t src_addr,  u32 src_pitch,  u32 src_x1,  u32 src_y1
    ) {
        XY_FAST_COPY_BLT_CMD cmd{};

        cmd.dw0.client     = CLIENT_2D_PROCESSOR;
        cmd.dw0.opcode     = OPCODE_XY_FAST_COPY_BLT;
        cmd.dw0.src_tiling = tile_mode_to_blt_flag(fb_.tile_mode);
        cmd.dw0.dst_tiling = TILING_LINEAR;
        cmd.dw0.dword_len  = XY_FAST_COPY_BLT_LEN;

        cmd.dw1.color_depth = FAST_COLOR_DEPTH_32BPP;
        cmd.dw1.dest_pitch  = dest_pitch & 0x7FFF;  // bit[15] must be 0 (positive)

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

        const auto temp_buffer = alloc_and_map_to_ggtt_transient(
            num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED
        );
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

        xy_fast_copy_blt(
            fb_.gfx_addr, fb_.pitch, dst_x, dst_y, dst_x + max_w, dst_y + max_h, temp_buffer.gfx_addr, src_pitch, 0, 0
        );

        sequence_number_++;
        mi_flush(sequence_number_);
        flush_commands();

        const bool success = wait_for_sequence(sequence_number_, 2'000'000);

        free_ggtt_transient(temp_buffer, num_pages);

        return success;
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

        sequence_number_++;
        mi_flush(sequence_number_);
        flush_commands();

        if (!wait_for_sequence(sequence_number_, 2'000'000)) {
            Log::error("Scroll timeout (combined)");
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
        strncpy(out, pci::get_vendor_name(pci_header_->header.vendor_id), len);
        out[len - 1] = '\0';
        return true;
    }

    bool IntelBlt::get_model(char* out, const usize len) {
        strncpy(out, pci::get_device_name(pci_header_->header.vendor_id, pci_header_->header.device_id), len);
        out[len - 1] = '\0';
        return true;
    }
}  // namespace blt