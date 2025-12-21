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

#include <log.h>

#include "kernel/memory.h"
#include "kernel/time.h"

IntelBlt::IntelBlt(PCI::PCIDeviceHeader* header)
{
    auto* pci = reinterpret_cast<PCI::PCIHeader0*>(header);

    uint64_t bar0 = pci->BAR0 & BAR0_ADDR_MASK;
    kernel::memory::map_range(
        reinterpret_cast<void*>(bar0),
        reinterpret_cast<void*>(bar0),
        BAR0_SIZE,
        (1ULL << CacheDisabled)
    );

    mmio_base = reinterpret_cast<volatile uint8_t*>(bar0);
    bcs_regs = reinterpret_cast<volatile uint32_t*>(mmio_base + BCS_RING_BASE);

    enable_force_wake();
    init_gtt();
    enable_bcs_power();
    reset_bcs();

    auto hwsp = alloc_and_map_to_ggtt(1, (1ULL << CacheDisabled));
    hwsp_cpu_addr = hwsp.cpu_addr;
    hwsp_graphics_addr = hwsp.gfx_addr;
    memset(hwsp_cpu_addr, 0, PAGE_SIZE);
    bcs_regs[BCS_HWSP / 4] = static_cast<uint32_t>(hwsp_graphics_addr & GTT_PHYS_ADDR_MASK);

    ring_size = RING_BUFFER_SIZE;
    uint32_t ring_pages = ring_size / PAGE_SIZE;
    auto ring = alloc_and_map_to_ggtt(ring_pages);

    ring_cpu_addr = ring.cpu_addr;
    ring_graphics_addr = ring.gfx_addr;
    memset(ring_cpu_addr, 0, ring_size);
    setup_ring_buffer();

    // Configure Ring Buffer registers
    bcs_regs[BCS_RING_START / 4] = static_cast<uint32_t>(ring_graphics_addr & GTT_PHYS_ADDR_MASK);

    uint32_t ring_ctl = ((ring_size - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
    bcs_regs[BCS_RING_CTL / 4] = ring_ctl;

    bcs_regs[BCS_RING_HEAD / 4] = 0;
    bcs_regs[BCS_RING_TAIL / 4] = 0;

    uint32_t head = bcs_regs[BCS_RING_HEAD / 4];
    uint32_t tail = bcs_regs[BCS_RING_TAIL / 4];
    uint32_t ctl = bcs_regs[BCS_RING_CTL / 4];

    if ((ctl & RING_CTL_ENABLED) && (head == tail))
    {
        Log::debug("BCS is READY!");
    }
    else
    {
        Log::Error("BCS initialization failed!");
    }
}

void IntelBlt::write_command(uint32_t cmd)
{
    uint32_t head = bcs_regs[BCS_RING_HEAD / 4];
    uint32_t available_space;

    if (ring_tail >= head)
    {
        available_space = (ring_size - ring_tail) + head;
    }
    else
    {
        available_space = head - ring_tail;
    }

    // 4 bytes + 64 bytes margin
    if (available_space < 68)
    {
        Log::Error("Ring buffer full! HEAD=0x%x TAIL=0x%x", head, ring_tail);
        if (!wait_for_ring_space(68, 100000))
        {
            // 100ms timeout
            Log::Error("Ring buffer deadlock!");
            return;
        }
    }

    volatile auto* ring = static_cast<volatile uint32_t*>(ring_cpu_addr);
    ring[ring_tail / 4] = cmd;

    __asm__ volatile("sfence" ::: "memory");

    ring_tail += 4;
    if (ring_tail >= ring_size)
    {
        ring_tail = 0;
    }
}

bool IntelBlt::wait_for_ring_space(uint32_t required_bytes, uint32_t timeout_us) const
{
    for (uint32_t i = 0; i < timeout_us; i++)
    {
        uint32_t head = bcs_regs[BCS_RING_HEAD / 4];
        uint32_t available;

        if (ring_tail >= head)
        {
            available = (ring_size - ring_tail) + head;
        }
        else
        {
            available = head - ring_tail;
        }

        if (available >= required_bytes)
        {
            return true;
        }

        // Micro-delay (1µs)
        for ( int j = 0; j < IDLE_CHECK_DELAY; j++);
    }
    return false;
}

bool IntelBlt::validate_blt_params(const BltRect& rect, const GpuFramebuffer* fb)
{
    if (!fb || !fb->cpu_addr)
    {
        Log::Error("Invalid framebuffer pointer");
        return false;
    }

    // Check dimensions
    if (rect.width == 0 || rect.height == 0)
    {
        Log::Error("Invalid rect dimensions: %dx%d", rect.width, rect.height);
        return false;
    }

    if (rect.width > 8192 || rect.height > 4096)
    {
        Log::Error("Rect too large: %dx%d (max 8192x4096)", rect.width, rect.height);
        return false;
    }

    // Check bounds
    if (rect.x + rect.width > fb->width || rect.y + rect.height > fb->height)
    {
        Log::Error("Rect out of bounds: (%d,%d)-(%d,%d) in %dx%d FB",
                   rect.x, rect.y, rect.x + rect.width, rect.y + rect.height,
                   fb->width, fb->height);
        return false;
    }

    // Check alignment
    if (fb->gfx_addr & 0x3F)
    {
        // 64-byte alignment
        Log::Error("Framebuffer not aligned: 0x%llx", fb->gfx_addr);
        return false;
    }

    // Check pitch
    if (fb->pitch & 0x3F)
    {
        // 64-byte alignment
        Log::Error("Pitch not aligned: %d", fb->pitch);
        return false;
    }

    return true;
}


void IntelBlt::xy_color_blt(
    uint64_t dest_addr,
    uint32_t dest_pitch,
    uint32_t x1, uint32_t y1,
    uint32_t x2, uint32_t y2,
    uint32_t color)
{
    // DW0: Command Header
    uint32_t dw0 = XY_COLOR_BLT_CMD |
        XY_COLOR_BLT_OPCODE |
        XY_COLOR_BLT_WRITE_RGB |
        XY_COLOR_BLT_WRITE_ALPHA |
        XY_COLOR_BLT_LEN;

    // DW1: BR13 - Format and ROP
    // Pitch should be in dwords. however when writing it in dwords, it only fills 1/4 of the buffer so we leave it in bytes lol
    // reference: https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02a-commandreference-instructions.pdf (page 1271)
    uint32_t dw1 = COLOR_DEPTH_8888 |
        PATCOPY |
        ((dest_pitch) & COORD_MASK);

    // DW2: BR22 - Top-Left coordinate (Y1, X1)
    uint32_t dw2 = ((y1 & COORD_MASK) << COORD_Y_SHIFT) | (x1 & COORD_MASK);

    // DW3: BR23 - Bottom-Right coordinate (Y2, X2)
    uint32_t dw3 = ((y2 & COORD_MASK) << COORD_Y_SHIFT) | (x2 & COORD_MASK);

    // DW4-5: Destination Base Address (64-bit)
    auto dw4 = static_cast<uint32_t>(dest_addr & 0xFFFFFFFF);
    auto dw5 = static_cast<uint32_t>(dest_addr >> 32);

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

void IntelBlt::emergency_reset_bcs()
{
    Log::Error("Emergency BCS reset initiated!");

    bcs_regs[BCS_RING_CTL / 4] &= ~RING_CTL_ENABLED;

    reset_bcs();

    memset(ring_cpu_addr, 0, ring_size);
    setup_ring_buffer();

    uint32_t ring_ctl = ((ring_size - PAGE_SIZE) & RING_SIZE_MASK) | RING_CTL_ENABLED;
    bcs_regs[BCS_RING_CTL / 4] = ring_ctl;
    bcs_regs[BCS_RING_HEAD / 4] = 0;
    bcs_regs[BCS_RING_TAIL / 4] = 0;
    ring_tail = 0;

    Log::Error("Emergency reset complete");
}

void IntelBlt::check_gpu_health()
{
    uint32_t head = bcs_regs[BCS_RING_HEAD / 4];
    uint32_t tail = bcs_regs[BCS_RING_TAIL / 4];
    uint32_t ctl = bcs_regs[BCS_RING_CTL / 4];

    // Check if ring is still enabled
    if (!(ctl & RING_CTL_ENABLED))
    {
        Log::Error("BCS ring disabled unexpectedly!");
        emergency_reset_bcs();
        return;
    }

    // Check for hung command
    static uint32_t last_head = 0;
    static uint32_t hang_counter = 0;

    if (head == last_head && head != tail)
    {
        hang_counter++;
        if (hang_counter > 1000)
        {
            // 1000 checks without progress
            Log::Error("GPU hang detected! HEAD stuck at 0x%x", head);
            emergency_reset_bcs();
            hang_counter = 0;
        }
    }
    else
    {
        hang_counter = 0;
    }
    last_head = head;
}

bool IntelBlt::fill_rect(BltRect rect,
                         uint32_t color, GpuFramebuffer* fb)
{
    if (!validate_blt_params(rect, fb))
    {
        return false;
    }

    check_gpu_health();

    uint32_t required = 12 * 4 + 64; // 12 DWords + margin
    if (!wait_for_ring_space(required, 1000000))
    {
        // 1s timeout
        Log::Error("Ring buffer full!");
        return false;
    }

    uint32_t x2 = rect.x + rect.width;
    uint32_t y2 = rect.y + rect.height;

    xy_color_blt(
        fb->gfx_addr,
        fb->pitch,
        rect.x, rect.y,
        x2, y2,
        color
    );

    mi_flush();
    flush_commands();

    if (!wait_idle(5000000)) // 5 second timeout
    {
        Log::Error("Timeout waiting for BLT to complete!");
        check_gpu_health();
        return false;
    }

    __asm__ volatile("mfence" ::: "memory");

    return true;
}

GpuFramebuffer IntelBlt::alloc_framebuffer(uint32_t width, uint32_t height, TileMode tile_mode)
{
    GpuFramebuffer fb{};
    fb.width = width;
    fb.height = height;
    fb.bpp = 4; // 32-bit ARGB
    fb.tile_mode = tile_mode;

    fb.pitch = ((width * fb.bpp + 127) / 128) * 128;

    size_t total_size = fb.pitch * height;

    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    Log::debug("Allocating framebuffer: %dx%d, pitch=%d, size=%zu bytes, pages=%zu, tile_mode=%u",
               width, height, fb.pitch, total_size, num_pages, static_cast<uint32_t>(tile_mode));

    auto allocation = alloc_and_map_to_ggtt(num_pages,
                                            (1ULL << CacheDisabled),
                                            MOCS_UNCACHED);

    fb.gfx_addr = allocation.gfx_addr;
    fb.cpu_addr = allocation.cpu_addr;

    memset(fb.cpu_addr, 0, total_size);

    return fb;
}


bool IntelBlt::wait_idle(uint32_t timeout_us) const
{
    uint32_t initial_head = bcs_regs[BCS_RING_HEAD / 4];
    uint32_t initial_tail = bcs_regs[BCS_RING_TAIL / 4];

    Log::debug("Waiting for idle: HEAD=0x%x, TAIL=0x%x", initial_head, initial_tail);

    for (uint32_t i = 0; i < timeout_us; i++)
    {
        uint32_t head = bcs_regs[BCS_RING_HEAD / 4];
        uint32_t tail = bcs_regs[BCS_RING_TAIL / 4];

        if (head == tail)
        {
            return true;
        }

        // 1µs delay
        for (int j = 0; j < IDLE_CHECK_DELAY; j++);
    }

    uint32_t final_head = bcs_regs[BCS_RING_HEAD / 4];
    uint32_t final_tail = bcs_regs[BCS_RING_TAIL / 4];
    uint32_t ring_ctl = bcs_regs[BCS_RING_CTL / 4];

    Log::Error("BCS timeout! Initial HEAD=0x%x TAIL=0x%x, Final HEAD=0x%x TAIL=0x%x",
               initial_head, initial_tail, final_head, final_tail);
    Log::Error("  Ring CTL=0x%x (enabled=%d)",
               ring_ctl, ring_ctl & RING_CTL_ENABLED);

    return false;
}

void IntelBlt::mi_flush()
{
    constexpr uint32_t dw0 = MI_COMMAND_TYPE |
        MI_FLUSH_DW_OPCODE |
        MI_FLUSH_STORE_INDEX |
        MI_FLUSH_POST_SYNC |
        MI_FLUSH_DW_LEN;

    write_command(dw0);
    write_command(HWSP_FLUSH_OFFSET);
    write_command(0);
    write_command(HWSP_FLUSH_MARKER);
    write_command(0);
}

void IntelBlt::flush_commands() const
{
    __asm__ volatile("mfence" ::: "memory");
    bcs_regs[BCS_RING_TAIL / 4] = ring_tail;
}

GgttAllocation IntelBlt::alloc_and_map_to_ggtt(size_t num_pages, uint64_t flags, uint8_t pat_index)
{
    void* cpu = kernel::memory::request_pages(num_pages);

    kernel::memory::map_range(
        cpu,
        cpu,
        num_pages * 4096,
        flags
    );

    const auto phys = reinterpret_cast<uint64_t>(cpu);
    uint64_t gfx = map_to_ggtt(phys, num_pages, pat_index);

    return {cpu, gfx};
}

void IntelBlt::setup_ring_buffer()
{
    ring_tail = 0;

    Log::debug("Ring Buffer: CPU=%p GFX=0x%llx",
               ring_cpu_addr, ring_graphics_addr);

    volatile auto* ring = static_cast<volatile uint32_t*>(ring_cpu_addr);
    for (uint32_t i = 0; i < ring_size / 4; i++)
    {
        ring[i] = MI_NOOP; // MI_NOOP
    }

    Log::debug("Ring Buffer cleared with NOOPs");
}

void IntelBlt::enable_force_wake() const
{
    volatile auto* forcewake_mt =
        reinterpret_cast<volatile uint32_t*>(mmio_base + FORCEWAKE_MT);
    volatile auto* forcewake_ack =
        reinterpret_cast<volatile uint32_t*>(mmio_base + FORCEWAKE_ACK);

    *forcewake_mt = FORCEWAKE_ENABLE;

    // Wait for acknowledgment
    int timeout = FORCE_WAKE_TIMEOUT;
    while (timeout-- > 0)
    {
        if (*forcewake_ack & GTT_VALID)
        {
            Log::debug("Force Wake ACK received");
            break;
        }
        for (int i = 0; i < IDLE_CHECK_DELAY; i++);
    }

    if (timeout <= 0)
    {
        Log::Error("Force Wake timeout!");
    }
}

void IntelBlt::enable_bcs_power() const
{
    auto bcs_swctrl =
        reinterpret_cast<volatile uint32_t*>(mmio_base + BCS_SWCTRL);

    *bcs_swctrl |= BCS_SWCTRL_WAKEUP;

    Log::debug("BCS Power enabled");
}

void IntelBlt::reset_bcs() const
{
    auto reset_ctl =
        reinterpret_cast<volatile uint32_t*>(mmio_base + RESET_CTL);

    // Request BCS reset
    *reset_ctl |= RESET_BCS_BIT;

    for (int i = 0; i < RESET_DELAY; i++) {}

    // Release reset
    *reset_ctl &= ~RESET_BCS_BIT;

    for (int i = 0; i < RESET_DELAY; i++) {}

    Log::debug("BCS Reset completed");
}

void IntelBlt::init_gtt()
{
    gtt_entries = reinterpret_cast<volatile uint64_t*>(mmio_base + GTT_OFFSET);
    gtt_total_entries = GTT_TOTAL_ENTRIES;
    gtt_next_free = GTT_START_INDEX;
}

uint64_t IntelBlt::map_to_ggtt(uint64_t phys_addr, size_t num_pages, uint8_t pat_index)
{
    if (gtt_next_free + num_pages > gtt_total_entries)
    {
        Log::Error("GGTT out of space!");
        return 0;
    }

    uint32_t gtt_index = gtt_next_free;

    for (size_t i = 0; i < num_pages; i++)
    {
        uint64_t page_phys = phys_addr + (i * PAGE_SIZE);

        uint64_t gtt_entry = (page_phys & GTT_PHYS_ADDR_MASK);
        gtt_entry |= (static_cast<uint64_t>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
        gtt_entry |= GTT_VALID;

        gtt_entries[gtt_index + i] = gtt_entry;
        __asm__ volatile("mfence" ::: "memory");
    }

    uint64_t graphics_addr = static_cast<uint64_t>(gtt_index) * PAGE_SIZE;
    gtt_next_free += num_pages;

    return graphics_addr;
}


// https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol02c-commandreference-registers-part2_0.pdf (page 604)
void IntelBlt::set_display_framebuffer(uint64_t gfx_addr, uint32_t width,
                                       uint32_t height, uint32_t pitch, TileMode tile_mode) const
{
    auto plane_regs = reinterpret_cast<volatile uint32_t*>(mmio_base);

    // Disable plane
    uint32_t plane_ctl = plane_regs[PLANE_CTL_1A / 4];
    plane_regs[PLANE_CTL_1A / 4] = plane_ctl & ~PLANE_CTL_ENABLE;

    for (int i = 0; i < 1000; i++) {}

    uint32_t stride_value;
    switch (tile_mode)
    {
    case TileMode::Linear:
        // Linear: stride in chunks of 64 bytes
        stride_value = pitch / 64;
        plane_ctl = PLANE_CTL_PIPE_A | PLANE_CTL_FORMAT_XRGB8888;
        break;

    case TileMode::X:
        stride_value = pitch / 512;
        plane_ctl = PLANE_CTL_PIPE_A |
            PLANE_CTL_FORMAT_XRGB8888 |
            (0b001 << 10); // Tile X format
        break;

    case TileMode::Y:
        stride_value = pitch / 128;
        plane_ctl = PLANE_CTL_PIPE_A |
            PLANE_CTL_FORMAT_XRGB8888 |
            PLANE_CTL_TILE_Y; // 0b100 << 10
        break;

    default:
        Log::Error("Unsupported tile mode!");
        return;
    }

    plane_regs[PLANE_STRIDE_1A / 4] = stride_value & 0x3FF;

    // Set size (width-1, height-1)
    uint32_t size = ((height - 1) << 16) | (width - 1);
    plane_regs[PLANE_SIZE_1A / 4] = size;

    // Set position (0, 0 for fullscreen)
    plane_regs[PLANE_POS_1A / 4] = 0;
    plane_regs[PLANE_OFFSET_1A / 4] = 0;

    plane_ctl =
        PLANE_CTL_PIPE_A |
        PLANE_CTL_FORMAT_XRGB8888;

    plane_regs[PLANE_CTL_1A / 4] = plane_ctl;

    plane_regs[PLANE_SURF_1A / 4] = gfx_addr;

    plane_regs[PLANE_CTL_1A / 4] = plane_ctl | PLANE_CTL_ENABLE;

    __asm__ volatile("mfence" ::: "memory");

    Log::debug("Display framebuffer updated: addr=0x%llx, %dx%d, pitch=%d",
               gfx_addr, width, height, pitch);
}

void IntelBlt::set_display_framebuffer(const GpuFramebuffer& fb) const
{
    set_display_framebuffer(fb.gfx_addr, fb.width, fb.height, fb.pitch, fb.tile_mode);
}


bool IntelBlt::init() { return true; }

void IntelBlt::fill(uint32_t color)
{
}

void IntelBlt::copy(BltRect src, BltRect dst)
{
}

void IntelBlt::flush()
{
    mi_flush();
    flush_commands();
}
