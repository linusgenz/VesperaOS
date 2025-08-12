#include <unistd.h>

#include "exec/elf.h"
#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "time/time.h"
#include "version.h"
#include "include/sys/syscalls.h"
#include "../include/log.h"
#include "include/scheduler.h"
#include "include/sys/syscall_numbers.h"
#include "sync/mutex.h"

extern "C" void switch_to_user_mode(void *user_stack_top, void *user_code_virt);

extern "C" void usermode_write_test(void *arg) {
  //  const char *msg = "Hello from Ring 3!\n";
  //  sys_write(1, msg, strlen(msg));


  //  const char *path = "/mnt/sd0/EFI/BOOT/t.txt";
  //  int64_t fd = sys_open(path);
  //  char buf[4096];
  //  sys_read(fd, buf, sizeof(buf));
  //  sys_write(1, buf, strlen(buf));
  //  sys_close(fd);

    // const char *dir = "/mnt/sd0/EFI";
    // int64_t res = sys_rmdir(dir);
    // char buf1[64];
    // snprintf(buf1, sizeof(buf1), "Return: %s\n", to_string(res));
    // sys_write(1, buf1, strlen(buf1));

    //   sys_create("/mnt/sd0/testfilexd.txt");
    // sys_rename("/mnt/sd0/startup.nsh", "/mnt/sd0/chaname.nsh");
    //   auto t = sys_rename("/mnt/sd0/EFI", "/mnt/sd0/testEFI");
    // char buf[64];
    //  snprintf(buf, sizeof(buf), "Return: %s\n", to_string(t));
    // sys_write(1, buf, strlen(buf));
    //  auto t = sys_rename("/mnt/sd0/EFI", "/mnt/sd0/testEFI");
    // char buf[64];
    // snprintf(buf, sizeof(buf), "Return: %s\n", to_string(t));
    // sys_write(1, buf, strlen(buf));
     while (1) {
           const char *msg = "Hello from Ring 3!\n";
           sys_write(1, msg, strlen(msg));
           char buf[4096];
           sys_read(0, buf, sizeof(buf));
           kernel::time::sleep_ms(1000);
     };
    sys_exit(0);
}

void kernel_thread(void *) {
    while (1) {
        Log::LogMsg("kernel thread alive");
        kernel::time::sleep_ms(1000);
    }
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


    void *user_stack_phys2 = kernel::memory::request_page();
    kernel::memory::map_memory((void *) user_stack_phys2, user_stack_phys2, (1ULL << PT_Flag::UserSuper));
    void *user_stack_top2 = (void *) (user_stack_phys2 + 0x1000);

   // uint64_t entry;
   // void *start_addr = load_elf_binary("/mnt/sd0/bin/shell.elf", &entry, 0x400000);

 //   uint64_t entry1;
 //   void *start_addr1 = load_elf_binary("/mnt/sd0/bin/shell1.elf", &entry1, 0x500000);

  //  if (start_addr) {
  //      Log::Info("Jumping to ELF entry point at %p", entry);
   //     auto shell = create_user_thread((void *) entry, user_stack_top);
     //   auto write_test = create_user_thread((void*)entry1, user_stack_top2);
      //  kernel::scheduling::add_thread(shell);
     //   kernel::scheduling::add_thread(write_test);
        //   create_kthread(kernel_thread, nullptr, 0);

    //    Log::debug("thread shell: ptr: %p id=%u is_user=%u user_entry=%p user_stack_top=%p kstack=%p ksp=%p", shell,
    //       shell->id, shell->is_user_thread, shell->user_entry, shell->user_stack_top,
    ////       shell->stack, shell->stack_pointer);
      /*  Log::debug("thread write_test: ptr: %p id=%u is_user=%u user_entry=%p user_stack_top=%p kstack=%p ksp=%p",
                   write_test,write_test->id, write_test->is_user_thread, write_test->user_entry, write_test->user_stack_top,
                   write_test->stack, write_test->stack_pointer);*/

    kprocess_t* shell_proc = create_process_from_elf("shell", "/mnt/sd0/bin/shell.elf");
    shell_proc->state = PROCESS_READY;
    shell_proc->main_thread->state = THREAD_READY;
    Log::debug("shell proc. %p", shell_proc->main_thread->entry);
        kernel::scheduling::add_thread(shell_proc->main_thread);
        kernel::scheduling::enable_on_cpu(0);
        kernel::scheduling::yield();
        // switch_to_user_mode(user_stack_top, (void *) &usermode_write_test);
 //   } else {
 //       Log::Warning("not able to load elf. %p %p", start_addr, entry);
//    }

    while (true);
}
