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


void my_thread_func(void* arg) {
    uint64_t id = *(uint64_t*)arg;

  //  for (int i = 0; i < 3; i++) {
  //      Log::PrintLn("Thread id(i) %u is running on CPU %u it: %u", id, CPUManager::get_current_cpu_id(), i);
  //      kernel::time::thread_sleep_ms(1000);
  //  }
    Log::PrintLn("Thread %u is running on CPU %u", id, CPUManager::get_current_cpu_id());
}

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

     for (int i = 0; i < 16; i++) {
        create_kthread(my_thread_func, (void*)(uintptr_t)i, i % CPUManager::get_available_cpu_count());
     }

    kernel::scheduling::enable_on_cpu(0);
    kernel::scheduling::yield();

    while (true) {
  //      kernel::time::sleep_ms(2000);
  //      global_renderer->new_line();
  //      global_renderer->print("heartbeat");
      //  shell_loop();
    };

}

// 0x6B03F78
// 0x6B06F78