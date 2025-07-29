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
#include "scheduling/scheduler.h"
#include "../arch/x86_64/interrupts/ioapic.h"

void prepare_memory(BootInfo* bootInfo){
    const uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

    global_allocator = PageFrameAllocator();

    global_allocator.read_efi_memory_map(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

    const uint64_t kernelStart = reinterpret_cast<uint64_t>(&_KernelStart);
    const uint64_t kernelEnd = reinterpret_cast<uint64_t>(&_KernelEnd);
    const uint64_t kernelSize = kernelEnd - kernelStart;
    const uint64_t kernelPages = kernelSize / 4096 + 1;

    global_allocator.lock_pages(&_KernelStart, kernelPages);

    global_allocator.lock_pages(nullptr, 256);


    PageTable* PML4 = (PageTable*)global_allocator.request_page();
    memset(PML4, 0, 0x1000);

    global_page_table_manager = PageTableManager(PML4);


    // just map everythin cuz it works lol. might not be a good practice tho, needs refactoring prob
    for (int i = 0; i < mMapEntries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)bootInfo->mMap + (i * bootInfo->mMapDescSize));
      //  if (desc->type != 7) continue; // Nur EfiConventionalMemory

        for (uint64_t addr = desc->phys_addr; addr < desc->phys_addr + desc->num_pages * 0x1000; addr += 0x1000) {
            global_page_table_manager.map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
        }
    }

    for (uint64_t addr = kernelStart; addr < kernelEnd; addr += 0x1000) {
        global_page_table_manager.map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
    }

    for (uint32_t i = 0; i < CPUManager::total_cpus; ++i) {
        void* stack_addr = (void*)(KERNEL_STACK_BASE + i * KERNEL_STACK_SIZE);
        global_page_table_manager.map_memory(stack_addr, stack_addr, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
    }

    global_page_table_manager.map_memory((void*)0x8000, (void*)0x8000, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
    global_page_table_manager.map_memory((void*)0x7000, (void*)0x7000, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
    global_page_table_manager.map_memory((void*)0x6000, (void*)0x6000, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
    global_page_table_manager.map_memory((void*)0x1000, (void*)0x1000, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);
    global_page_table_manager.map_memory((void*)0x2000, (void*)0x2000, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);

    uint64_t fb_base = (uint64_t)bootInfo->framebuffer->base_address;
    uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;
    global_allocator.lock_pages((void*)fb_base, fb_size/ 0x1000 + 1);
    for (uint64_t t = fb_base; t < fb_base + fb_size; t += 0x1000){
        global_page_table_manager.map_memory((void*)t, (void*)t);
    }

    asm ("mov %0, %%cr3" : : "r" (PML4));
}

extern "C" void irq_stub_0x30();
void prepare_interrupts() {
    global_page_table_manager.map_memory((void*)g_localApicAddr, (void*)g_localApicAddr, PT_Flag::WriteThrough | PT_Flag::CacheDisabled);

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
    set_idt_gate((void*)irq_stub_0x30, IRQ_XHCI_VECTOR, IDT_TA_InterruptGate, 0x08);
    // Catch-all for unhandled interrupts (fill some common vectors)
  //  for (int i = 0x20; i <= 0x2F; i++) {
   //     if (i != IRQ_TIMER && i != IRQ_AP_ENTRY) {
   //         set_idt_gate((void*)unhandled_interrupt_handler, i, IDT_TA_InterruptGate, 0x08);
   //     }
   // }

    set_idt_gate((void*)keyboard_int_handler, 0x21, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)mouse_int_handler, 0x2C, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)apic_timer_int_handler, IRQ_TIMER, IDT_TA_InterruptGate, 0x08);
    set_idt_gate((void*)spurious_int_handler, IRQ_SPURIOUS, IDT_TA_InterruptGate, 0x08);


    // AP Entry Handler für Vector IRQ_AP_ENTRY
  //  set_idt_gate((void*)ap_entry_int_handler, IRQ_AP_ENTRY, IDT_TA_InterruptGate, 0x08);


    asm ("lidt %0" : : "m" (idtr));
    asm ("cli");
    remap_pic();
}

uint32_t* scroll_buffer_top = nullptr;
uint32_t* scroll_buffer_bottom = nullptr;
void setup_scroll_buffer(Framebuffer *buffer)
{
    uint64_t buffer_size = buffer->width * buffer->height * sizeof(uint32_t) * 10;
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
}

void prepare_ap_trampoline(uint64_t pml4_phys) {
    *(volatile uint64_t*)0x2000 = pml4_phys;
    *(IDTR*)0x1000 = idtr;
 //   __asm__ volatile("wbinvd" ::: "memory");
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
    global_renderer = &renderer;
    memset(bootInfo->framebuffer->base_address, 0, bootInfo->framebuffer->buffer_size);

    TargetFramebuffer = bootInfo->framebuffer;
    Log::enableDebug();

    GDTDescriptor gdt_descriptor;
    gdt_descriptor.size = sizeof(GDT) - 1;
    gdt_descriptor.offset = (uint64_t)&default_gdt;
    load_GDT(&gdt_descriptor);

    prepare_memory(bootInfo);
    initialize_heap((void*)0x0000100000000000, 0x500);

    prepare_acpi(bootInfo);
    MADT::parse_madt(ACPI::TableManager::get_madt());

    prepare_interrupts();

    initialize_ps2_mouse();

    outb(PIC1_DATA, 0b11111001); // = 0xFD → IRQ1 (keyboard) activated
    outb(PIC2_DATA, 0b11101111); // = 0xEF → IRQ12 (mouse) activated
    asm ("sti");

    lapic_init(0);
  //  pic_disable();
   // IOAPIC::init();


    setup_scroll_buffer(bootInfo->framebuffer);
    s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer, &renderer);
    scroll_manager = &s;

//    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    CPUManager::initialize();
    kernel::scheduling::init(CPUManager::total_cpus);
    Log::init(); // threads are possible -> switch to mutex
    uint64_t pml4_phys = global_page_table_manager.get_physical_address(global_page_table_manager.PML4);
    prepare_ap_trampoline(pml4_phys);

    CPUManager::smp_init();

    CPUManager::print_cpu_info();

  //  StackManager::print_stack_info();


    Log::Info("Free RAM: %u mb", global_allocator.get_free_ram() / 1024 / 1024);
    Log::Info("Reserved RAM: %u mb", global_allocator.get_reserved_ram() / 1024 / 1024);
    Log::Info("Used RAM: %u mb", global_allocator.get_used_ram() / 1024 / 1024);

}
