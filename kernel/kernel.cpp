#include "./include/kernel_utils.h"
#include "./scheduling/pit/pit.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"
#include "time/timer.h"

static inline bool are_interrupts_enabled()
{
    unsigned long flags;
    asm volatile ( "pushf\n\t"
                   "pop %0"
                   : "=g"(flags) );
    return flags & (1 << 9);
}

extern "C" void kernel_main(BootInfo* boot_info){
    initialize_kernel(boot_info);

/*
    uint64_t mMapEntries = boot_info->mMapSize / boot_info->mMapDescSize;

    for (int i = 0; i < mMapEntries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)boot_info->mMap + (i * boot_info->mMapDescSize));
        global_renderer->print(EFI_MEMORY_TYPE_STRINGS[desc->type]);
        global_renderer->print(" ");
        global_renderer->set_colour(CYAN);
        global_renderer->print(to_string(desc->num_pages * 4096 / 1024));
        global_renderer->print(" ");
        global_renderer->print("kb");
        global_renderer->print(" (");
        global_renderer->print(to_string(desc->num_pages * 4096 / 1024 / 1024));
        global_renderer->print(" ");
        global_renderer->print("mb)");
        global_renderer->new_line();
        global_renderer->set_colour(WHITE);
    }
*/
    global_renderer->print("INTERRUPTS:");
    global_renderer->print(are_interrupts_enabled() ? " ENABLED" : " DISABLED");
    global_renderer->new_line();

    char vendor[13];
    get_cpu_vendor(vendor);
    global_renderer->print("CPU Vendor: ");
    global_renderer->print(vendor);
    global_renderer->new_line();
    char brand[49];
    get_cpu_brand(brand);
    global_renderer->print("CPU Brand: ");
    global_renderer->print(brand);
    global_renderer->new_line();
    global_renderer->print("Kernel initialized successfully");

    while (true) {
  //      kernel::time::sleep_ms(2000);
  //      global_renderer->new_line();
  //      global_renderer->print("heartbeat");
      //  shell_loop();
    };

}