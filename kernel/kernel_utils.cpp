#include "./include/kernel_utils.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/interrupts/idt.h"
#include "../arch/x86_64/interrupts/interrupts.h"
#include "../arch/x86_64/interrupts/apic.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include "../include/log.h"
#include "memory/stack_manager.h"
#include "cpu/cpu_manager.h"
#include "cpu/ap_trampoline.h"

void prepare_memory(BootInfo* bootInfo){
    const uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

    global_allocator = PageFrameAllocator();

    global_allocator.read_efi_memory_map(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

    const uint64_t kernelStart = reinterpret_cast<uint64_t>(&_KernelStart);
    const uint64_t kernelEnd = reinterpret_cast<uint64_t>(&_KernelEnd);
    const uint64_t kernelSize = kernelEnd - kernelStart;
    const uint64_t kernelPages = kernelSize / 4096 + 1;

    global_allocator.lock_pages(&_KernelStart, kernelPages);

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
            global_page_table_manager.map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr), false);
        }
    }

    uint64_t fb_base = (uint64_t)bootInfo->framebuffer->base_address;
    uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;
    global_allocator.lock_pages((void*)fb_base, fb_size/ 0x1000 + 1);
    for (uint64_t t = fb_base; t < fb_base + fb_size; t += 0x1000){
        global_page_table_manager.map_memory((void*)t, (void*)t, false);
    }

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
    global_page_table_manager.map_memory((void*)LAPIC_ADDRESS, (void*)LAPIC_ADDRESS, false);

    void* idt_page = global_allocator.request_page();
    memset(idt_page, 0, 0x1000);

    idtr.limit = 0x0FFF;
    idtr.offset = reinterpret_cast<uint64_t>(idt_page);

    // Standard Exception Handlers
    set_idt_gate((void*)divide_error_handler, 0x00, IDT_TA_InterruptGate, 0x08);          // Divide by Zero
    set_idt_gate((void*)invalid_opcode_handler, 0x06, IDT_TA_InterruptGate, 0x08);       // Invalid Opcode
    set_idt_gate((void*)double_fault_handler, 0x08, IDT_TA_InterruptGate, 0x08);         // Double Fault
    set_idt_gate((void*)segment_not_present_handler, 0x0B, IDT_TA_InterruptGate, 0x08);  // Segment Not Present
    set_idt_gate((void*)stack_fault_handler, 0x0C, IDT_TA_InterruptGate, 0x08);          // Stack Fault
    set_idt_gate((void*)gp_fault_handler, 0x0D, IDT_TA_InterruptGate, 0x08);             // General Protection
    set_idt_gate((void*)page_fault_handler, 0x0E, IDT_TA_InterruptGate, 0x08);           // Page Fault
    set_idt_gate((void*)machine_check_handler, 0x12, IDT_TA_InterruptGate, 0x08);        // Machine Check
    
    // Catch-all for unhandled interrupts (fill some common vectors)
    for (int i = 0x20; i <= 0x2F; i++) {
        if (i != IRQ_TIMER && i != IRQ_AP_ENTRY) {
            set_idt_gate((void*)unhandled_interrupt_handler, i, IDT_TA_InterruptGate, 0x08);
        }
    }
  //  set_idt_gate((void*)keyboard_int_handler, 0x21, IDT_TA_InterruptGate, 0x08);
   // set_idt_gate((void*)mouse_int_handler, 0x2C, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)apic_timer_int_handler, IRQ_TIMER, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)spurious_int_handler, IRQ_SPURIOUS, IDT_TA_InterruptGate, 0x08);

    // AP Entry Handler für Vector IRQ_AP_ENTRY
    set_idt_gate((void*)ap_entry_int_handler, IRQ_AP_ENTRY, IDT_TA_InterruptGate, 0x08);
    
    asm ("lidt %0" : : "m" (idtr));
    asm ("sti");
}

uint32_t* scroll_buffer_top = nullptr;
uint32_t* scroll_buffer_bottom = nullptr;
void setup_scroll_buffer(Framebuffer *buffer)
{
    uint64_t buffer_size = buffer->width * buffer->height * sizeof(uint32_t) * 5;
    scroll_buffer_top = (uint32_t*)malloc(buffer_size);
    if (!scroll_buffer_top) {
        Log::Error("Failed to allocate scroll buffer (top)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_top, 0, buffer_size);


    scroll_buffer_bottom = (uint32_t*)malloc(buffer_size);
    if (!scroll_buffer_bottom) {
        Log::Error("Failed to allocate scroll buffer (bot)\n");
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

void prepare_ap_trampoline(uint64_t pml4_phys) {
    constexpr uint64_t trampoline_phys_addr = 0x8000; // SIPI-Adresse (IRQ_AP_ENTRY << 12)
    constexpr size_t pml4_ptr_offset = 0x200;  // muss mit Trampolin übereinstimmen
    constexpr size_t stack_ptr_offset = 0x208; // dito

    memcpy((void*)trampoline_phys_addr, kernel_cpu_ap_trampoline_bin, kernel_cpu_ap_trampoline_bin_len);

    for (uint32_t i = 0; i < CPUManager::get_available_cpu_count(); i++) {
        auto& cpu = CPUManager::cpu_infos[i];
        if (cpu.is_bsp) continue;

        void* stack_top = cpu.kernel_stack->stack_top;
        uint64_t stack_phys = global_page_table_manager.get_physical_address(stack_top); // Diese Funktion brauchst du

        *(volatile uint64_t*)(trampoline_phys_addr + pml4_ptr_offset)  = pml4_phys;
        *(volatile uint64_t*)(trampoline_phys_addr + stack_ptr_offset) = stack_phys;
    }
}

extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

void zero_bss() {
    uint8_t* bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }
}

ScrollManager s = ScrollManager(nullptr, nullptr, nullptr, nullptr);
static BasicRenderer renderer = BasicRenderer(nullptr, nullptr);
Framebuffer *TargetFramebuffer = nullptr;
void initialize_kernel(BootInfo* bootInfo){
    zero_bss();
    renderer = BasicRenderer(bootInfo->framebuffer, bootInfo->psf1_font);
    Log::SetRenderer(&renderer);
    memset(bootInfo->framebuffer->base_address, 0, bootInfo->framebuffer->buffer_size);

    TargetFramebuffer = bootInfo->framebuffer;

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

  //  setup_scroll_buffer(bootInfo->framebuffer);
 //   s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer, &renderer);
 //   scroll_manager = &s;

    CPUManager::initialize();
    
    // Zeige CPU-Informationen
    CPUManager::print_cpu_info();

    uint64_t pml4_phys = global_page_table_manager.get_physical_address(global_page_table_manager.PML4);
    prepare_ap_trampoline(pml4_phys);

    CPUManager::start_all_aps();
  //  StackManager::print_stack_info();

 //   PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    Log::Info("Free RAM: %u mb", global_allocator.get_free_ram() / 1024 / 1024);
    Log::Info("Reserved RAM: %u mb", global_allocator.get_reserved_ram() / 1024 / 1024);
    Log::Info("Used RAM: %u mb", global_allocator.get_used_ram() / 1024 / 1024);

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

   //

   // initialize_ps2_mouse();

    // TODO THIS CAUSES A SYSTEM ERROR WHEN MEM IS ABOVE 760m
   // outb(PIC1_DATA, 0b11111000);
  //  outb(PIC2_DATA, 0b11101111);
}
