#include "./include/kernel_utils.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/interrupts/idt.h"
#include "../arch/x86_64/interrupts/interrupts.h"
#include "../arch/x86_64/interrupts/apic.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"

void prepare_memory(BootInfo* bootInfo){
    const uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

    global_allocator = PageFrameAllocator();

    global_allocator.read_efi_memory_map(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

    const uint64_t kernelStart = reinterpret_cast<uint64_t>(&_KernelStart);
    const uint64_t kernelEnd = reinterpret_cast<uint64_t>(&_KernelEnd);
    const uint64_t kernelSize = kernelEnd - kernelStart;
    const uint64_t kernelPages = kernelSize / 4096 + 1;

    global_allocator.lock_pages(&_KernelStart, kernelPages);
    global_renderer->print(to_string(kernelPages));
    PageTable* PML4 = (PageTable*)global_allocator.request_page();
    memset(PML4, 0, 0x1000);

    global_page_table_manager = PageTableManager(PML4);

  /*  for (uint64_t addr = kernelStart; addr < kernelEnd; addr += 0x1000) {
        global_page_table_manager.map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
    }*/
    // just map everythin cuz it works lol. might not be a good practice tho, needs refactoring prob
    for (int i = 0; i < mMapEntries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)bootInfo->mMap + (i * bootInfo->mMapDescSize));
      //  if (desc->type != 7) continue; // Nur EfiConventionalMemory

        for (uint64_t addr = desc->phys_addr; addr < desc->phys_addr + desc->num_pages * 0x1000; addr += 0x1000) {
            global_page_table_manager.map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
        }
    }

    uint64_t fb_base = (uint64_t)bootInfo->framebuffer->base_address;
    uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;
    global_allocator.lock_pages((void*)fb_base, fb_size/ 0x1000 + 1);
    for (uint64_t t = fb_base; t < fb_base + fb_size; t += 0x1000){
        global_page_table_manager.map_memory((void*)t, (void*)t);
    }

    const char* address_str = to_hstring(PML4);
    asm ("mov %0, %%cr3" : : "r" (PML4));
}

IDTR idtr;
void set_idt_gate(void* handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector) {
    IDTDescEntry* interrupt = (IDTDescEntry*)(idtr.offset + entry_offset * sizeof(IDTDescEntry));
    interrupt->set_offset((uint64_t)handler);
    interrupt->selector = selector;
    interrupt->ist = 0;
    interrupt->type_attr = type_attr;
    interrupt->ignore = 0;
}

void prepare_interrupts() {
    global_page_table_manager.map_memory((void*)LAPIC_ADDRESS, (void*)LAPIC_ADDRESS);

    void* idt_page = global_allocator.request_page();
    memset(idt_page, 0, 0x1000);

    idtr.limit = 0x0FFF;
    idtr.offset = reinterpret_cast<uint64_t>(idt_page);

    set_idt_gate((void*)page_fault_handler, 0x0E, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)double_fault_handler, 0x08, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)gp_fault_handler, 0x0D, IDT_TA_InterruptGate, 0x08);
  //  set_idt_gate((void*)keyboard_int_handler, 0x21, IDT_TA_InterruptGate, 0x08);
   // set_idt_gate((void*)mouse_int_handler, 0x2C, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)apic_timer_int_handler, IRQ_TIMER, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)spurious_int_handler, IRQ_SPURIOUS, IDT_TA_InterruptGate, 0x08);
    asm ("lidt %0" : : "m" (idtr));
    asm ("sti");
}

uint32_t* scroll_buffer_top = nullptr;
uint32_t* scroll_buffer_bottom = nullptr;
void setup_scroll_buffer()
{
    uint64_t buffer_size = global_renderer->TargetFramebuffer->width * global_renderer->TargetFramebuffer->height * sizeof(uint32_t) * 5;
    scroll_buffer_top = (uint32_t*)malloc(buffer_size);
    if (!scroll_buffer_top) {
        global_renderer->print("Failed to allocate scroll buffer (top)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_top, 0, buffer_size);


    scroll_buffer_bottom = (uint32_t*)malloc(buffer_size);
    if (!scroll_buffer_bottom) {
        global_renderer->print("Failed to allocate scroll buffer (bot)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_bottom, 0, buffer_size);
}

void prepare_acpi(BootInfo* boot_info) {
    ACPI::SDTHeader* xsdt = reinterpret_cast<ACPI::SDTHeader *>(boot_info->rsdp->xsdt_address);
    ACPI::SDTHeader* rsdt = reinterpret_cast<ACPI::SDTHeader *>(boot_info->rsdp->rsdt_address);

    ACPI::TableManager::init(xsdt);

    ACPI::TableManager::register_madt();
    ACPI::TableManager::register_mcfg();
    ACPI::TableManager::register_fadr();

    MADT::parse_madt(ACPI::TableManager::get_madt());

}

ScrollManager s = ScrollManager(NULL, NULL, NULL);
BasicRenderer r = BasicRenderer(NULL, NULL);
void initialize_kernel(BootInfo* bootInfo){
    r = BasicRenderer(bootInfo->framebuffer, bootInfo->psf1_font);
    global_renderer = &r;
    memset(bootInfo->framebuffer->base_address, 0, bootInfo->framebuffer->buffer_size);

    GDTDescriptor gdt_descriptor;
    gdt_descriptor.size = sizeof(GDT) - 1;
    gdt_descriptor.offset = (uint64_t)&default_gdt;
    load_GDT(&gdt_descriptor);

    prepare_memory(bootInfo);
    initialize_heap((void*)0x0000100000000000, 0x500);

    prepare_acpi(bootInfo);

    prepare_interrupts();

    pic_init();
    lapic_init();

    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    global_renderer->print("free mem: ");
    global_renderer->print(to_string(global_allocator.get_free_ram() / 1024 / 1024));
    global_renderer->print(" mb");
    global_renderer->new_line();
    global_renderer->print("reserved mem: ");
    global_renderer->print(to_string(global_allocator.get_reserved_ram() / 1024 / 1024));
    global_renderer->print(" mb");
    global_renderer->new_line();
    global_renderer->print("used mem: ");
    global_renderer->print(to_string(global_allocator.get_used_ram() / 1024 / 1024));
    global_renderer->print(" mb");
    global_renderer->new_line();

   /* uint32_t svr_test = lapic_read(LAPIC_SVR);
    if (!(svr_test & 0x100 | IRQ_SPURIOUS)) {
        global_renderer->print("LAPIC NICHT AKTIVIERT");
    }
    else {
        global_renderer->print("LAPIC AKTIVIERT");
    }
    global_renderer->new_line();

    if ((LAPIC_ADDRESS & (1 << 11)) != 0) {
        global_renderer->print("APIC Aktiviert");
    }
    else {
        global_renderer->print("APIC Deaktiviert");
    }*/

   // setup_scroll_buffer();

   // s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer);
  //  scroll_manager = &s;

   // initialize_ps2_mouse();

    // TODO THIS CAUSES A SYSTEM ERROR WHEN MEM IS ABOVE 760m
   // outb(PIC1_DATA, 0b11111000);
  //  outb(PIC2_DATA, 0b11101111);


  //  calibrate_and_init_apic_timer( 100);

  //   calibrate_apic_timer();
}
