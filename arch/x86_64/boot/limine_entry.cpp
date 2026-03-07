// limine_entry.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 24.02.26.
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

#include <limine.h>
#include <vespera/boot/boot.h>
#include <vespera/graphics.h>

__attribute__((used, section(".requests_start_marker"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests"))) static volatile limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0, .response = nullptr
};

__attribute__((used, section(".requests"))) static volatile limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0, .response = nullptr
};

__attribute__((used, section(".requests"))) static volatile limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST, .revision = 0, .response = nullptr
};

__attribute__((used, section(".requests"))) static volatile limine_kernel_address_request kaddr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0, .response = nullptr
};

__attribute__((used, section(".requests"))) static volatile limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0, .response = nullptr
};

__attribute__((used, section(".requests_end_marker"))) static volatile LIMINE_REQUESTS_END_MARKER;

extern "C" {
extern u8 psf_font_start[];
extern u8 psf_font_end[];
extern u8 psf_font_size[];
}

static framebuffer_t framebuffer;
static BootInfo boot_info;
static font_t embedded_font;
static EFI_MEMORY_DESCRIPTOR efi_map[512];

[[noreturn]] static void hlt_forever() {
    while (true) asm volatile("cli; hlt");
}

static void parse_psf_font() {
    u8* data = psf_font_start;

    // PSF1: Magic = 0x36 0x04
    if (data[0] == 0x36 && data[1] == 0x04) {
        auto* hdr = reinterpret_cast<psf1_header_t*>(data);
        embedded_font.header = hdr;
        embedded_font.glyph_buffer = data + sizeof(psf1_header_t);
        embedded_font.type = 1;
        embedded_font.width = 8;
        embedded_font.height = hdr->charsize;
        embedded_font.charsize = hdr->charsize;
        return;
    }

    // PSF2: Magic = 0x72 0xb5 0x4a 0x86
    if (data[0] == 0x72 && data[1] == 0xb5 && data[2] == 0x4a && data[3] == 0x86) {
        auto* hdr = reinterpret_cast<psf2_header_t*>(data);
        embedded_font.header = hdr;
        embedded_font.glyph_buffer = data + hdr->headersize;
        embedded_font.type = 2;
        embedded_font.width = hdr->width;
        embedded_font.height = hdr->height;
        embedded_font.charsize = hdr->charsize;
    }
}

static void convert_memmap(u64* out_count) {
    *out_count = 0;
    if (!memmap_request.response) return;

    auto* resp = memmap_request.response;
    u64 n = 0;

    for (u64 i = 0; i < resp->entry_count && n < 512; i++) {
        limine_memmap_entry* src = resp->entries[i];
        EFI_MEMORY_DESCRIPTOR* dst = &efi_map[n];

        switch (src->type) {
            case LIMINE_MEMMAP_USABLE:
                dst->type = 7;  // EfiConventionalMemory
                break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                dst->type = 4;  // EfiBootServicesData
                break;
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
                dst->type = 9;  // EfiACPIReclaimMemory
                break;
            case LIMINE_MEMMAP_ACPI_NVS:
                dst->type = 10;  // EfiACPIMemoryNVS
                break;
            case LIMINE_MEMMAP_FRAMEBUFFER:
                dst->type = 11;  // EfiMemoryMappedIO
                break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                dst->type = 1;  // EfiLoaderCode
                break;
            case LIMINE_MEMMAP_RESERVED:
            default:
                dst->type = 0;  // EfiReservedMemoryType
                break;
        }

        // Limine gibt immer physische Basisadressen in der Memory Map
        dst->phys_addr = src->base;
        dst->virt_addr = src->base;
        dst->num_pages = src->length / 4096;
        dst->attribs = 0;

        n++;
    }

    *out_count = n;
}

// ============================================================================
// Kernel Entry Point
// ============================================================================

extern "C" void kernel_main(BootInfo* info);

extern "C" void limine_entry() {
    // --- Framebuffer ---
    if (!fb_request.response || fb_request.response->framebuffer_count == 0) hlt_forever();

    limine_framebuffer* lfb = fb_request.response->framebuffers[0];

    framebuffer.base_address = lfb->address;
    framebuffer.buffer_size = lfb->pitch * lfb->height;
    framebuffer.width = lfb->width;
    framebuffer.height = lfb->height;
    framebuffer.pixels_per_scanline = lfb->pitch / (lfb->bpp / 8);

    // Physische Adresse des Framebuffers für späteres Mapping in init.cpp
    // phys = virt - hhdm_offset (wird unten gesetzt)
    framebuffer.phys_base_address = reinterpret_cast<u64>(lfb->address);

    boot_info.framebuffer = &framebuffer;

    // --- Memory Map ---
    u64 map_count = 0;
    convert_memmap(&map_count);
    boot_info.m_map = efi_map;
    boot_info.m_map_size = map_count * sizeof(EFI_MEMORY_DESCRIPTOR);
    boot_info.m_map_desc_size = sizeof(EFI_MEMORY_DESCRIPTOR);

    // --- HHDM Offset ---
    // Alle von Limine zurückgegebenen Pointer (außer Memory-Map-Basisadressen)
    // sind HHDM-virtuell. phys = virt - hhdm_offset.
    if (hhdm_request.response)
        boot_info.hhdm_offset = hhdm_request.response->offset;
    else
        boot_info.hhdm_offset = 0;

    // Jetzt phys_base_address korrekt setzen
    framebuffer.phys_base_address = reinterpret_cast<u64>(lfb->address) - boot_info.hhdm_offset;

    // --- RSDP ---
    // rsdp_request.response->address ist ebenfalls HHDM-virtuell
    if (rsdp_request.response)
        boot_info.rsdp = static_cast<acpi::RSDP2*>(rsdp_request.response->address);
    else
        boot_info.rsdp = nullptr;

    // --- Kernel-Adressen ---
    // Wichtig für KERNEL_BASE-Berechnungen in init.cpp
    if (kaddr_request.response) {
        boot_info.kernel_phys_base = kaddr_request.response->physical_base;
        boot_info.kernel_virt_base = kaddr_request.response->virtual_base;
    } else {
        // Fallback: linker.ld Werte
        boot_info.kernel_phys_base = 0x100000;
        boot_info.kernel_virt_base = 0xFFFFFFFF80100000ULL;
    }

    // --- Font ---
    parse_psf_font();
    boot_info.font = &embedded_font;

    // --- Kernel starten ---
    kernel_main(&boot_info);

    hlt_forever();
}