#include "./include/kernel_utils.h"

#include <memory.h>

#include "throbber.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include "../include/log.h"
#include "memory/stack_manager.h"
#include "cpu/cpu_manager.h"
#include <scheduling.h>
#include "include/interrupts.h"
#include "../drivers/usb/usb_manager.h"
#include "../filesystem/devfs/devfs.h"
#include "../filesystem/vfs/vfs.h"
#include "devices/device_manager.h"
#include "sys/syscall_interface.h"
#include "input/input_manager.h"
#include "realm/realm.h"
#include "realm/realm_manager.h"
#include "system/log_writer.h"
#include "types/types.h"
#include "units/unit_manager.h"
#include "system/system_manager.h"
#include "input/worker.h"
#include "../arch/x86_64/boot/bss.h"
#include "../arch/x86_64/smp/prepare_ap_trampoline.h"
#include "devices/init.h"
#include "tty/init.h"

uint32_t *scroll_buffer_top = nullptr;
uint32_t *scroll_buffer_bottom = nullptr;

void setup_scroll_buffer(Framebuffer *buffer) {
    uint64_t buffer_size = buffer->width * buffer->height * sizeof(uint32_t) * 10;
    scroll_buffer_top = (uint32_t *) malloc(buffer_size);
    if (!scroll_buffer_top) {
        Log::Error("Failed to allocate scroll buffer (top)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_top, 0, buffer_size);


    scroll_buffer_bottom = (uint32_t *) malloc(buffer_size);
    if (!scroll_buffer_bottom) {
        Log::Error("Failed to allocate scroll buffer (bot)\n");
        asm ("hlt");
    }
    memset(scroll_buffer_bottom, 0, buffer_size);
}

void prepare_acpi(BootInfo *boot_info) {
    ACPI::SDTHeader *xsdt = reinterpret_cast<ACPI::SDTHeader *>(boot_info->rsdp->xsdt_address);
    ACPI::SDTHeader *rsdt = reinterpret_cast<ACPI::SDTHeader *>(boot_info->rsdp->rsdt_address);

    ACPI::TableManager::init(xsdt);

    ACPI::TableManager::register_madt();
    ACPI::TableManager::register_mcfg();
    ACPI::TableManager::register_fadr();
}

typedef enum {
    PIXEL_FORMAT_RGB,
    PIXEL_FORMAT_BGR
} PixelFormat;

void render_image_rgba8888_centered(
    const Framebuffer *fb,
    const uint8_t *img_data,
    uint32_t img_width,
    uint32_t img_height,
    PixelFormat format
) {
    if (!fb || !img_data) return;

    uint32_t x_offset = (fb->width - img_width) / 2;
    uint32_t y_offset = (fb->height - img_height) / 2;

    uint32_t *framebuffer = (uint32_t *) fb->base_address;

    for (uint32_t y = 0; y < img_height; y++) {
        if (y + y_offset >= fb->height) break;

        for (uint32_t x = 0; x < img_width; x++) {
            if (x + x_offset >= fb->width) break;

            uint32_t img_index = (y * img_width + x) * 4;

            uint8_t r = img_data[img_index + 0];
            uint8_t g = img_data[img_index + 1];
            uint8_t b = img_data[img_index + 2];
            // uint8_t a = img_data[img_index + 3]; // Ignorieren

            uint32_t pixel_value;
            if (format == PIXEL_FORMAT_RGB) {
                pixel_value = (r << 16) | (g << 8) | b;
            } else {
                // PIXEL_FORMAT_BGR
                pixel_value = (b << 16) | (g << 8) | r;
            }

            uint32_t fb_x = x + x_offset;
            uint32_t fb_y = y + y_offset;
            uint32_t fb_index = fb_y * fb->pixels_per_scanline + fb_x;

            framebuffer[fb_index] = pixel_value;
        }
    }
}

extern uint8_t Splash_VesperaOS_raw[]; // Aus xxd -i
extern unsigned int Splash_VesperaOS_raw_len;
ScrollManager s = ScrollManager(nullptr, nullptr, nullptr, nullptr, 0);
static BasicRenderer renderer = BasicRenderer(nullptr, nullptr);
Framebuffer *TargetFramebuffer = nullptr;

void initialize_kernel(BootInfo *bootInfo) {
    zero_bss();

    renderer = BasicRenderer(bootInfo->framebuffer, bootInfo->font);
    Log::SetRenderer(&renderer);
    global_renderer = &renderer;

    TargetFramebuffer = bootInfo->framebuffer;

    uint32_t *framebuffer = (uint32_t *) TargetFramebuffer->base_address;
    size_t width = TargetFramebuffer->width - 1;
    size_t height = TargetFramebuffer->height - 1;

    const auto fb_base = reinterpret_cast<uint64_t>(TargetFramebuffer->base_address);
    const uint64_t bytes_per_scanline = TargetFramebuffer->pixels_per_scanline * 4;
    const uint64_t fb_height = TargetFramebuffer->height;
    global_renderer->clear();

    kernel::input::InputManager::init();

    /*  render_image_rgba8888_centered(
          TargetFramebuffer,
          Splash_VesperaOS_raw,
          1024,
          1024,
          PIXEL_FORMAT_RGB
      );*/
    // generate_throbber();

    Log::enableDebug();

    gdt_install();

    kernel::memory::initialize_memory(bootInfo);
    kernel::memory::initialize_heap((void *) 0x0000100000000000, 0x500);


    prepare_acpi(bootInfo);
    MADT::parse_madt(ACPI::TableManager::get_madt());
    ACPI::parse_fadt();

    kernel::interrupts::initialize();

    //  ps2::mouse::init();

    asm ("sti");

    //  setup_scroll_buffer(bootInfo->framebuffer);
    s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer, &renderer,
                      bootInfo->font->height);
    scroll_manager = &s;

    Log::init(); // threads are possible -> switch to mutex
    kernel::DeviceManager::Init();

    kernel::SystemManager::initialize();

    CPUManager::initialize();
    RealmManager::initialize();

    RealmConfig realm_config_sys = {
        .name = "system_realm",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    Realm *sys_realm = RealmManager::create(&realm_config_sys);

    RealmConfig realm_config_drv = {
        .name = "driver_realm",
        .memory_limit = 0,
        .max_units = 32,
        .is_user = false,
    };
    RealmManager::create(&realm_config_drv);

    UnitManager::initialize();
    kernel::scheduling::init(CPUManager::total_cpus);

    initialize_input_bus();

    //  RealmManager::list();

    //   UnitManager::list();

    prepare_ap_trampoline();
    CPUManager::smp_init();

    vfs_init();
    DevFS::init();
    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    if (USBManager::wait_for_all_controllers(10000)) {
        Log::Info("All USB controllers ready");
    } else {
        Log::Warning("Timeout waiting for USB controllers (%u/%u ready)",
                     USBManager::get_expected_count(),
                     USBManager::get_initialized_count());
    }
    kernel::tty::initialize_ttys();
    initialize_devices();

    vfs_remount_all();

    FileLogWriter* fw = new FileLogWriter("/var/log/system.log");
    kernel::SystemManager::register_log_writer(fw);
    kernel::SystemManager::process_events_to_logs(128);

    syscall_init();
    install_syscalls();

    kernel::interrupts::mask_pic();
}
