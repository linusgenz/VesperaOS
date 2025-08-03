#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"
#include "time/time.h"
#include "version.h"
#include "include/sys/syscalls.h"
#include "../filesystem/vfs/vfs.h"
#include "../include/log.h"
#include "include/scheduler.h"
#include "sync/mutex.h"
#include "include/sys/syscall_numbers.h"

extern "C" void switch_to_user_mode(void *user_stack_top, void *user_code_virt);

extern "C" void usermode_write_test() {
    const char *msg = "Hello from Ring 3!\n";
    sys_write(1, msg, strlen(msg));


    /*   const char* path = "/mnt/sd0/EFI/BOOT/t.txt";
       int64_t fd = syscall(SYSCALL_OPEN, (uint64_t)path);
       char buf[4096];
       syscall(SYSCALL_READ, fd, (uint64_t) buf, sizeof(buf));
       syscall(SYSCALL_WRITE, 1, (uint64_t) buf, strlen(buf));
       syscall(SYSCALL_CLOSE, fd);
   */
    const char *dir = "/mnt/sd0/EFI";
    int64_t res = sys_rmdir(dir);
    char buf[64];
    snprintf(buf, sizeof(buf), "Return: %s\n", to_string(res));
    sys_write(1, buf, strlen(buf));

    sys_exit(0);
}

extern "C" [[noreturn]] void kernel_main(BootInfo *boot_info) {
    initialize_kernel(boot_info);
    kernel::scheduling_started = true;
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
    kernel::memory::map_memory((void *) user_stack_phys, user_stack_phys, (1ULL << PT_Flag::UserSuper));
    void *user_stack_top = (void *) (user_stack_phys + 0x1000);

    //  Log::PrintLn("addr func: %p, stack: %p", (void *) &usermode_write_test, user_stack_top);


    //  auto fd = vfs_open("/mnt/sd0/EFI/BOOT/t.txt");
    //   Log::PrintLn("fd: %p", fd);
    //  vfs_close(fd);

    switch_to_user_mode(user_stack_top, (void *) &usermode_write_test);

    while (true);
}
