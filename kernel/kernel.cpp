#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"
#include "time/time.h"
#include "version.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "../include/log.h"
#include "include/scheduler.h"
#include "sys/syscall_interface.h"

extern "C" void switch_to_user_mode(void *user_stack_top, void *user_code_virt);

extern "C" void usermode_write_test() {
    const char* msg = "Hello from Ring 3!\n";
    syscall(SYSCALL_WRITE, 1, (uint64_t)msg, strlen(msg));
    const char* msg1 = "Second iteration\n";

    syscall(SYSCALL_WRITE, 1, (uint64_t)msg1, strlen(msg1));
    syscall(SYSCALL_EXIT, 0);
}

extern "C" [[noreturn]] void kernel_main(BootInfo *boot_info) {
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


    //  create_kthread(shell_loop, nullptr, 1);
    //  kernel::scheduling::enable_on_cpu(0);
    //  kernel::scheduling::yield();


    //  void *user_stack_phys = kernel::memory::request_page();
    //   uintptr_t user_stack_virt = 0x7FFFFFF000;  // Beispieladresse
    //   kernel::memory::map_memory((void*)user_stack_virt, user_stack_phys, PT_Flag::Present | PT_Flag::UserSuper);
    //   switch_to_user_mode((void*)user_stack_virt, nullptr);

    void *user_stack_phys = kernel::memory::request_page();
    kernel::memory::map_memory((void*)user_stack_phys, user_stack_phys, (1ULL << PT_Flag::UserSuper));
    void* user_stack_top = (void*)(user_stack_phys + 0x1000);

    Log::PrintLn("addr func: %p, stack: %p", (void*)&usermode_write_test, user_stack_top);

    // Sprung ins User-Mode mit RIP = user_code_virt, RSP = user_stack_top
    switch_to_user_mode(user_stack_top, (void*)&usermode_write_test);

    while (true);
}

