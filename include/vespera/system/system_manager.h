// system_manager.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 10.10.25.
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
#ifndef VESPERAOS_SYSTEM_MANAGER_H
#define VESPERAOS_SYSTEM_MANAGER_H


#include <vespera/types.h>
#include <vespera/ipc/channel.h>
#include <vespera/sync/mutex.h>
#include <vespera/sync/spinlock.h>
#include <vespera/terminal.h>

#include "../../../kernel/acpi/madt.h"

namespace kernel {

    enum class SystemEventType : u8 {
        KERNEL_LOG,
        UNIT_CREATED,
        UNIT_DESTROYED,
        REALM_CREATED,
        REALM_DESTROYED,
        DEVICE_REGISTERED,
        DEVICE_REMOVED,
        MEMORY_LOW,
        CPU_HIGH_USAGE,
        INTERRUPT_STORM,
        FILESYSTEM_MOUNT,
        FILESYSTEM_UNMOUNT,
        SYSTEM_SHUTDOWN,
        SYSTEM_PANIC
    };

    struct SystemEvent {
        SystemEventType type;
        u64 timestamp;
        u32 cpu_id;
        union {
            struct {
                UnitId unit_id;
                RealmId realm_id;
            } unit_event;
            struct {
                RealmId realm_id;
                char name[32];
            } realm_event;
            struct {
                char device_name[32];
                u32 device_id;
            } device_event;
            struct {
                u64 available_bytes;
                u64 threshold_bytes;
            } memory_event;
            struct {
                u32 cpu_id;
                u8 usage_percent;
            } cpu_event;
            struct {
                char message[128];
                u32 error_code;
            } log_event;
            struct {
                char fs_path[256];
                char fs_type[32];
            } fs_event;
        } data;
    };

    struct SystemStats {
        u64 uptime_ms;
        u64 total_memory;
        u64 used_memory;
        u64 free_memory;
        u64 reserved_memory;
        u32 total_units;
        u32 active_units;
        u32 total_realms;
        u32 total_devices;
        u32 total_interrupts;
        u8 cpu_usage[MAX_CPU_CORES];
        u64 last_update_timestamp;
    };

    struct ILogWriter {
        virtual ~ILogWriter() = default;
        virtual bool append_line(const char* line, usize len) = 0;
    };

    class SystemManager {
       public:
        static void initialize();

        static void set_system_initialized();

        static bool is_system_initialized();

        static void set_system_terminal(Terminal* term) {
            system_terminal_ = term;
        }

        static Terminal* get_system_terminal() {
            return system_terminal_;
        }

        // Event-System
        static void publish_event(const SystemEvent& event);
        static Channel* get_event_channel();
        static Channel* get_log_channel();

        // System-Monitoring
        static SystemStats get_system_stats();
        static void update_system_stats();

        // Channel-Management für verschiedene System-Services
        static Channel* create_system_channel(const char* name, usize buffer_size);
        static bool destroy_system_channel(const char* name);
        static Channel* get_system_channel(const char* name);

        static void initiate_shutdown(const char* reason, bool reboot);
        [[noreturn]] static void system_panic(const char* message, i32 error_code);

        // Debug/Monitoring Funktionen
        static void list_system_channels();
        static void dump_system_stats();
        static void enable_event_logging(bool enabled);

        // Helper für andere Manager
        static void notify_unit_lifecycle(UnitId unit_id, RealmId realm_id, bool created);
        static void notify_realm_lifecycle(RealmId realm_id, const char* name, bool created);
        static void notify_device_lifecycle(const char* device_name, u32 device_id, bool registered);
        static void notify_memory_pressure(u64 available_bytes);
        static void notify_filesystem_mount(const char* path, const char* fs_type, bool mounted);

        static void register_log_writer(ILogWriter* writer);
        static void unregister_log_writer();

        static void process_events_to_logs(usize max_events_to_process = 64);

       private:
        struct SystemChannel {
            char name[64];
            Channel* channel;
            u64 created_timestamp;
        };

        static Terminal* system_terminal_;

        static constexpr usize MAX_SYSTEM_CHANNELS = 32;
        static constexpr usize EVENT_CHANNEL_SIZE = static_cast<usize>(64) * 1024;  // 64KB für Events
        static constexpr usize LOG_CHANNEL_SIZE = static_cast<usize>(128) * 1024;   // 128KB für Logs
        static constexpr usize MEMORY_LOW_BYTES_THRESHOLD = static_cast<usize>(64) * 1024 * 1024;

        static bool manager_initialized_;
        static bool system_initialized_;

        static Spinlock global_lock_;
        static Mutex stats_mutex_;

        // Core System Channels
        static Channel* event_channel_;  // Für SystemEvent structs
        static Channel* log_channel_;    // Für Kernel-Logs

        // Managed Channels
        static SystemChannel system_channels_[MAX_SYSTEM_CHANNELS];
        static usize channel_count_;

        // System Statistics
        static SystemStats current_stats_;
        static u64 boot_timestamp_;
        static bool event_logging_enabled_;

        static ILogWriter* log_writer_;

        // Interne Helper
        static void internal_publish_event(const SystemEvent& event);
        static u64 get_current_timestamp();
        static void update_cpu_stats();
        static void update_memory_stats();
        static SystemChannel* find_channel_by_name(const char* name);
    };

// Convenience Macros für Event-Publishing
#define SYS_EVENT_UNIT_CREATED(unit_id, realm_id) kernel::SystemManager::notify_unit_lifecycle(unit_id, realm_id, true)

#define SYS_EVENT_UNIT_DESTROYED(unit_id, realm_id) \
    kernel::SystemManager::notify_unit_lifecycle(unit_id, realm_id, false)

#define SYS_EVENT_REALM_CREATED(realm_id, name) kernel::SystemManager::notify_realm_lifecycle(realm_id, name, true)

#define SYS_EVENT_REALM_DESTROYED(realm_id, name) kernel::SystemManager::notify_realm_lifecycle(realm_id, name, false)

#define SYS_EVENT_DEVICE_REGISTERED(name, id) kernel::SystemManager::notify_device_lifecycle(name, id, true)

#define SYS_EVENT_DEVICE_REMOVED(name, id) kernel::SystemManager::notify_device_lifecycle(name, id, false)

#define SYS_EVENT_FILESYSTEM_MOUNT(path, fs_type) kernel::SystemManager::notify_filesystem_mount(path, fs_type, true)

#define SYS_EVENT_FILESYSTEM_UNMOUNT(path, fs_type) kernel::SystemManager::notify_filesystem_mount(path, fs_type, false)

}  // namespace kernel

#endif  // VESPERAOS_SYSTEM_MANAGER_H
