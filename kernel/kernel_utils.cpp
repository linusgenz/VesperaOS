#include <arch/x86_64/gdt.h>

#include "../drivers/fb/framebuffer_driver.h"
#include "../include/acpi/acpi_subsystem.h"
#include "cpu/cpu.h"
#include "drivers/pci/pci_driver.h"
#include "graphics/font/ttf_glyph_provider.h"
#include "vespera/ipc/vbus_manager.h"
#include "vespera/time.h"
#if DEBUG_SPINLOCK
#include "debug/deadlock_detector.h"
#include "debug/lock_debug.h"
#endif

#include <filesystem/devfs.h>
#include <filesystem/vfs.h>
#include <klib/result.h>
#include <units/unit.h>
#include <vespera/devices/device_manager.h>
#include <vespera/input/input_manager.h>
#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>
#include <vespera/types.h>

#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/smp/prepare_ap_trampoline.h"
#include "../drivers/pci/msi.h"
#include "../drivers/ps2/ps2_init.h"
#include "../include/arch/x86_64/syscall.h"
#include "../include/drivers/usb/usb_manager.h"
#include "../include/filesystem/realmfs.h"
#include "../include/vespera/graphics/display_manager.h"
#include "../include/vespera/unit/unit_manager.h"
#include "cpu/cpu_manager.h"
#include "devices/init.h"
#include "graphics/framebuffer_device.h"
#include "input/worker.h"
#include "sys/syscall_interface.h"
#include "system/log_writer.h"
#include "tty/init.h"
#include "vespera/interrupts.h"

Framebuffer* target_framebuffer = nullptr;

static void initialize_early_boot(const BootInfo* boot_info) {
#if DEBUG_SPINLOCK
    lock_debug_init();
    deadlock_detector_init();
#endif

    system_font = boot_info->font;
    target_framebuffer = boot_info->framebuffer;
    memset(target_framebuffer->base_address, 0, target_framebuffer->buffer_size);

    detect_qemu();

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
    kernel::input::InputManager::init();
}

static void initialize_graphics_and_terminal(const BootInfo* boot_info) {
    auto* renderer = new FramebufferDriver(boot_info->framebuffer, boot_info->font);
    const DisplayBackend be{renderer, renderer->get_kd()};
    DisplayManager::init(be);

    // Register framebuffer device
    auto* fbdev = new FramebufferDevice("fb0", BusType::VIRTUAL);
    auto* fb_kd = DeviceManager::register_device(
        DeviceDescriptor{}
            .set_name("fb0")
            .set_type(DeviceType::Char)
            .set_class(DeviceClass::Graphics)
            .with_char(fbdev)
            .set_bus(BusType::VIRTUAL)
            .set_controller(ControllerType::None)
    );

    DevFs::register_device(fb_kd);

    auto* gpu_kd = DeviceManager::register_device(
        DeviceDescriptor{}
            .set_name("gpu")
            .set_class(DeviceClass::Pseudo)
            .set_bus(BusType::VIRTUAL)
            .set_controller(ControllerType::None)
    );
    DevFs::register_device(gpu_kd);

    // Setup terminal for logging
    const auto terminal = new Terminal(renderer, system_font->width, system_font->height);
    Log::set_terminal(terminal);
    kernel::SystemManager::set_system_terminal(terminal);

    Log::info("Vespera booting...");
    Log::debug("using simd extentions: %s", renderer->using_avx ? "AVX2" : (renderer->using_sse ? "SSE" : "None"));
}

static void initialize_acpi_and_interrupts(BootInfo* boot_info) {
    kernel::acpi::early_init(boot_info);

    kernel::interrupts::initialize();
    asm("sti");

    kernel::time::init_clock();
    kernel::time::epoch_init();
}

static void initialize_cpu_and_realms() {
    kernel::SystemManager::initialize();
    cpu_manager::initialize();
    setup_cpu_tss(0);
    RealmManager::initialize();

    constexpr RealmConfig realm_config_sys = {
        .name = "systemv",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    RealmManager::create(&realm_config_sys);

    constexpr RealmConfig realm_config_drv = {
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

    arch::x86_64::smp::prepare_ap_trampoline();
    cpu_manager::smp_init();

    Log::init();

    kernel::acpi::init();
}

static void initialize_hardware_buses() {
    ps2_init();
    initialize_input_bus();
    pci::driver_registry::init_drivers();
    pci::enumerate_pci(kernel::acpi::get_mcfg());

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

    if (Result<VfsNode*> font_result = VFS::open("/etc/fonts/CaskaydiaCoveNerdFontMono.ttf"); font_result.is_ok()) {
        VfsNode* font_node = font_result.unwrap();
        const usize font_size = font_node->size;
        if (auto* font_data = static_cast<u8*>(kernel::memory::malloc(font_size))) {
            Log::debug("font data: %p", font_data);
            Result<usize> i = VFS::read(font_node, 0, font_size, font_data);
            Log::debug("read: %lu", i);
            VFS::close(font_node);

            auto* ttf = new TtfGlyphProvider(font_data, font_size, 20.0f);
            Log::debug("ttf: %p", ttf);
            if (ttf->is_valid()) {
                kernel::SystemManager::get_system_terminal()->set_glyph_provider(ttf);
                Log::ok("[TTF] Terminal font switched to CascadiaCode");
            } else {
                delete ttf;
                kernel::memory::free(font_data);
                Log::warning("[TTF] Font load failed, keeping PSF");
            }
        } else {
            VFS::close(font_node);
        }
    } else {
        Log::warning("[TTF] /etc/fonts/CaskaydiaCoveNerdFontMono.ttf not found, keeping PSF");
    }
    auto* fw = new FileLogWriter("/var/log/system.log");
    kernel::SystemManager::register_log_writer(fw);

    VBusManager::init();

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