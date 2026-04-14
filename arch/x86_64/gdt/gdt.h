//
// Created by linus on 04.10.24.
//

#ifndef GDT_H
#define GDT_H
#include <vespera/types.h>

#include <acpi/madt.h>

struct __attribute__((packed)) GDT_ENTRY {
    u16 limit_low;   // Limit bits 0-15
    u16 base_low;    // Base bits 0-15
    u8 base_middle;  // Base bits 16-23
    u8 access;       // Access byte
    u8 granularity;  // Flags + Limit bits 16-19
    u8 base_high;    // Base bits 24-31
};

// TSS Descriptor (16 bytes)
struct __attribute__((packed)) TSS_DESCRIPTOR {
    u16 limit_low;   // Limit bits 0-15
    u16 base_low;    // Base bits 0-15
    u8 base_middle;  // Base bits 16-23
    u8 access;       // Access byte
    u8 granularity;  // Flags + Limit bits 16-19
    u8 base_high;    // Base bits 24-31
    u32 base_upper;  // Base bits 32-63
    u32 reserved;    // Must be zero
};

struct __attribute__((packed)) TSS {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
};

// GDTR (GDT Register)
struct __attribute__((packed)) GDT_PTR {
    u16 limit;
    u64 base;
};

// GDT Entries
#define GDT_ENTRIES 7
extern GDT_ENTRY gdt[GDT_ENTRIES + (kernel::acpi::madt::MAX_CPU_CORES * 2)];
extern TSS_DESCRIPTOR tss_desc;
extern TSS tss[kernel::acpi::madt::MAX_CPU_CORES];
extern GDT_PTR gdt_ptr;

void setup_cpu_tss(u32 cpu_id);
void tss_set_rsp0(u8 cpu_id, u64 rsp0);

void gdt_install();

extern "C" void load_gdt(GDT_PTR *);

#endif  // GDT_H
