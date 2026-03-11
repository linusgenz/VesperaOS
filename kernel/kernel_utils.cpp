#include "cpu/cpu.h"
#if DEBUG_SPINLOCK
#include "debug/deadlock_detector.h"
#include "debug/lock_debug.h"
#endif

#include <vespera/devices/device_manager.h>
#include <vespera/input/input_manager.h>
#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/smp/prepare_ap_trampoline.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "../drivers/pci/msi.h"
#include "../drivers/ps2/ps2_init.h"
#include "../drivers/usb/usb_manager.h"
#include "../filesystem/devfs/devfs.h"
#include "../filesystem/realmfs/realmfs.h"
#include "../filesystem/vfs/vfs.h"
#include "../include/vespera/types.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include "cpu/cpu_manager.h"
#include "devices/init.h"
#include "graphics/display_manager.h"
#include "graphics/framebuffer_device.h"
#include "graphics/gop_render_driver.h"
#include "input/worker.h"
#include "sys/syscall_interface.h"
#include "system/log_writer.h"
#include "tty/init.h"
#include "units/unit_manager.h"
#include "vespera/interrupts.h"

framebuffer_t* target_framebuffer = nullptr;

static void initialize_early_boot(const BootInfo* boot_info) {
#if DEBUG_SPINLOCK
    lock_debug_init();
    deadlock_detector_init();
#endif

    system_font = boot_info->font;
    target_framebuffer = boot_info->framebuffer;
    memset(target_framebuffer->base_address, 0, target_framebuffer->buffer_size);

    detect_qemu();

    kernel::input::InputManager::init();
    Log::enable_debug();
    gdt_install();
}

static void initialize_memory_subsystem(BootInfo* boot_info) {
    kernel::memory::initialize_memory(boot_info);
    const virt_addr_t heap_start = virt_from_raw((reinterpret_cast<uptr>(&kernel_end) + 0xFFFFF) & ~0xFFFFFULL);
    kernel::memory::initialize_heap((heap_start), 0x500);
}

static void initialize_device_manager_and_vfs() {
    DeviceManager::init();
    VFS::init();
    DevFs::init();
    RealmFs::init();
}

static void initialize_graphics_and_terminal(const BootInfo* boot_info) {
    auto* renderer = new GopRenderDriver(boot_info->framebuffer, boot_info->font);
    DisplayBackend be{renderer, renderer->get_kd()};
    DisplayManager::init(be);

    // Register framebuffer device
    auto* fbdev = new FramebufferDevice("fb0", BusType::VIRTUAL);
    auto* fb_kd = DeviceManager::register_char_device(
        fbdev, "fb0", DeviceClass::Graphics, BusType::VIRTUAL, ControllerType::None, nullptr
    );
    DevFs::register_device(fb_kd);

    // Setup terminal for logging
    auto terminal = new Terminal(renderer, system_font->width, system_font->height);
    Log::set_terminal(terminal);
    global_terminal = terminal;

    Log::info("Vespera booting...");
}

static void initialize_acpi_and_interrupts(BootInfo* boot_info) {
    acpi::TableManager::init(boot_info);
    madt::parse_madt(acpi::TableManager::get_madt());
    acpi::parse_fadt();

    kernel::interrupts::initialize();
    asm("sti");
}

static void initialize_cpu_and_realms() {
    kernel::SystemManager::initialize();
    cpu_manager::initialize();
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
    kernel::scheduling::init(cpu_manager::total_cpus);

    prepare_ap_trampoline();
    cpu_manager::smp_init();

    // Threading now available - upgrade log to use mutex
    Log::init();
}

static void initialize_hardware_buses() {
    ps2_init();
    initialize_input_bus();
    pci::enumerate_pci(acpi::TableManager::get_mcfg());

    if (UsbManager::wait_for_all_controllers(10000))  // TODO
    {
        Log::info("All USB controllers ready");
    } else {
        Log::warning(
            "Timeout waiting for USB controllers (%u/%u ready)",
            UsbManager::get_expected_count(),
            UsbManager::get_initialized_count()
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