#include "exec/elf.h"
#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "time/time.h"
#include "version.h"
#include "include/sys/syscalls.h"
#include "../include/log.h"
#include "include/scheduler.h"
#include "sync/mutex.h"

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
    /*   const char *dir = "/mnt/sd0/EFI";
       int64_t res = sys_rmdir(dir);
       char buf[64];
       snprintf(buf, sizeof(buf), "Return: %s\n", to_string(res));
       sys_write(1, buf, strlen(buf));*/

    sys_create("/mnt/sd0/testfilexd.txt");
    // sys_rename("/mnt/sd0/startup.nsh", "/mnt/sd0/chaname.nsh");
    //   auto t = sys_rename("/mnt/sd0/EFI", "/mnt/sd0/testEFI");
    // char buf[64];
    //  snprintf(buf, sizeof(buf), "Return: %s\n", to_string(t));
    // sys_write(1, buf, strlen(buf));
    //  auto t = sys_rename("/mnt/sd0/EFI", "/mnt/sd0/testEFI");
    // char buf[64];
    // snprintf(buf, sizeof(buf), "Return: %s\n", to_string(t));
    // sys_write(1, buf, strlen(buf));


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

    uint64_t entry;
    void *start_addr = load_elf_binary("/mnt/sd0/bin/shell.elf", &entry);

    if (start_addr) {
        Log::Info("Jumping to ELF entry point at %p", entry);
        switch_to_user_mode(user_stack_top, (void *) entry);
    } else {
        Log::Warning("not able to load elf. %p %p", start_addr, entry);
    }

    while (true);
}
