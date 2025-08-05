#include "./include/kernel_utils.h"
#include "../arch/x86_64/gdt/gdt.h"
#include "../arch/x86_64/syscalls/syscall.h"
#include "../drivers/input/ps2/keyboard/ps2_keyboard.h"
#include "acpi/acpi_manager.h"
#include "acpi/madt.h"
#include "../include/log.h"
#include "memory/stack_manager.h"
#include "cpu/cpu_manager.h"
#include "include/scheduler.h"
#include "include/interrupts.h"
#include "../drivers/input/ps2/mouse/mouse.h"
#include "../drivers/input/ps2/mouse/ps2_mouse.h"
#include "../filesystem/fat32/fat32.h"
#include "../filesystem/fat32/fat32_vfs_adapter.h"
#include "../filesystem/vfs/fs_registry.h"
#include "../filesystem/vfs/vfs.h"
#include "devices/device_manager.h"
#include "sys/syscall_interface.h"
#include "time/time.h"

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
        kernel::memory::map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
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

extern FileSystemDriver fat32_driver;

void vfs_system_init() {
    vfs_init();

    register_fs_driver(&fat32_driver);

    auto devices = kernel::DeviceManager::GetDevices();
    size_t device_count = kernel::DeviceManager::GetDeviceCount();

    Log::debug("device count: %d", device_count);

    int mount_index = 0;
    for (size_t i = 0; i < device_count; ++i) {
        BlockDevice *dev = devices[i];
        if (!dev) continue;

        char mount_path[32];
        snprintf(mount_path, sizeof(mount_path), "/mnt/sd%d", mount_index++);

        // Nutze das VFS-Treiber-System, nicht manuell FAT32 aufrufen!
        int result = vfs_mount(dev, mount_path, "fat32");

        if (result == 0) {
            Log::Info("Mounted FAT32 at %s", mount_path);
        } else {
            Log::Warning("Failed to mount FAT32 at %s (code %d)", mount_path, result);
        }
    }

    if (mount_index == 0) {
        Log::Warning("No FAT32 volumes found.");
    }
}

ScrollManager s = ScrollManager(nullptr, nullptr, nullptr, nullptr);
static BasicRenderer renderer = BasicRenderer(nullptr, nullptr);
Framebuffer *TargetFramebuffer = nullptr;

void initialize_kernel(BootInfo *bootInfo) {
    zero_bss();
    renderer = BasicRenderer(bootInfo->framebuffer, bootInfo->psf1_font);
    Log::SetRenderer(&renderer);
    global_renderer = &renderer;
    memset(bootInfo->framebuffer->base_address, 0, bootInfo->framebuffer->buffer_size);

    TargetFramebuffer = bootInfo->framebuffer;
    Log::enableDebug();

    gdt_install();

    prepare_memory(bootInfo);
    kernel::memory::initialize_heap((void *) 0x0000100000000000, 0x500);

    prepare_acpi(bootInfo);
    MADT::parse_madt(ACPI::TableManager::get_madt());
    ACPI::parse_fadt();

    kernel::interrupts::initialize();

    ps2::mouse::init();

    asm ("sti");

    setup_scroll_buffer(bootInfo->framebuffer);
    s = ScrollManager(scroll_buffer_top, scroll_buffer_bottom, bootInfo->framebuffer, &renderer);
    scroll_manager = &s;

    kernel::DeviceManager::Init();

    PCI::enumerate_pci(ACPI::TableManager::get_mcfg());

    CPUManager::initialize();
    kernel::scheduling::init(CPUManager::total_cpus);
    Log::init(); // threads are possible -> switch to mutex
    prepare_ap_trampoline();

    CPUManager::smp_init();

    //  CPUManager::print_cpu_info();

    //  StackManager::print_stack_info();

    syscall_init();
    install_syscalls();

    vfs_system_init();

    //   Log::Info("Free RAM: %u mb", kernel::memory::get_free_ram() / 1024 / 1024);
    //   Log::Info("Reserved RAM: %u mb", kernel::memory::get_reserved_ram() / 1024 / 1024);
    //   Log::Info("Used RAM: %u mb", kernel::memory::get_used_ram() / 1024 / 1024);

    kernel::interrupts::mask_pic();
}

