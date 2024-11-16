#include "./include/kernel_utils.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/interrupts/idt.h"
#include "../arch/x86_64/interrupts/interrupts.h"

void prepare_memory(BootInfo* bootInfo){
    uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

    global_allocator = PageFrameAllocator();
    global_allocator.read_efi_memory_map(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

    uint64_t kernelSize = (uint64_t)&_KernelEnd - (uint64_t)&_KernelStart;
    uint64_t kernelPages = (uint64_t)kernelSize / 4096 + 1;

    global_allocator.lock_pages(&_KernelStart, kernelPages);

    PageTable* PML4 = (PageTable*)global_allocator.request_page();
    memset(PML4, 0, 0x1000);

    global_page_table_manager = PageTableManager(PML4);

    for (uint64_t t = 0; t < get_memory_size(bootInfo->mMap, mMapEntries, bootInfo->mMapDescSize); t+= 0x1000){
        global_page_table_manager.map_memory((void*)t, (void*)t);
    }

    uint64_t fb_base = (uint64_t)bootInfo->framebuffer->base_address;
    uint64_t fb_size = (uint64_t)bootInfo->framebuffer->buffer_size + 0x1000;
    global_allocator.lock_pages((void*)fb_base, fb_size/ 0x1000 + 1);
    for (uint64_t t = fb_base; t < fb_base + fb_size; t += 4096){
        global_page_table_manager.map_memory((void*)t, (void*)t);
    }

    asm ("mov %0, %%cr3" : : "r" (PML4));
}

IDTR idtr;
void set_idt_gate(void* handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector) {
    IDTDescEntry* interrupt = (IDTDescEntry*)(idtr.offset + entry_offset * sizeof(IDTDescEntry));
    interrupt->set_offset((uint64_t)handler);
    interrupt->type_attr = type_attr;
    interrupt->selector = selector;
}

void prepare_interrupts() {
    idtr.limit = 0x0FFF;
    idtr.offset = (uint64_t)global_allocator.request_page();

    set_idt_gate((void*)page_fault_handler, 0xE, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)double_fault_handler, 0x8, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)gp_fault_handler, 0xD, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)keyboard_int_handler, 0x21, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)mouse_int_handler, 0x2C, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)pit_int_handler, 0x20, IDT_TA_InterruptGate, 0x08);
    asm ("lidt %0" : : "m" (idtr));

    remap_pic();
}

uint32_t* scroll_buffer_top = nullptr;
uint32_t* scroll_buffer_bottom = nullptr;
void setup_scroll_buffer()
{
    uint64_t buffer_size = global_renderer->TargetFramebuffer->width * global_renderer->TargetFramebuffer->height * sizeof(uint32_t) * 5;
    scroll_buffer_top = (uint32_t*)malloc(buffer_size);
    if (!scroll_buffer_top) {
        global_renderer->print("Failed to allocate scroll buffer\n");
        asm ("hlt");
    }
    memset(scroll_buffer_top, 0, buffer_size);


    scroll_buffer_bottom = (uint32_t*)malloc(buffer_size);
    if (!scroll_buffer_bottom) {
        global_renderer->print("Failed to allocate scroll buffer\n");
        asm ("hlt");
    }
    memset(scroll_buffer_bottom, 0, buffer_size);
}

void prepare_acpi(BootInfo* boot_info) {
    ACPI::SDTHeader* xsdt = (ACPI::SDTHeader*)(boot_info->rsdp->xsdt_address);

    ACPI::MCFGHeader* mcfg = (ACPI::MCFGHeader*)ACPI::find_table(xsdt, (char*)"MCFG");

    PCI::enumerate_pci(mcfg);
}

BasicRenderer r = BasicRenderer(NULL, NULL);
ScrollManager s = ScrollManager(NULL, NULL, NULL);
void initialize_kernel(BootInfo* bootInfo){
    r = BasicRenderer(bootInfo->framebuffer, bootInfo->psf1_font);
    global_renderer = &r;

    GDTDescriptor gdt_descriptor;
    gdt_descriptor.size = sizeof(GDT) - 1;
    gdt_descriptor.offset = (uint64_t)&default_gdt;
    load_GDT(&gdt_descriptor);

    prepare_memory(bootInfo);
    memset(bootInfo->framebuffer->base_address, 0, bootInfo->framebuffer->buffer_size);
    initialize_heap((void*)0x0000100000000000, 0x500);

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

    prepare_interrupts();

    setup_scroll_buffer();

    s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer);
    scroll_manager = &s;

    initialize_ps2_mouse();

    outb(PIC1_DATA, 0b11111000);
    outb(PIC2_DATA, 0b11101111);

    prepare_acpi(bootInfo);

    asm ("sti");
}