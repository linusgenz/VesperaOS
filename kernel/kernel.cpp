#include "./include/kernel_utils.h"
#include "./scheduling/pit_legacy/pit.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"
#include "time/time.h"
#include "version.h"
#include "../include/log.h"

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

    Log::LogMsg("INTERRUPTS: %s", are_interrupts_enabled() ? "ENABLED" : "DISABLED");

    char vendor[13];
    get_cpu_vendor(vendor);
    Log::Info("CPU Vendor: %s", vendor);

    char brand[49];
    get_cpu_brand(brand);
    Log::Info("CPU Brand: %s", brand);
    Log::Ok("Kernel initialized successfully");
    Log::Info("Kernel version: %s", get_os_version());
    kernel::time::print_current_time();
    while (true) {
  //      kernel::time::sleep_ms(2000);
  //      global_renderer->new_line();
  //      global_renderer->print("heartbeat");
      //  shell_loop();
    };

}