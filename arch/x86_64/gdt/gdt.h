//
// Created by linus on 04.10.24.
//

#ifndef GDT_H
#define GDT_H
#include <stdint.h>

struct GDTDescriptor {
    uint16_t size;
    uint64_t offset;
}__attribute__((packed));

struct __attribute__((packed)) GDTEntry {
    uint16_t limit_low;       // Limit bits 0-15
    uint16_t base_low;        // Base bits 0-15
    uint8_t  base_middle;     // Base bits 16-23
    uint8_t  access;          // Access byte
    uint8_t  granularity;     // Flags + Limit bits 16-19
    uint8_t  base_high;       // Base bits 24-31
};

// TSS Descriptor (16 bytes)
struct __attribute__((packed)) TSSDescriptor {
    uint16_t limit_low;       // Limit bits 0-15
    uint16_t base_low;        // Base bits 0-15
    uint8_t  base_middle;     // Base bits 16-23
    uint8_t  access;          // Access byte
    uint8_t  granularity;     // Flags + Limit bits 16-19
    uint8_t  base_high;       // Base bits 24-31
    uint32_t base_upper;      // Base bits 32-63
    uint32_t reserved;        // Must be zero
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

struct GDT {
    GDTEntry Null;  // 0x00
    GDTEntry KernelCode; // 0x08
    GDTEntry KernelData; // 0x10
    GDTEntry UserNull; 
    GDTEntry UserCode;
    GDTEntry UserData;
    uint64_t TSS_Low;
    uint64_t TSS_High;
}__attribute__((packed)) __attribute__((aligned(0x1000)));

// GDT Entries
#define GDT_ENTRIES 7
static GDTEntry gdt[GDT_ENTRIES];
static TSSDescriptor tss_desc;
static TSS tss;

// GDTR (GDT Register)
struct __attribute__((packed)) GDTPtr {
    uint16_t limit;
    uint64_t base;
};
static GDTPtr gdt_ptr;

void gdt_set_tss(TSS* tss);

extern GDT default_gdt;

void gdt_install();

extern "C" void load_GDT(GDTPtr*);

#endif //GDT_H