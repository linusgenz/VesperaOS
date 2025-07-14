//
// Created by linus on 04.10.24.
//

#ifndef GDT_H
#define GDT_H
#include <stdint.h>
#include "../tss.h"

struct GDTDescriptor {
    uint16_t size;
    uint64_t offset;
}__attribute__((packed));

struct GDTEntry {
    uint16_t Limit0;
    uint16_t Base0;
    uint8_t Base1;
    uint8_t AccessByte;
    uint8_t Limit1_Flags;
    uint8_t Base2;
}__attribute__((packed));

struct GDT {
    GDTEntry Null;  // 0x00
    GDTEntry KernelCode; // 0x08
    GDTEntry KernelData; // 0x10
    GDTEntry UserNull; 
    GDTEntry UserCode;
    GDTEntry UserData;
 //   GDTEntry TSS_Low;
 //   GDTEntry TSS_High;
}__attribute__((packed)) __attribute__((aligned(0x1000)));

void gdt_set_tss(TSS* tss);

extern GDT default_gdt;

extern "C" void load_GDT(GDTDescriptor* gdt_descriptor);

#endif //GDT_H