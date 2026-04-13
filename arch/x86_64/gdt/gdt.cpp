#include "gdt.h"

#include <klib/string.h>

#include "../../../include/acpi/madt.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "vespera/log.h"

GDT_ENTRY gdt[GDT_ENTRIES + (kernel::acpi::madt::MAX_CPU_CORES * 2)];
TSS_DESCRIPTOR tss_desc;
TSS tss[kernel::acpi::madt::MAX_CPU_CORES] __attribute__((aligned(4096)));
GDT_PTR gdt_ptr;

static void set_gdt_entry(const int idx, const u32 base, const u32 limit, const u8 access, const u8 gran) {
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].base_low = base & 0xFFFF;
    gdt[idx].base_middle = (base >> 16) & 0xFF;
    gdt[idx].access = access;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].base_high = (base >> 24) & 0xFF;
}

void setup_cpu_tss(u32 cpu_id) {
    if (cpu_id >= kernel::acpi::madt::MAX_CPU_CORES) return;

    memset(&tss[cpu_id], 0, sizeof(TSS));

    uptr kernel_stack = cpu_manager::get_cpu_info(cpu_id)->kernel_stack_top;
    tss[cpu_id].rsp0 = kernel_stack;
    tss[cpu_id].iomap_base = sizeof(TSS);

    // Index: 5 + (cpu_id * 2) weil TSS 16 bytes = 2 GDT entries braucht
    u32 gdt_index = 5 + (cpu_id * 2);

    TSS_DESCRIPTOR temp_desc;
    memset(&temp_desc, 0, sizeof(TSS_DESCRIPTOR));

    auto base = reinterpret_cast<u64>(&tss[cpu_id]);
    u32 limit = sizeof(TSS) - 1;

    temp_desc.limit_low = limit & 0xFFFF;
    temp_desc.base_low = base & 0xFFFF;
    temp_desc.base_middle = (base >> 16) & 0xFF;
    temp_desc.access = 0x89;
    temp_desc.granularity = ((limit >> 16) & 0x0F);
    temp_desc.base_high = (base >> 24) & 0xFF;
    temp_desc.base_upper = (base >> 32) & 0xFFFFFFFF;
    temp_desc.reserved = 0;

    memcpy(&gdt[gdt_index], &temp_desc, sizeof(TSS_DESCRIPTOR));

    u16 tss_selector = (gdt_index << 3);  // Index * 8
    asm volatile("ltr %w0" ::"r"(tss_selector));
}

u16 get_tss_selector(const u32 cpu_id) {
    return (5 + (cpu_id * 2)) << 3;
}

void tss_set_rsp0(u8 cpu_id, u64 rsp0) {
    if (cpu_id >= kernel::acpi::madt::MAX_CPU_CORES) return;

    if (rsp0 & 0xF) {
        Log::warning("tss_set_rsp0: stack not aligned");
    }

    tss[cpu_id].rsp0 = rsp0;
}

void gdt_install() {
    memset(&gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    // Null descriptor
    set_gdt_entry(0, 0, 0, 0, 0);

    // Kernel code segment
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // Kernel data segment
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xA0);

    // User data segment
    set_gdt_entry(3, 0, 0xFFFFF, 0xF2, 0xA0);

    // User code segment
    set_gdt_entry(4, 0, 0xFFFFF, 0xFA, 0xA0);

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = reinterpret_cast<u64>(&gdt);

    load_gdt(&gdt_ptr);
}