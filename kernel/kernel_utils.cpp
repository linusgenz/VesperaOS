#if DEBUG_SPINLOCK
#include "debug/deadlock_detector.h"
#include "debug/lock_debug.h"
#endif

#include "../include/kernel/kernel_utils.h"
#include <kernel/memory.h>
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include <log.h>
#include "cpu/cpu_manager.h"
#include <kernel/scheduling.h>
#include "kernel/interrupts.h"
#include "../drivers/usb/usb_manager.h"
#include "../filesystem/devfs/devfs.h"
#include "../filesystem/vfs/vfs.h"
#include "../include/kernel/devices/device_manager.h"
#include "sys/syscall_interface.h"
#include <kernel/input/input_manager.h>
#include <kernel/realm/realm_manager.h>
#include "system/log_writer.h"
#include "types/types.h"
#include "units/unit_manager.h"
#include <kernel/system/system_manager.h>
#include "input/worker.h"
#include "../arch/x86_64/boot/bss.h"
#include "../arch/x86_64/smp/prepare_ap_trampoline.h"
#include "../drivers/pci/msi.h"
#include "devices/init.h"
#include <kernel/time.h>

#include <kernel/basic_renderer.h>

#include "../drivers/ps2/ps2_init.h"
#include "../drivers/ps2/keyboard/ps2_keyboard.h"
#include "../filesystem/realmfs/realmfs.h"
#include "tty/init.h"

uint64_t* scroll_buffer_top = nullptr;
uint64_t* scroll_buffer_bottom = nullptr;

void setup_scroll_buffer(Framebuffer* buffer)
{
    uint64_t buffer_size = buffer->width * buffer->height * sizeof(uint32_t) * 10;
    scroll_buffer_top = static_cast<uint64_t*>(kernel::memory::malloc(buffer_size));
    if (!scroll_buffer_top)
    {
        Log::Error("Failed to allocate scroll buffer (top)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_top, 0, buffer_size);

    scroll_buffer_bottom = static_cast<uint64_t*>(kernel::memory::malloc(buffer_size));
    if (!scroll_buffer_bottom)
    {
        Log::Error("Failed to allocate scroll buffer (bot)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_bottom, 0, buffer_size);
}

void prepare_acpi(const BootInfo* boot_info)
{
    auto* xsdt = reinterpret_cast<ACPI::SDTHeader*>(boot_info->rsdp->xsdt_address);
    // auto *rsdt = reinterpret_cast<ACPI::SDTHeader *>(boot_info->rsdp->rsdt_address);

    ACPI::TableManager::init(xsdt);

    ACPI::TableManager::register_madt();
    ACPI::TableManager::register_mcfg();
    ACPI::TableManager::register_fadr();
}

[[noreturn]] void sys_log_writer(void* arg)
{
    while (true)
    {
        kernel::SystemManager::process_events_to_logs(128);
        kernel::time::sleep_ms(1000);
    }
}

void init_sys_log_writer()
{
    UnitConfig uc = {
        .name = "system_log",
        .cpu_id = 7,
        .priority = 0,
        .stack_size = DEFAULT_UNIT_STACK_SIZE,
        .initial_handles = nullptr,
        .initial_handle_count = 0,
        .is_idle = false,
        .is_user = false,
        .user_stack_size = 0
    };
    UnitManager::create(KERNEL_REALM_SYSTEM, sys_log_writer, nullptr, &uc);
}

extern uint8_t Splash_VesperaOS_raw[]; // Aus xxd -i
extern unsigned int Splash_VesperaOS_raw_len;
static auto renderer = screen_renderer(nullptr, nullptr);
Framebuffer* TargetFramebuffer = nullptr;
void initialize_kernel(BootInfo* boot_info)
{
    zero_bss();

#if DEBUG_SPINLOCK
    lock_debug_init();
    deadlock_detector_init();
#endif
    system_font = boot_info->font;

    renderer = screen_renderer(boot_info->framebuffer, boot_info->font);
    Log::SetRenderer(&renderer);
    global_renderer = &renderer;

    TargetFramebuffer = boot_info->framebuffer;


    global_renderer->clear();

    kernel::input::InputManager::init();

    // generate_throbber();

    Log::enableDebug();

    gdt_install();

    kernel::memory::initialize_memory(boot_info);
    kernel::memory::initialize_heap(reinterpret_cast<void*>(0x0000100000000000), 0x500);


    prepare_acpi(boot_info);
    MADT::parse_madt(ACPI::TableManager::get_madt());
    ACPI::parse_fadt();

    kernel::interrupts::initialize();

    asm ("sti");

    kernel::SystemManager::initialize();

    CPUManager::initialize();
    setup_cpu_tss(0);
    RealmManager::initialize();

    VFS::init();
    DevFS::init();
    RealmFS::init();

    ps2_init();

    RealmConfig realm_config_sys = {
        .name = "systemv",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    RealmManager::create(&realm_config_sys);

    RealmConfig realm_config_drv = {
        .name = "driverv",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    RealmManager::create(&realm_config_drv);

    UnitManager::initialize();
    kernel::scheduling::init(CPUManager::total_cpus);
    // cannot create units before the scheduler inits, this is a feature not a bug


    //  RealmManager::list();

    //   UnitManager::list();

    prepare_ap_trampoline();
   // CPUManager::smp_init();
    Log::init(); // threads are possible -> switch to mutex

    initialize_input_bus();
    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    if (USBManager::wait_for_all_controllers(10000))
    {
        Log::Info("All USB controllers ready");
    }
    else
    {
        Log::Warning("Timeout waiting for USB controllers (%u/%u ready)",
                     USBManager::get_expected_count(),
                     USBManager::get_initialized_count());
    }

    kernel::tty::initialize_ttys();
    initialize_pseudo_devices();

    VFS::remount_all();

    auto* fw = new FileLogWriter("/var/log/system.log");
    kernel::SystemManager::register_log_writer(fw);
    //   init_sys_log_writer();
      // kernel::SystemManager::process_events_to_logs(128);

    syscall_init();
    install_syscalls();

    kernel::interrupts::mask_pic();
}
