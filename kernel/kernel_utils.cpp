#include "./include/kernel_utils.h"

#include "throbber.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "../drivers/input/ps2/keyboard/ps2_keyboard.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include "../include/log.h"
#include "memory/stack_manager.h"
#include "cpu/cpu_manager.h"
#include <scheduling.h>
#include "include/interrupts.h"
#include "../drivers/input/ps2/mouse/mouse.h"
#include "../drivers/input/ps2/mouse/ps2_mouse.h"
#include "../filesystem/fat32/fat32.h"
#include "../filesystem/fat32/fat32_vfs_adapter.h"
#include "../filesystem/vfs/fs_registry.h"
#include "../filesystem/vfs/vfs.h"
#include "devices/device_manager.h"
#include "sys/syscall_interface.h"
#include "include/time.h"
#include "../filesystem/vfs/vfs.h"
#include "include/sys/syscalls.h"
#include "input/input_manager.h"
#include "proc/process_manager.h"
#include "scheduling/thread_manager.h"
#include "threading/threading.h"
#include "tty/tty.h"


void prepare_memory(BootInfo *bootInfo) {
    const uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

    kernel::memory::initialize_page_frame_allocator(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

    const uint64_t kernelStart = reinterpret_cast<uint64_t>(&_KernelStart);
    const uint64_t kernelEnd = reinterpret_cast<uint64_t>(&_KernelEnd);
    const uint64_t kernelSize = kernelEnd - kernelStart;
    const uint64_t kernelPages = kernelSize / 4096 + 1;

    kernel::memory::lock_pages(&_KernelStart, kernelPages);
    kernel::memory::lock_pages(nullptr, 256);

    kernel::memory::initialize_page_table_manager();

    // just map everythin cuz it works lol. might not be a good practice tho, needs refactoring prob
    for (int i = 0; i < mMapEntries; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *) (
            (uint64_t) bootInfo->mMap + (i * bootInfo->mMapDescSize));
        //  if (desc->type != 7) continue; // Nur EfiConventionalMemory

        for (uint64_t addr = desc->phys_addr; addr < desc->phys_addr + desc->num_pages * 0x1000; addr += 0x1000) {
            kernel::memory::map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
        }
    }

    for (uint64_t addr = kernelStart; addr < kernelEnd; addr += 0x1000) {
        kernel::memory::map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr),  (1ULL << PT_Flag::UserSuper));
    }

    for (uint32_t i = 0; i < CPUManager::total_cpus; ++i) {
        void *stack_addr = (void *) (KERNEL_STACK_BASE + i * KERNEL_STACK_SIZE);
        kernel::memory::map_memory(stack_addr, stack_addr,
                                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
    }

    kernel::memory::map_memory((void *) 0x8000, (void *) 0x8000,
                               (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
    kernel::memory::map_memory((void *) 0x7000, (void *) 0x7000,
                               (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
    kernel::memory::map_memory((void *) 0x6000, (void *) 0x6000,
                               (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
    kernel::memory::map_memory((void *) 0x1000, (void *) 0x1000,
                               (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
    kernel::memory::map_memory((void *) 0x2000, (void *) 0x2000, (1ULL << PT_Flag::CacheDisabled));

    uint64_t fb_base = (uint64_t) bootInfo->framebuffer->base_address;
    uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;
    kernel::memory::lock_pages((void *) fb_base, fb_size / 0x1000 + 1);
    for (uint64_t t = fb_base; t < fb_base + fb_size; t += 0x1000) {
        kernel::memory::map_memory((void *) t, (void *) t);
    }

    asm ("mov %0, %%cr3" : : "r" (kernel::memory::get_pagetable_address()));
}

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

void prepare_ap_trampoline() {
    *(volatile uint64_t *) 0x2000 = kernel::memory::get_pagetable_address();
    *(arch::x86_64::interrupts::idt::IDTR *) 0x1000 = *kernel::interrupts::get_idtr_address();
    //   __asm__ volatile("wbinvd" ::: "memory");
}

extern uint8_t __bss_start[];
extern uint8_t __bss_end[];

void zero_bss() {
    uint8_t *bss = __bss_start;
    while (bss < __bss_end) {
        *bss++ = 0;
    }
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

void input_poll_thread(void *arg) {
    kernel::input::InputEvent ev;
    memset(&ev, 0, sizeof(ev));
   while (true) {
        while (kernel::input::InputManager::pop_event(ev)) {
            kernel::tty::tty_handle_input(ev);
        }
      //  kernel::scheduling::yield(); // CPU an andere Threads
    }
}

extern uint8_t Splash_VesperaOS_raw[]; // Aus xxd -i
extern unsigned int Splash_VesperaOS_raw_len;
ScrollManager s = ScrollManager(nullptr, nullptr, nullptr, nullptr);
static BasicRenderer renderer = BasicRenderer(nullptr, nullptr);
Framebuffer *TargetFramebuffer = nullptr;

void initialize_kernel(BootInfo *bootInfo) {
    zero_bss();
    renderer = BasicRenderer(bootInfo->framebuffer, bootInfo->psf1_font);
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

    kernel::tty::tty_init();
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

    prepare_memory(bootInfo);
    kernel::memory::initialize_heap((void *) 0x0000100000000000, 0x500);

    prepare_acpi(bootInfo);
    MADT::parse_madt(ACPI::TableManager::get_madt());
    ACPI::parse_fadt();

    kernel::interrupts::initialize();

    //  ps2::mouse::init();

    asm ("sti");

    //  setup_scroll_buffer(bootInfo->framebuffer);
    s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer, &renderer);
    scroll_manager = &s;

    Log::init(); // threads are possible -> switch to mutex
    kernel::DeviceManager::Init();


    CPUManager::initialize();
    kernel::process::Manager::initialize();
    kernel::scheduling::init(CPUManager::total_cpus);

    kernel::process::ProcessCreateOptions options = {
        .name = "input_poll_process",
        .cpu_id = 2,
        .heap_start = 0,
        .heap_size = 0,
        .stack_size = THREAD_STACK_SIZE,
        .is_kernel_process = true,
    };
    kprocess_t *shell_proc = PROCESS_MANAGER::create_kernel_process(options, input_poll_thread, nullptr);

    prepare_ap_trampoline();
    CPUManager::smp_init();

    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());


    /*  kernel::process::ProcessCreateOptions options = {
          .name = "throbber",
          .cpu_id = 2,
          .heap_start = 0,
          .heap_size = 0,
          .stack_size = 0x1000,
          .is_kernel_process = true,
          .custom_pml4 = nullptr
      };

      PROCESS_MANAGER::create_kernel_process(options, render_throbber, nullptr);*/



    syscall_init();
    install_syscalls();

    //   Log::Info("Free RAM: %u mb", kernel::memory::get_free_ram() / 1024 / 1024);
    //   Log::Info("Reserved RAM: %u mb", kernel::memory::get_reserved_ram() / 1024 / 1024);
    //   Log::Info("Used RAM: %u mb", kernel::memory::get_used_ram() / 1024 / 1024);

    kernel::interrupts::mask_pic();
}
