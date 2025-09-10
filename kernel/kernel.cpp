// kernel.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 02.08.25.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "throbber.h"
#include "exec/elf.h"
#include "./include/kernel_utils.h"
#include "./cpu/cpu.h"
#include "include/time.h"
#include "version.h"
#include "include/sys/syscalls.h"
#include "../include/log.h"
#include "acpi/acpi_manager.h"
#include "include/scheduling.h"
#include "proc/process_manager.h"
#include "sync/mutex.h"

#include "../filesystem/vfs/vfs.h"
#include "input/input_manager.h"
#include "threading/threading.h"
#include "tty/tty.h"

extern "C" void switch_to_user_mode(void *user_stack_top, void *user_code_virt);

extern "C" void usermode_write_test() {
    const char *msg = "Hello from Ring 3!\n";
    sys_write(1, msg, strlen(msg));
    sys_exit(0);
}

void *load_elf_binary(const char *path, uint64_t *entry_out, uintptr_t USER_BASE) {
    VfsNode *file = vfs_open(path);
    if (!file) {
        Log::Error("Failed to open file %s", path);
        return nullptr;
    };

    size_t size = vfs_file_size(file);
    void *file_data = kernel::memory::malloc(size);
    vfs_read(file, 0, size, file_data);

    auto *header = reinterpret_cast<Elf64_Ehdr *>(file_data);

    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        Log::Error("Invalid ELF file");
        return nullptr;
    }

    auto *phdrs = reinterpret_cast<Elf64_Phdr *>(
        reinterpret_cast<uint8_t *>(file_data) + header->e_phoff
    );

    for (int i = 0; i < header->e_phnum; ++i) {
        Elf64_Phdr &ph = phdrs[i];

        if (ph.p_type != 1) continue; // PT_LOAD

        // 3. Zieladresse im Virtuellen Speicher
        void *seg_vaddr = reinterpret_cast<void *>(USER_BASE + ph.p_vaddr);
        size_t filesz = ph.p_filesz;
        size_t memsz = ph.p_memsz;

        Log::debug("Mapping segment: vaddr = %p, filesz = %d, memsz = %d", seg_vaddr, filesz, memsz);

        kernel::memory::map_range(seg_vaddr, seg_vaddr, memsz, ph.p_flags); // flags = R/W/X

        memcpy(seg_vaddr,
               reinterpret_cast<uint8_t *>(file_data) + ph.p_offset,
               filesz);

        //  zeroing bss
        if (memsz > filesz) {
            memset(reinterpret_cast<uint8_t *>(seg_vaddr) + filesz, 0, memsz - filesz);
        }
    }

    *entry_out = USER_BASE + header->e_entry;
    return reinterpret_cast<void *>(header->e_entry);
}

extern "C" [[noreturn]] void kernel_main(BootInfo *boot_info) {
    system_initialized = false;
    initialize_kernel(boot_info);
    kernel::scheduling_started = true;
    char vendor[13];
    get_cpu_vendor(vendor);
    //  Log::Info("CPU Vendor: %s", vendor);

    char brand[49];
    get_cpu_brand(brand);
    //  Log::Info("CPU Brand: %s", brand);
    Log::Ok("Kernel initialized successfully");
    //  Log::Info("Kernel version: %s", get_os_version());
    //  kernel::time::print_current_time();
    kernel::time::internal::sleep(5000);

    vfs_init();


    /* auto dir21 = vfs_opendir("/mnt/fat32_0/");
     Log::debug("vfs_opendir: %d", dir21);
     char name21[128];
     while (vfs_readdir(dir21, name21, sizeof(name21)) == 1) {
         Log::PrintLn("Eintrag2: %s", name21);
     }

     vfs_closedir(dir21);*/


    kernel::process::ProcessCreateOptions options_shell = {
        .name = "shell",
        .priority = 0,
        .cpu_id = 0,
        .heap_start = 0,
        .heap_size = 0,
        .stack_size = 0x3000,
        .is_kernel_process = false,
        .custom_pml4 = nullptr,
    };
    kprocess_t *shell_proc = PROCESS_MANAGER::create_process_from_elf(options_shell, "/mnt/fat32_0/bin/shell.elf");

    /*    constexpr kernel::threading::ThreadCreateParams thread_params = {
            .name = "shell",
            .priority = 0,
            .cpu_id = 0,
            .stack_size = 0x1000,
            .custom_stack = nullptr,
            .is_idle_thread = false,
            .is_user_thread = true,
            .process = nullptr,
        };
        ElfLoader loader;
        auto result = loader.load_elf_binary("/mnt/fat32_0/bin/shell.elf", 0x300000);

        if (!result.success) {
            Log::Error("failed to load shell: %s", result.error_message);
        }
        Log::LogMsg("entry: %p", result.entry_point);

        kernel::time::internal::sleep(1000);

        void *user_stack_phys = kernel::memory::request_page();
        kernel::memory::map_memory((void *) user_stack_phys, user_stack_phys, (1ULL << PT_Flag::UserSuper));
        void *user_stack_top = (void *) (user_stack_phys + 0x1000);

        uint64_t entry;
        void* start_addr = load_elf_binary("/mnt/fat32_0/bin/shell.elf", &entry, 0x400000);
    */
    /*if (start_addr) {
        Log::Info("Jumping to ELF entry point at %p", entry);
        switch_to_user_mode(user_stack_top,  (void*)entry);
    }
    else {
        Log::Warning("not able to load elf. %p %p", start_addr, entry);
    }

    // switch_to_user_mode(pg,  (void*)result.entry_point);
    auto t = kernel::threading::ThreadFactory::create_user_thread(thread_params, (void*)entry, user_stack_top);
    kernel::scheduling::add_thread(t);*/
    //    kernel::process::util::print_process_list();
    //    kernel::process::util::print_thread_list();


    system_initialized = true;
    kernel::scheduling::enable_on_cpu(0);

    while (true);
}
