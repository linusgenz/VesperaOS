#include "gdt.h"

#include "../../../include/log.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "../../../kernel/include/interrupts.h"
#include "../../../kernel/include/memory.h"

GDTEntry gdt[GDT_ENTRIES];
TSSDescriptor tss_desc;
TSS tss __attribute__((aligned(4096)));
GDTPtr gdt_ptr;

static void set_gdt_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].base_low = base & 0xFFFF;
    gdt[idx].base_middle = (base >> 16) & 0xFF;
    gdt[idx].access = access;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].base_high = (base >> 24) & 0xFF;
}

static void set_tss_descriptor(TSS *tss_ptr) {
    uint64_t base = (uint64_t) tss_ptr;
    uint32_t limit = sizeof(TSS) - 1;

    tss_desc.limit_low = limit & 0xFFFF;
    tss_desc.base_low = base & 0xFFFF;
    tss_desc.base_middle = (base >> 16) & 0xFF;
    tss_desc.access = 0x89; // Present, DPL=0, Type=9 (available 64-bit TSS)
    tss_desc.granularity = ((limit >> 16) & 0x0F);
    tss_desc.base_high = (base >> 24) & 0xFF;
    tss_desc.base_upper = (base >> 32) & 0xFFFFFFFF;
    tss_desc.reserved = 0;
}

void gdt_install() {
    memset(&gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    // Null descriptor
    set_gdt_entry(0, 0, 0, 0, 0);

    // Kernel code segment: base=0, limit=4GB, Access=0x9A, Granularity=0xA0 (L=1, G=1)
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // Kernel data segment: base=0, limit=4GB, Access=0x92, Granularity=0xA0
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xA0);

    // User code segment: base=0, limit=4GB, Access=0xFA (DPL=3), Granularity=0xA0
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    // User data segment: base=0, limit=4GB, Access=0xF2 (DPL=3), Granularity=0xA0
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);

    // TSS Descriptor (uses 2 GDT entries: 5 and 6)

    set_tss_descriptor(&tss);
    // Kopiere TSS Descriptor in GDT (gdt[5] und gdt[6])
    memcpy(&gdt[5], &tss_desc, sizeof(TSSDescriptor));

    // Initialize TSS rsp0 stack pointer (z.B. kernel stack)
    // TODO cpu manager muss init sein damit wir fetchen können
  //  uintptr_t kernel_stack_top = CPUManager::get_cpu_info(kernel::interrupts::lapic_get_id())->kernel_stack_top;
    int rsp0_addr = 0x20000;
    kernel::memory::map_memory((void*)rsp0_addr, (void*)rsp0_addr);
    tss.rsp0 = rsp0_addr; // kernel_stack_top; TODO
    tss.rsp1 = 0xDEADBEEF;

    //Log::PrintLn("stack top: %p", kernel_stack_top);

    // Set IO Map base beyond TSS size (kein IO Bitmap)
    tss.iomap_base = sizeof(TSS);
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint64_t) &gdt;

    // Lade GDT in CPU
    load_GDT(&gdt_ptr);

    // Lade TSS
    asm volatile ("ltr %w0" : : "r" (5 << 3)); // Selector: Index 5 (TSS), RPL=0
}
