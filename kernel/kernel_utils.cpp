#if DEBUG_SPINLOCK
#include "debug/deadlock_detector.h"
#include "debug/lock_debug.h"
#endif

#include <kernel/input/input_manager.h>
#include <kernel/memory.h>
#include <kernel/realm/realm_manager.h>
#include <kernel/scheduling.h>
#include <kernel/system/system_manager.h>
#include <log.h>

#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/smp/prepare_ap_trampoline.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "../drivers/pci/msi.h"
#include "../drivers/ps2/ps2_init.h"
#include "../drivers/usb/usb_manager.h"
#include "../filesystem/devfs/devfs.h"
#include "../filesystem/realmfs/realmfs.h"
#include "../filesystem/vfs/vfs.h"
#include "../include/kernel/devices/device_manager.h"
#include "../include/kernel/kernel_utils.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include "cpu/cpu_manager.h"
#include "devices/init.h"
#include "graphics/display_manager.h"
#include "graphics/framebuffer_device.h"
#include "graphics/gop_render_driver.h"
#include "input/worker.h"
#include "kernel/interrupts.h"
#include "sys/syscall_interface.h"
#include "system/log_writer.h"
#include "tty/init.h"
#include "types/types.h"
#include "units/unit_manager.h"

Framebuffer* TargetFramebuffer = nullptr;

static void initialize_early_boot(const BootInfo* boot_info) {
#if DEBUG_SPINLOCK
    lock_debug_init();
    deadlock_detector_init();
#endif

    system_font = boot_info->font;
    TargetFramebuffer = boot_info->framebuffer;
    memset(TargetFramebuffer->base_address, 0, TargetFramebuffer->buffer_size);

    kernel::input::InputManager::init();
    Log::enableDebug();
    gdt_install();
}

static void initialize_memory_subsystem(BootInfo* boot_info) {
    kernel::memory::initialize_memory(boot_info);
    const uintptr_t heap_start = (reinterpret_cast<uintptr_t>(&_KernelEnd) + 0xFFFFF) & ~0xFFFFFULL;
    kernel::memory::initialize_heap(reinterpret_cast<void*>(heap_start), 0x500);
}

static void initialize_device_manager_and_vfs() {
    DeviceManager::init();
    VFS::init();
    DevFS::init();
    RealmFS::init();
}

static void initialize_graphics_and_terminal(const BootInfo* boot_info) {
    auto* renderer = new gop_render_driver(boot_info->framebuffer, boot_info->font);
    DisplayBackend be{renderer, renderer->get_kd()};
    DisplayManager::init(be);

    // Register framebuffer device
    auto* fbdev = new FramebufferDevice("fb0", BusType::VIRTUAL);
    auto* fb_kd = DeviceManager::RegisterCharDevice(
        fbdev, "fb0", DeviceClass::Graphics, BusType::VIRTUAL, ControllerType::None, nullptr
    );
    DevFS::register_device(fb_kd);

    // Setup terminal for logging
    auto terminal = new Terminal(renderer, system_font->width, system_font->height);
    Log::SetTerminal(terminal);
    global_terminal = terminal;

    Log::Info("Vespera booting...");
}

static void initialize_acpi_and_interrupts(BootInfo* boot_info) {
    ACPI::TableManager::init(boot_info);
    MADT::parse_madt(ACPI::TableManager::get_madt());
    ACPI::parse_fadt();

    kernel::interrupts::initialize();
    asm("sti");
}

static void initialize_cpu_and_realms() {
    kernel::SystemManager::initialize();
    CPUManager::initialize();
    setup_cpu_tss(0);
    RealmManager::initialize();

    // Create system realm
    RealmConfig realm_config_sys = {
        .name = "systemv",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    RealmManager::create(&realm_config_sys);

    // Create driver realm
    RealmConfig realm_config_drv = {
        .name = "driverv",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    RealmManager::create(&realm_config_drv);
}

static void initialize_scheduling_and_smp() {
    UnitManager::initialize();
    kernel::scheduling::init(CPUManager::total_cpus);

    prepare_ap_trampoline();
    //  CPUManager::smp_init(); REVIEW LOW ADDRESSES CPUSTARTUPREPORT AP_TRAMPOLINE

    // Threading now available - upgrade log to use mutex
    Log::init();
}

static void initialize_hardware_buses() {
    ps2_init();
    initialize_input_bus();
    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    if (USBManager::wait_for_all_controllers(2000))  // TODO
    {
        Log::Info("All USB controllers ready");
    } else {
        Log::Warning(
            "Timeout waiting for USB controllers (%u/%u ready)",
            USBManager::get_expected_count(),
            USBManager::get_initialized_count()
        );
    }
}

static void initialize_user_space_interfaces() {
    kernel::tty::initialize_ttys();
    initialize_pseudo_devices();
    VFS::remount_all();

    auto* fw = new FileLogWriter("/var/log/system.log");
    kernel::SystemManager::register_log_writer(fw);

    syscall_init();
    install_syscalls();
}

static void finalize_initialization() {
    kernel::interrupts::mask_pic();
}

// ============================================================================
// Main Kernel Initialization
// ============================================================================

void initialize_kernel(BootInfo* boot_info) {
    initialize_early_boot(boot_info);

    initialize_memory_subsystem(boot_info);

    initialize_device_manager_and_vfs();

    initialize_graphics_and_terminal(boot_info);

    initialize_acpi_and_interrupts(boot_info);

    initialize_cpu_and_realms();

    initialize_scheduling_and_smp();

    initialize_hardware_buses();

    initialize_user_space_interfaces();

    finalize_initialization();
}