#include "./include/kernel_utils.h"
#include "./scheduling/pit_legacy/pit.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"
#include "time/time.h"
#include "version.h"
#include "../include/log.h"
#include "cpu/cpu_manager.h"
#include "scheduling/scheduler.h"
#include "scheduling/thread.h"
#include "../arch/x86_64/interrupts/apic.h"


extern "C" [[noreturn]] void kernel_main(BootInfo* boot_info){
    initialize_kernel(boot_info);

    char vendor[13];
    get_cpu_vendor(vendor);
    Log::Info("CPU Vendor: %s", vendor);

    char brand[49];
    get_cpu_brand(brand);
    Log::Info("CPU Brand: %s", brand);
    Log::Ok("Kernel initialized successfully");
    Log::Info("Kernel version: %s", get_os_version());
    kernel::time::print_current_time();

   // kernel::scheduling::enable_on_cpu(0);
   /// kernel::scheduling::yield();

    while (true) {
        kernel::time::sleep_ms(2000);
     //   global_renderer->clear();
        shell_loop();
    };

}