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
#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "../../../filesystem/devfs/devfs.h"

IntelBlt::IntelBlt(pci::PCI_DEVICE_HEADER* header)
    : ring_size_(RING_BUFFER_SIZE)
    , sequence_number_(0) {
    const auto* pci = reinterpret_cast<pci::PCI_HEADER0*>(header);

    phys_addr_t bar0 = make_phys(pci->bar0 & BAR0_ADDR_MASK);
    kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE, (1ULL << CacheDisabled));

    mmio_base_ = static_cast<volatile uint8_t*>(virt_ptr(phys_to_virt(bar0)));
    bcs_regs_ = reinterpret_cast<volatile uint32_t*>(mmio_base_ + BCS_RING_BASE);

    enable_force_wake();
    init_gtt();
    enable_bcs_power();
    reset_bcs();

    auto hwsp = alloc_and_map_to_ggtt(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
    hwsp_cpu_addr_ = hwsp.cpu_addr;
    hwsp_gfx_addr_ = hwsp.gfx_addr;
    memset(hwsp_cpu_addr_, 0, PAGE_SIZE);
    bcs_regs_[BCS_HWSP / 4] = static_cast<uint32_t>(gfx_raw(hwsp_gfx_addr_));

    const uint32_t ring_pages = ring_size_ / PAGE_SIZE;
    auto [cpu_addr, gfx_addr] = alloc_and_map_to_ggtt(ring_pages);

    ring_cpu_addr_ = cpu_addr;
    ring_gfx_addr_ = gfx_addr;
    memset(ring_cpu_addr_, 0, ring_size_);
    setup_ring_buffer();

    bcs_regs_[BCS_RING_START / 4] = static_cast<uint32_t>(gfx_raw(ring_gfx_addr_) & GTT_PHYS_ADDR_MASK);

    const uint32_t ring_ctl = ((ring_size_ - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
    bcs_regs_[BCS_RING_CTL / 4] = ring_ctl;

    bcs_regs_[BCS_RING_HEAD / 4] = 0;
    bcs_regs_[BCS_RING_TAIL / 4] = 0;

    const uint32_t head = bcs_regs_[BCS_RING_HEAD / 4];
    const uint32_t tail = bcs_regs_[BCS_RING_TAIL / 4];
    const uint32_t ctl = bcs_regs_[BCS_RING_CTL / 4];

    init_text_buffer(system_font, target_framebuffer->width);

    if ((ctl & RING_CTL_ENABLED) && (head == tail)) {
        Log::debug("BCS is READY!");
    } else {
        Log::error("BCS initialization failed!");
        return;
    }

    char name[16];
    DeviceManager::alloc_unique_device_name("intel_blt", name, sizeof(name));
    kd_ = DeviceManager::register_gpu_device(
        this, name, DeviceClass::Graphics, BusType::Pci, ControllerType::IntelGpu, nullptr
    );

    DevFs::register_device(kd_);
}

void IntelBlt::start_device(uint32_t screen_width, uint32_t screen_height) {
    alloc_framebuffer(screen_width, screen_height, TileMode::Linear);
    set_display_framebuffer();
}

void IntelBlt::init_text_buffer(const font_t* font, const uint32_t screen_width) {
    if (!font) {
        Log::error("Invalid font for text buffer initialization");
        return;
    }

    const uint32_t max_chars = screen_width / font->width;
    const uint32_t max_width = max_chars * font->width;
    const uint32_t max_height = font->height;

    size_t stride = ((max_width + 7) / 8);
    stride = ((stride + 1) / 2) * 2;
    const size_t total_size = stride * max_height;
    const size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

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

void IntelBlt::write_command(uint32_t cmd) {
    uint32_t head = bcs_regs_[BCS_RING_HEAD / 4];
    uint32_t available_space = 0;

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

    volatile auto* ring = virt_as<uint32_t>(ring_cpu_addr_);
    ring[ring_tail_ / 4] = cmd;

    asm volatile("sfence" ::: "memory");

    ring_tail_ += 4;
    if (ring_tail_ >= ring_size_) {
        ring_tail_ = 0;
    }
}

bool IntelBlt::wait_for_ring_space(uint32_t required_bytes, uint32_t timeout_us) const {
    for (uint32_t i = 0; i < timeout_us; i++) {
        uint32_t head = bcs_regs_[BCS_RING_HEAD / 4];
        uint32_t available = 0;

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
    gfx_addr_t dest_addr, uint32_t dest_pitch, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color
) {
    // DW0: Command Header
    uint32_t dw0 =
        XY_COLOR_BLT_CMD | XY_COLOR_BLT_OPCODE | XY_COLOR_BLT_WRITE_RGB | XY_COLOR_BLT_WRITE_ALPHA | XY_COLOR_BLT_LEN;

    // DW1: BR13 - Format and ROP
    uint32_t dw1 = COLOR_DEPTH_8888 | PATCOPY | ((dest_pitch)&COORD_MASK);

    // DW2: BR22 - Top-Left coordinate (Y1, X1)
    uint32_t dw2 = ((y1 & COORD_MASK) << COORD_Y_SHIFT) | (x1 & COORD_MASK);

    // DW3: BR23 - Bottom-Right coordinate (Y2, X2)
    uint32_t dw3 = ((y2 & COORD_MASK) << COORD_Y_SHIFT) | (x2 & COORD_MASK);

    // DW4-5: Destination Base Address (64-bit)
    auto dw4 = static_cast<uint32_t>(gfx_raw(dest_addr) & 0xFFFFFFFF);
    auto dw5 = static_cast<uint32_t>(gfx_raw(dest_addr) >> 32);

    // DW6: BR16 - Solid Pattern Color (ARGB8888)
    uint32_t dw6 = color;

    write_command(dw0);
    write_command(dw1);
    write_command(dw2);
    write_command(dw3);
    write_command(dw4);
    write_command(dw5);
    write_command(dw6);
}

void IntelBlt::emergency_reset_bcs() {
    Log::error("Emergency BCS reset initiated!");

    bcs_regs_[BCS_RING_CTL / 4] &= ~RING_CTL_ENABLED;

    reset_bcs();

    memset(ring_cpu_addr_, 0, ring_size_);
    setup_ring_buffer();

    uint32_t ring_ctl = ((ring_size_ - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
    bcs_regs_[BCS_RING_CTL / 4] = ring_ctl;
    bcs_regs_[BCS_RING_HEAD / 4] = 0;
    bcs_regs_[BCS_RING_TAIL / 4] = 0;
    ring_tail_ = 0;

    Log::error("Emergency reset complete");
}

void IntelBlt::check_gpu_health() {
    const uint32_t head = bcs_regs_[BCS_RING_HEAD / 4];
    const uint32_t tail = bcs_regs_[BCS_RING_TAIL / 4];

    // Check if ring is still enabled
    if (const uint32_t ctl = bcs_regs_[BCS_RING_CTL / 4]; !(ctl & RING_CTL_ENABLED)) {
        Log::error("BCS ring disabled unexpectedly!");
        emergency_reset_bcs();
        return;
    }

    // Check for hung command
    static uint32_t last_head = 0;
    static uint32_t hang_counter = 0;

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

bool IntelBlt::fill_rect(uint32_t px, uint32_t py, uint32_t w, uint32_t h, uint32_t colour) {
    BltRect rect{.x = px, .y = py, .width = w, .height = h};

    if (!validate_blt_params(rect)) {
        return false;
    }

    check_gpu_health();

    if (constexpr uint32_t required = 12 * 4 + 64; !wait_for_ring_space(required, 1'000'000)) {
        Log::error("Ring buffer full!");
        return false;
    }

    const uint32_t x2 = rect.x + rect.width;
    const uint32_t y2 = rect.y + rect.height;

    xy_color_blt(fb_.gfx_addr, fb_.pitch, rect.x, rect.y, x2, y2, colour);

    sequence_number_++;
    const uint32_t target_seqno = sequence_number_;

    mi_flush(target_seqno);
    flush_commands();

    if (!wait_for_sequence(target_seqno, 5'000'000)) {
        Log::error("Timeout for sequence %u!", target_seqno);
        return false;
    }

    return true;
}

void IntelBlt::alloc_framebuffer(uint32_t width, uint32_t height, TileMode tile_mode) {
    fb_.width = width;
    fb_.height = height;
    fb_.bpp = 4;  // 32-bit ARGB
    fb_.tile_mode = tile_mode;

    fb_.pitch = ((width * fb_.bpp + 63) / 64) * 64;

    const size_t total_size = static_cast<size_t>(fb_.pitch) * height;
    const size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    Log::debug(
        "Allocating framebuffer: %dx%d, pitch=%d, size=%zu bytes, pages=%zu, tile_mode=%u",
        width,
        height,
        fb_.pitch,
        total_size,
        num_pages,
        static_cast<uint32_t>(tile_mode)
    );

    auto allocation = alloc_and_map_to_ggtt(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);

    fb_.gfx_addr = allocation.gfx_addr;
    fb_.cpu_addr = allocation.cpu_addr;

    memset(fb_.cpu_addr, 0, total_size);
}

void IntelBlt::mi_flush(uint32_t seqno) {
    constexpr uint32_t dw0 =
        MI_COMMAND_TYPE | MI_FLUSH_DW_OPCODE | MI_FLUSH_STORE_INDEX | MI_FLUSH_POST_SYNC | MI_FLUSH_DW_LEN;

    constexpr uint32_t dw1 = HWSP_SEQNO_OFFSET;
    constexpr uint32_t dw2 = 0;
    const uint32_t dw3 = seqno;

    write_command(dw0);
    write_command(dw1);
    write_command(dw2);
    write_command(dw3);
    write_command(0);
}

bool IntelBlt::wait_for_sequence(uint32_t target_seqno, uint32_t timeout_us) const {
    auto* hwsp = virt_as<uint32_t>(hwsp_cpu_addr_);
    uint32_t* seqno_ptr = &hwsp[HWSP_SEQNO_OFFSET_DWORDS];

    for (uint32_t i = 0; i < timeout_us; i++) {
        asm volatile("lfence" ::: "memory");

        if (uint32_t current_seqno = *seqno_ptr; static_cast<int32_t>(current_seqno - target_seqno) >= 0) {
            asm volatile("lfence" ::: "memory");
            return true;
        }

        // 1µs delay
        for (int j = 0; j < IDLE_CHECK_DELAY; j++);
    }

    asm volatile("lfence" ::: "memory");
    uint32_t final_seqno = *seqno_ptr;
    Log::error("Sequence timeout! Expected %u, got %u", target_seqno, final_seqno);

    return false;
}

void IntelBlt::flush_commands() const {
    asm volatile("mfence" ::: "memory");
    bcs_regs_[BCS_RING_TAIL / 4] = ring_tail_;
}

GgttAllocation IntelBlt::alloc_and_map_to_ggtt(size_t num_pages, uint64_t flags, uint8_t pat_index) {
    phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
    virt_addr_t cpu = phys_to_virt(phys);

    kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

    gfx_addr_t gfx = map_to_ggtt(phys, num_pages, pat_index);

    return {cpu, gfx};
}

void IntelBlt::setup_ring_buffer() {
    ring_tail_ = 0;

    Log::debug("Ring Buffer: CPU=%p GFX=0x%llx", virt_ptr(ring_cpu_addr_), gfx_raw(ring_gfx_addr_));

    volatile auto* ring = virt_as<uint32_t>(ring_cpu_addr_);
    for (uint32_t i = 0; i < ring_size_ / 4; i++) {
        ring[i] = MI_NOOP;
    }

    Log::debug("Ring Buffer cleared with NOOPs");
}

void IntelBlt::enable_force_wake() const {
    volatile auto* forcewake_mt = reinterpret_cast<volatile uint32_t*>(mmio_base_ + FORCEWAKE_MT);
    volatile auto* forcewake_ack = reinterpret_cast<volatile uint32_t*>(mmio_base_ + FORCEWAKE_ACK);

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
    auto bcs_swctrl = reinterpret_cast<volatile uint32_t*>(mmio_base_ + BCS_SWCTRL);
    *bcs_swctrl |= BCS_SWCTRL_WAKEUP;
    Log::debug("BCS Power enabled");
}

void IntelBlt::reset_bcs() const {
    auto reset_ctl = reinterpret_cast<volatile uint32_t*>(mmio_base_ + RESET_CTL);

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
    gtt_entries_ = reinterpret_cast<volatile uint64_t*>(mmio_base_ + GTT_OFFSET);
    gtt_total_entries_ = GTT_TOTAL_ENTRIES;
    gtt_next_free_ = GTT_START_INDEX;
}

gfx_addr_t IntelBlt::map_to_ggtt(phys_addr_t phys_addr, size_t num_pages, uint8_t pat_index) {
    if (gtt_next_free_ + num_pages > gtt_total_entries_) {
        Log::error("GGTT out of space!");
        return make_gfx(0);
    }

    uint32_t gtt_index = gtt_next_free_;

    for (size_t i = 0; i < num_pages; i++) {
        uint64_t page_phys = phys_raw(phys_add(phys_addr, i * PAGE_SIZE));

        uint64_t gtt_entry = page_phys & GTT_PHYS_ADDR_MASK;
        gtt_entry |= (static_cast<uint64_t>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
        gtt_entry |= GTT_VALID;

        gtt_entries_[gtt_index + i] = gtt_entry;
        asm volatile("mfence" ::: "memory");
    }

    gtt_next_free_ += num_pages;
    return make_gfx(static_cast<uint64_t>(gtt_index) * PAGE_SIZE);
}

// https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02c-commandreference-registers-part2_0.pdf
// (page 604)
void IntelBlt::set_display_framebuffer() const {
    auto plane_regs = reinterpret_cast<volatile uint32_t*>(mmio_base_);

    // Disable plane
    uint32_t plane_ctl = plane_regs[PLANE_CTL_1_A / 4];
    plane_regs[PLANE_CTL_1_A / 4] = plane_ctl & ~PLANE_CTL_ENABLE;

    for (int i = 0; i < 1000; i++) {
    }

    uint32_t stride_value = 0;
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
    uint32_t size = ((fb_.height - 1) << 16) | (fb_.width - 1);
    plane_regs[PLANE_SIZE_1_A / 4] = size;

    // Set position (0, 0 for fullscreen)
    plane_regs[PLANE_POS_1_A / 4] = 0;
    plane_regs[PLANE_OFFSET_1_A / 4] = 0;

    plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888;

    plane_regs[PLANE_CTL_1_A / 4] = plane_ctl;
    plane_regs[PLANE_SURF_1_A / 4] = static_cast<uint32_t>(gfx_raw(fb_.gfx_addr));
    plane_regs[PLANE_CTL_1_A / 4] = plane_ctl | PLANE_CTL_ENABLE;

    asm volatile("mfence" ::: "memory");

    Log::debug(
        "Display framebuffer updated: addr=0x%llx, %dx%d, pitch=%d", gfx_raw(fb_.gfx_addr), fb_.width, fb_.height, fb_.pitch
    );
}

void IntelBlt::xy_mono_src_copy_blt(
    gfx_addr_t dest_addr, uint32_t dest_pitch, uint32_t dest_x1, uint32_t dest_y1, uint32_t dest_x2, uint32_t dest_y2,
    gfx_addr_t mono_src_addr, uint32_t src_bit_position, bool transparency_enabled, uint32_t bg_color, uint32_t fg_color
) {
    // DW0: BR00 - Command Header
    uint32_t dw0 = XY_MONO_SRC_COPY_CMD | XY_MONO_SRC_COPY_OPCODE | XY_MONO_SRC_COPY_WRITE_ALPHA |
                   XY_MONO_SRC_COPY_WRITE_RGB | ((src_bit_position & 0x7) << 17) | XY_MONO_SRC_COPY_LEN;

    // DW1: BR13 - Format, transparency, ROP, pitch
    uint32_t dw1 = (transparency_enabled ? MONO_SRC_TRANSPARENCY : MONO_SRC_USE_BACKGROUND) | COLOR_DEPTH_8888 |
                   (SRCCOPY << 16) | (dest_pitch & 0xFFFF);

    // DW2: BR22 - Destination Y1, X1
    uint32_t dw2 = ((dest_y1 & 0xFFFF) << 16) | (dest_x1 & 0xFFFF);

    // DW3: BR23 - Destination Y2, X2
    uint32_t dw3 = ((dest_y2 & 0xFFFF) << 16) | (dest_x2 & 0xFFFF);

    // DW4-5: Destination Base Address (64-bit)
    uint32_t dw4 = static_cast<uint32_t>(gfx_raw(dest_addr) & 0xFFFFFFFF);
    uint32_t dw5 = static_cast<uint32_t>(gfx_raw(dest_addr) >> 32);

    // DW6-7: Monochrome Source Address (64-bit, 64-byte aligned)
    uint32_t dw6 = static_cast<uint32_t>(gfx_raw(mono_src_addr) & 0xFFFFFFFF);
    uint32_t dw7 = static_cast<uint32_t>(gfx_raw(mono_src_addr) >> 32);

    // DW8: BR18 - Source Background Color
    uint32_t dw8 = bg_color;

    // DW9: BR19 - Source Foreground Color
    uint32_t dw9 = fg_color;

    write_command(dw0);
    write_command(dw1);
    write_command(dw2);
    write_command(dw3);
    write_command(dw4);
    write_command(dw5);
    write_command(dw6);
    write_command(dw7);
    write_command(dw8);
    write_command(dw9);
    write_command(0);  // trailing DW
}

void IntelBlt::build_text_scanline(const char* text, size_t length, font_t* font, uint8_t* buffer, size_t buffer_stride) {
    const size_t glyph_width = font->width;
    const size_t glyph_height = font->height;
    const size_t glyph_stride = (glyph_width + 7) / 8;

    auto glyphs = static_cast<const uint8_t*>(font->glyph_buffer);

    // Hintergrund transparent
    memset(buffer, 0, buffer_stride * glyph_height);

    for (size_t char_idx = 0; char_idx < length; char_idx++) {
        const uint8_t c = static_cast<uint8_t>(text[char_idx]);
        const size_t glyph_offset = static_cast<size_t>(c) * static_cast<size_t>(font->charsize);
        const uint8_t* glyph = glyphs + glyph_offset;

        // Glyph beginnt exakt an seiner Zelle
        const uint32_t char_x = char_idx * glyph_width;

        for (uint32_t y = 0; y < glyph_height; y++) {
            uint8_t* dst_row = buffer + y * buffer_stride;
            const uint8_t* src_row = glyph + y * glyph_stride;

            for (uint32_t src_byte = 0; src_byte < glyph_stride; src_byte++) {
                const uint8_t bits = src_row[src_byte];  // PSF2: MSB-first

                const uint32_t glyph_bit_base = src_byte * 8;
                if (glyph_bit_base >= glyph_width) break;

                uint32_t bits_to_process = glyph_width - glyph_bit_base;
                if (bits_to_process > 8) bits_to_process = 8;

                for (uint32_t bit = 0; bit < bits_to_process; bit++) {
                    if (bits & (0x80 >> bit)) {
                        const uint32_t dst_pixel = char_x + glyph_bit_base + bit;
                        const uint32_t dst_byte = dst_pixel / 8;
                        const uint32_t dst_bit = dst_pixel % 8;
                        dst_row[dst_byte] |= (0x80 >> dst_bit);
                    }
                }
            }
        }
    }

    asm volatile("sfence" ::: "memory");
}

bool IntelBlt::draw_str(const char* text, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    if (!text || !system_font || virt_null(text_buffer_.cpu_addr)) {
        Log::error("Invalid parameters for draw_string");
        return false;
    }

    size_t text_len = strlen(text);
    if (text_len == 0) {
        return true;
    }

    // Calculate text dimensions
    uint32_t text_width = text_len * system_font->width;
    uint32_t text_height = system_font->height;

    size_t text_stride = ((text_width + 7) / 8);
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

    if (uint32_t required = 12 * 4 + 64; !wait_for_ring_space(required, 1000000)) {
        Log::error("Ring buffer full!");
        return false;
    }

    build_text_scanline(text, text_len, system_font, virt_as<uint8_t>(text_buffer_.cpu_addr), text_stride);

    asm volatile("mfence" ::: "memory");

    xy_mono_src_copy_blt(
        fb_.gfx_addr, fb_.pitch, x, y, x + text_width, y + text_height, text_buffer_.gfx_addr, 0, false, bg_color, fg_color
    );

    sequence_number_++;
    uint32_t target_seqno = sequence_number_;
    mi_flush(target_seqno);
    flush_commands();

    if (!wait_for_sequence(target_seqno, 1000000)) {
        Log::error(
            "Timeout waiting for text! HEAD=0x%x TAIL=0x%x", bcs_regs_[BCS_RING_HEAD / 4], bcs_regs_[BCS_RING_TAIL / 4]
        );
        return false;
    }

    return true;
}

void IntelBlt::xy_src_copy_blt(
    gfx_addr_t dest_addr, uint32_t dest_pitch, uint32_t dest_x1, uint32_t dest_y1, uint32_t dest_x2, uint32_t dest_y2,
    gfx_addr_t src_addr, uint32_t src_pitch, uint32_t src_x1, uint32_t src_y1
) {
    // DW0: BR00 - Command Header
    uint32_t dw0 = XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_OPCODE | XY_SRC_COPY_WRITE_ALPHA | XY_SRC_COPY_WRITE_RGB |
                   XY_SRC_COPY_BLT_LEN;

    // DW1: BR13 - Format and ROP
    uint32_t dw1 = COLOR_DEPTH_8888 | (SRCCOPY << 16) |  // ROP: SRCCOPY (0xCC)
                   (dest_pitch & 0xFFFF);

    // DW2: BR22 - Destination Y1, X1
    uint32_t dw2 = ((dest_y1 & COORD_MASK) << COORD_Y_SHIFT) | (dest_x1 & COORD_MASK);

    // DW3: BR23 - Destination Y2, X2
    uint32_t dw3 = ((dest_y2 & COORD_MASK) << COORD_Y_SHIFT) | (dest_x2 & COORD_MASK);

    // DW4-5: Destination Base Address (64-bit)
    uint32_t dw4 = static_cast<uint32_t>(gfx_raw(dest_addr) & 0xFFFFFFFF);
    uint32_t dw5 = static_cast<uint32_t>(gfx_raw(dest_addr) >> 32);

    // DW6: BR26 - Source Y1, X1
    uint32_t dw6 = ((src_y1 & COORD_MASK) << COORD_Y_SHIFT) | (src_x1 & COORD_MASK);

    // DW7: BR11 - Source Pitch
    uint32_t dw7 = src_pitch & 0xFFFF;

    // DW8-9: Source Base Address (64-bit)
    uint32_t dw8 = static_cast<uint32_t>(gfx_raw(src_addr) & 0xFFFFFFFF);
    uint32_t dw9 = static_cast<uint32_t>(gfx_raw(src_addr) >> 32);

    write_command(dw0);
    write_command(dw1);
    write_command(dw2);
    write_command(dw3);
    write_command(dw4);
    write_command(dw5);
    write_command(dw6);
    write_command(dw7);
    write_command(dw8);
    write_command(dw9);
}

void IntelBlt::xy_fast_copy_blt(
    gfx_addr_t dest_addr, uint32_t dest_pitch, uint32_t dest_x1, uint32_t dest_y1, uint32_t dest_x2, uint32_t dest_y2,
    gfx_addr_t src_addr, uint32_t src_pitch, uint32_t src_x1, uint32_t src_y1
) {
    // DW0: BR00 – Command Header
    uint32_t dw0 = XY_FAST_COPY_BLT_CMD | XY_FAST_COPY_BLT_OPCODE | FAST_SRC_TILING_LINEAR | FAST_DST_TILING_LINEAR |
                   XY_FAST_COPY_BLT_LEN;

    // DW1: BR13 – Color depth + Destination Pitch (BYTES!)
    uint32_t dw1 = FAST_COLOR_DEPTH_8888 | (dest_pitch & 0x7FFF);  // must be positive, bit15 = 0

    // DW2: BR22 – Dest Y1, X1
    uint32_t dw2 = ((dest_y1 & COORD_MASK) << COORD_Y_SHIFT) | (dest_x1 & COORD_MASK);

    // DW3: BR23 – Dest Y2, X2
    uint32_t dw3 = ((dest_y2 & COORD_MASK) << COORD_Y_SHIFT) | (dest_x2 & COORD_MASK);

    // DW4–5: Destination Base Address
    uint32_t dw4 = static_cast<uint32_t>(gfx_raw(dest_addr) & 0xFFFFFFFF);
    uint32_t dw5 = static_cast<uint32_t>(gfx_raw(dest_addr) >> 32);

    // DW6: BR26 – Source Y1, X1
    uint32_t dw6 = ((src_y1 & COORD_MASK) << COORD_Y_SHIFT) | (src_x1 & COORD_MASK);

    // DW7: BR11 – Source Pitch (BYTES!)
    uint32_t dw7 = src_pitch & 0x7FFF;

    // DW8–9: Source Base Address
    uint32_t dw8 = static_cast<uint32_t>(gfx_raw(src_addr) & 0xFFFFFFFF);
    uint32_t dw9 = static_cast<uint32_t>(gfx_raw(src_addr) >> 32);

    write_command(dw0);
    write_command(dw1);
    write_command(dw2);
    write_command(dw3);
    write_command(dw4);
    write_command(dw5);
    write_command(dw6);
    write_command(dw7);
    write_command(dw8);
    write_command(dw9);
}

bool IntelBlt::blit_buffer(
    const void* pixels, uint32_t buffer_width, uint32_t buffer_height, uint32_t dst_x, uint32_t dst_y
) {
    if (!pixels) return false;

    uint32_t max_w = buffer_width;
    uint32_t max_h = buffer_height;
    if (dst_x >= fb_.width || dst_y >= fb_.height) return false;
    if (dst_x + buffer_width > fb_.width) max_w = fb_.width - dst_x;
    if (dst_y + buffer_height > fb_.height) max_h = fb_.height - dst_y;

    // Allocate temporary GPU buffer
    constexpr size_t bytes_per_pixel = 4;
    const size_t width_bytes = static_cast<size_t>(buffer_width) * bytes_per_pixel;
    const size_t src_pitch = ((width_bytes + 63) / 64) * 64;
    const size_t buffer_size = src_pitch * max_h;
    const size_t num_pages = (buffer_size + PAGE_SIZE - 1) / PAGE_SIZE;

    const auto temp_buffer = alloc_and_map_to_ggtt(num_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);

    const auto* src = static_cast<const uint8_t*>(pixels);
    uint8_t* dst = virt_as<uint8_t>(temp_buffer.cpu_addr);

    for (uint32_t y = 0; y < max_h; y++) {
        memcpy(dst + y * src_pitch, src + y * width_bytes, width_bytes);
    }

    check_gpu_health();
    if (constexpr uint32_t required = 30 * 4 + 64; !wait_for_ring_space(required, 1'000'000)) {
        // free_ggtt_buffer(temp_buffer);
        return false;
    }

    xy_fast_copy_blt(
        fb_.gfx_addr, fb_.pitch, dst_x, dst_y, dst_x + max_w, dst_y + max_h, temp_buffer.gfx_addr, src_pitch, 0, 0
    );

    sequence_number_++;
    mi_flush(sequence_number_);
    flush_commands();

    // free_ggtt_buffer(temp_buffer);

    if (const bool success = wait_for_sequence(sequence_number_, 2'000'000); !success) {
        return false;
    }

    return true;
}

bool IntelBlt::scroll_pixels(const int dy) {
    if (dy == 0) return true;

    if (dy >= static_cast<int>(fb_.height)) {
        xy_color_blt(fb_.gfx_addr, fb_.pitch, 0, 0, fb_.width, fb_.height, BLACK);
        return true;
    }

    check_gpu_health();

    const uint32_t copy_height = fb_.height - dy;

    if (constexpr uint32_t required = 30 * 4 + 64; !wait_for_ring_space(required, 1000000)) return false;

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

uint32_t IntelBlt::screen_height_px() const {
    return fb_.height;
}

uint32_t IntelBlt::screen_width_px() const {
    return fb_.width;
}

uint32_t IntelBlt::bytes_per_scanline() const {
    return fb_.pitch;
}
