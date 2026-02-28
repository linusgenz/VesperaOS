//
// Created by linus on 04.10.24.
//

#ifndef GDT_H
#define GDT_H
#include "../../../kernel/acpi/madt.h"
#include <cstdint>

struct __attribute__((packed)) GDTEntry {
    uint16_t limit_low;   // Limit bits 0-15
    uint16_t base_low;    // Base bits 0-15
    uint8_t base_middle;  // Base bits 16-23
    uint8_t access;       // Access byte
    uint8_t granularity;  // Flags + Limit bits 16-19
    uint8_t base_high;    // Base bits 24-31
};

// TSS Descriptor (16 bytes)
struct __attribute__((packed)) TSSDescriptor {
    uint16_t limit_low;   // Limit bits 0-15
    uint16_t base_low;    // Base bits 0-15
    uint8_t base_middle;  // Base bits 16-23
    uint8_t access;       // Access byte
    uint8_t granularity;  // Flags + Limit bits 16-19
    uint8_t base_high;    // Base bits 24-31
    uint32_t base_upper;  // Base bits 32-63
    uint32_t reserved;    // Must be zero
};

struct __attribute__((packed)) TSS {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

// GDTR (GDT Register)
struct __attribute__((packed)) GDTPtr {
    uint16_t limit;
    uint64_t base;
};

// GDT Entries
#define GDT_ENTRIES 7
extern GDTEntry gdt[GDT_ENTRIES + (MAX_CPU_CORES * 2)];
extern TSSDescriptor tss_desc;
extern TSS tss[MAX_CPU_CORES];
extern GDTPtr gdt_ptr;

void setup_cpu_tss(uint32_t cpu_id);

void gdt_install();

extern "C" void load_GDT(GDTPtr *);

#endif  // GDT_H
