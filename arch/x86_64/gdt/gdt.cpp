#include "gdt.h"

__attribute__((aligned(0x1000)))
GDT default_gdt = {
    {0, 0, 0, 0x00, 0x00, 0}, // null
    {0, 0, 0, 0x9a, 0xa0, 0}, // kernel code segment
    {0, 0, 0, 0x92, 0xa0, 0}, // kernel data segment
    {0, 0, 0, 0x00, 0x00, 0}, // user null
    {0, 0, 0, 0x9a, 0xa0, 0}, // user code segment
    {0, 0, 0, 0x92, 0xa0, 0}, // user data segment
   // {0, 0, 0, 0x9a, 0xa0, 0}, // user code segment
   // {0, 0, 0, 0x92, 0xa0, 0}, // user data segment
};

/*
void gdt_set_tss(TSS* tss) {
    const auto base = (uint64_t)tss;
    constexpr uint32_t limit = sizeof(struct TSS);

    default_gdt.TSS_Low = (GDTEntry){
        .Limit0 = static_cast<uint16_t>(limit & 0xFFFF),
        .Base0 = static_cast<uint16_t>(base & 0xFFFF),
        .Base1 = static_cast<uint8_t>((base >> 16) & 0xFF),
        .AccessByte = 0x89,
        .Limit1_Flags = static_cast<uint8_t>(((limit >> 16) & 0x0F) | 0x00),
        .Base2 = static_cast<uint8_t>((base >> 24) & 0xFF),
    };

    const uint32_t base_high = (base >> 32) & 0xFFFFFFFF;
    default_gdt.TSS_High = (GDTEntry){
        .Limit0 = static_cast<uint16_t>(base_high & 0xFFFF),
        .Base0 = static_cast<uint16_t>((base_high >> 16) & 0xFFFF),
        .Base1 = 0,
        .AccessByte = 0,
        .Limit1_Flags = 0,
        .Base2 = 0,
    };
}*/

