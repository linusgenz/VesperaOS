#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"
#include "time/time.h"
#include "version.h"
#include "../include/log.h"
#include "include/scheduler.h"

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


    create_kthread(shell_loop, nullptr, 1);
    kernel::scheduling::enable_on_cpu(0);
    kernel::scheduling::yield();
}