#include "./include/kernel_utils.h"
#include "./scheduling/pit/pit.h"
#include "./cpu/cpu.h"
#include "../interface/shell.h"

extern "C" void kernel_main(BootInfo* boot_info){
    
    PIT::set_divisor(65535);

    initialize_kernel(boot_info);

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

    PIT::sleep(2000);
    global_renderer->clear();

    while (true) {
        shell_loop();
    };

}