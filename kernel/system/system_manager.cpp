// system_manager.cpp
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

#include <acpi/acpi.h>
#include <klib/string.h>
#include <vespera/devices/device_manager.h>
#include <vespera/log.h>
#include <vespera/sync/spinlock.h>
#include <vespera/system/system_manager.h>
#include <vespera/time.h>

#include <filesystem/vfs/fs_detection.h>
#include "../cpu/cpu_manager.h"
#include "../utils/panic.h"
#include "vespera/scheduling.h"

namespace kernel {
    bool SystemManager::manager_initialized_ = false;
    bool SystemManager::system_initialized_ = false;
    Spinlock SystemManager::global_lock_;

    Mutex SystemManager::stats_mutex_;

    Terminal *SystemManager::system_terminal_;

    Channel *SystemManager::event_channel_ = nullptr;
    Channel *SystemManager::log_channel_ = nullptr;

    SystemManager::SystemChannel SystemManager::system_channels_[MAX_SYSTEM_CHANNELS];
    usize SystemManager::channel_count_ = 0;

    SystemStats SystemManager::current_stats_ = {};
    u64 SystemManager::boot_timestamp_ = 0;
    bool SystemManager::event_logging_enabled_ = true;

    ILogWriter *SystemManager::log_writer_ = nullptr;

    void SystemManager::initialize() {
        global_lock_.init("system_manager_lock");
        global_lock_.lock();

        if (manager_initialized_) {
            global_lock_.unlock();
            return;
        }

        boot_timestamp_ = get_current_timestamp();

        event_channel_ = Channel::create(EVENT_CHANNEL_SIZE);
        if (!event_channel_) {
            Log::error("SystemManager: Failed to create event channel");
            global_lock_.unlock();
            return;
        }

        log_channel_ = Channel::create(LOG_CHANNEL_SIZE);
        if (!log_channel_) {
            Log::error("SystemManager: Failed to create log channel");
            Channel::destroy(event_channel_);
            event_channel_ = nullptr;
            global_lock_.unlock();
            return;
        }

        memset(system_channels_, 0, sizeof(system_channels_));
        channel_count_ = 0;

        memset(&current_stats_, 0, sizeof(current_stats_));
        current_stats_.last_update_timestamp = boot_timestamp_;

        global_lock_.unlock();

        Log::info("SystemManager: Initialized successfully");

        manager_initialized_ = true;

        SystemEvent init_event = {};
        init_event.type = SystemEventType::KERNEL_LOG;
        init_event.timestamp = boot_timestamp_;
        init_event.cpu_id = 0;
        strncpy(
            init_event.data.log_event.message,
            "SystemManager initialized",
            sizeof(init_event.data.log_event.message) - 1
        );
        init_event.data.log_event.message[sizeof(init_event.data.log_event.message) - 1] = '\0';
        init_event.data.log_event.error_code = 0;

        internal_publish_event(init_event);
    }

    void SystemManager::set_system_initialized() {
        system_initialized_ = true;
    }

    bool SystemManager::is_system_initialized() {
        return system_initialized_;
    }

    void SystemManager::publish_event(const SystemEvent &event) {
        if (!manager_initialized_) return;

        internal_publish_event(event);

        if (event_logging_enabled_) {
            switch (event.type) {
                case SystemEventType::SYSTEM_PANIC:
                    Log::error(
                        "SystemManager: PANIC - %s (code: %u)",
                        event.data.log_event.message,
                        event.data.log_event.error_code
                    );
                    break;
                case SystemEventType::MEMORY_LOW:
                    Log::warning(
                        "SystemManager: Low memory - %lu bytes available", event.data.memory_event.available_bytes
                    );
                    break;
                case SystemEventType::CPU_HIGH_USAGE:
                    Log::warning(
                        "SystemManager: High CPU usage on CPU %u - %u%%",
                        event.data.cpu_event.cpu_id,
                        static_cast<unsigned>(event.data.cpu_event.usage_percent)
                    );
                    break;
                case SystemEventType::SYSTEM_SHUTDOWN:
                    Log::info("SystemManager: System shutdown initiated - %s", event.data.log_event.message);
                    break;
                default:
                    break;
            }
        }
    }

    void SystemManager::internal_publish_event(const SystemEvent &event) {
        if (!event_channel_) return;

        if (!event_channel_->send(&event, sizeof(SystemEvent))) {
            if (log_channel_) {
                char buf[128];
                const int n = snprintf(
                    buf, sizeof(buf), "SystemManager: Dropped event type %u\n", static_cast<unsigned>(event.type)
                );
                log_channel_->send(buf, n);
            }
        }
    }

    Channel *SystemManager::get_event_channel() {
        return event_channel_;
    }

    Channel *SystemManager::get_log_channel() {
        return log_channel_;
    }

    SystemStats SystemManager::get_system_stats() {
        // Kopie zurückgeben (thread-safe)
        //  mutex_acquire(&stats_mutex);
        const SystemStats copy = current_stats_;
        //  mutex_release(&stats_mutex);
        return copy;
    }

    void SystemManager::update_system_stats() {
        if (!manager_initialized_) return;

        //  mutex_acquire(&stats_mutex);

        const u64 now = get_current_timestamp();
        current_stats_.uptime_ms = now - boot_timestamp_;
        current_stats_.last_update_timestamp = now;

        // CPU/Mem/Device/Unit/Realm Stat Updates
        update_memory_stats();
        update_cpu_stats();

        // Unit/Realm Statistics updaten (TODO: Implementationen in jeweiligen Managern)
        // current_stats.total_units = UnitManager::get_total_count();
        // current_stats.active_units = UnitManager::get_active_count();
        // current_stats.total_realms = RealmManager::get_total_count();

        // Device Statistics
        current_stats_.total_devices = DeviceManager::get_device_count();

        //  mutex_release(&stats_mutex);
    }

    Channel *SystemManager::create_system_channel(const char *name, const usize buffer_size) {
        if (!manager_initialized_ || !name) return nullptr;

        global_lock_.lock();

        if (find_channel_by_name(name) != nullptr) {
            global_lock_.unlock();
            return nullptr;
        }

        if (channel_count_ >= MAX_SYSTEM_CHANNELS) {
            global_lock_.unlock();
            return nullptr;
        }

        Channel *new_channel = Channel::create(buffer_size);
        if (!new_channel) {
            global_lock_.unlock();
            return nullptr;
        }

        // Channel registrieren
        SystemChannel *sys_chan = &system_channels_[channel_count_];
        strncpy(sys_chan->name, name, sizeof(sys_chan->name) - 1);
        sys_chan->name[sizeof(sys_chan->name) - 1] = '\0';
        sys_chan->channel = new_channel;
        sys_chan->created_timestamp = get_current_timestamp();

        channel_count_++;

        global_lock_.unlock();

        Log::info("SystemManager: Created system channel '%s' (size: %zu)", name, buffer_size);
        return new_channel;
    }

    bool SystemManager::destroy_system_channel(const char *name) {
        if (!manager_initialized_ || !name) return false;

        global_lock_.lock();

        for (usize i = 0; i < channel_count_; i++) {
            if (strcmp(system_channels_[i].name, name) == 0) {
                Channel::destroy(system_channels_[i].channel);

                // Array kompaktieren
                for (usize j = i; j < channel_count_ - 1; j++) {
                    system_channels_[j] = system_channels_[j + 1];
                }
                channel_count_--;

                global_lock_.unlock();
                Log::info("SystemManager: Destroyed system channel '%s'", name);
                return true;
            }
        }

        global_lock_.unlock();
        return false;
    }

    Channel *SystemManager::get_system_channel(const char *name) {
        if (!manager_initialized_ || !name) return nullptr;

        global_lock_.lock();
        const SystemChannel *sys_chan = find_channel_by_name(name);
        Channel *result = sys_chan ? sys_chan->channel : nullptr;
        global_lock_.unlock();

        return result;
    }

    void SystemManager::initiate_shutdown(const char *reason, const bool reboot) {
        SystemEvent shutdown_event = {};
        shutdown_event.type = SystemEventType::SYSTEM_SHUTDOWN;
        shutdown_event.timestamp = get_current_timestamp();
        shutdown_event.cpu_id = cpu_manager::get_current_cpu_id();

        if (reason) {
            strncpy(shutdown_event.data.log_event.message, reason, sizeof(shutdown_event.data.log_event.message) - 1);
            shutdown_event.data.log_event.message[sizeof(shutdown_event.data.log_event.message) - 1] = '\0';
        } else {
            strncpy(
                shutdown_event.data.log_event.message,
                "User initiated shutdown",
                sizeof(shutdown_event.data.log_event.message) - 1
            );
            shutdown_event.data.log_event.message[sizeof(shutdown_event.data.log_event.message) - 1] = '\0';
        }
        shutdown_event.data.log_event.error_code = 0;

        publish_event(shutdown_event);

        FilesystemDetector::unmount_all();

        DeviceManager::shutdown_all();

        if (reboot) {
            acpi::reboot();
        } else {
            acpi::power_off();
        }
    }

    [[noreturn]] void SystemManager::system_panic(const char *message, const i32 error_code) {
        SystemEvent panic_event = {};
        panic_event.type = SystemEventType::SYSTEM_PANIC;
        panic_event.timestamp = get_current_timestamp();
        panic_event.cpu_id = cpu_manager::get_current_cpu_id();

        if (message) {
            strncpy(panic_event.data.log_event.message, message, sizeof(panic_event.data.log_event.message) - 1);
            panic_event.data.log_event.message[sizeof(panic_event.data.log_event.message) - 1] = '\0';
        } else {
            strncpy(
                panic_event.data.log_event.message, "Unknown panic", sizeof(panic_event.data.log_event.message) - 1
            );
            panic_event.data.log_event.message[sizeof(panic_event.data.log_event.message) - 1] = '\0';
        }
        panic_event.data.log_event.error_code = error_code;

        internal_publish_event(panic_event);

        Log::error("KERNEL PANIC: %s (Error Code: %d)", message ? message : "Unknown", error_code);

        panic(message);
    }

    void SystemManager::list_system_channels() {
        if (!manager_initialized_) return;

        global_lock_.lock();

        Log::info("SystemManager: Active system channels (%u/%u):", channel_count_, MAX_SYSTEM_CHANNELS);

        for (usize i = 0; i < channel_count_; i++) {
            SystemChannel *chan = &system_channels_[i];
            Log::info(
                "  [%zu] %s (created: % ms ago)", i, chan->name, get_current_timestamp() - chan->created_timestamp
            );
        }

        global_lock_.unlock();
    }

    void SystemManager::dump_system_stats() {
        if (!manager_initialized_) return;

        update_system_stats();
        const SystemStats stats = get_system_stats();

        Log::info("=== System Statistics ===");
        Log::info("Uptime: %lu ms", stats.uptime_ms);
        Log::info(
            "Memory: %lu MB total, %lu MB used, %lu MB free",
            stats.total_memory / 1024 / 1024,
            stats.used_memory / 1024 / 1024,
            stats.free_memory / 1024 / 1024
        );
        Log::info("Units: %u total, %u active", stats.total_units, stats.active_units);
        Log::info("Realms: %u total", stats.total_realms);
        Log::info("Devices: %u total", stats.total_devices);
        Log::info("Interrupts: %u total", stats.total_interrupts);
        Log::info("Last updated: %lu ms ago", get_current_timestamp() - stats.last_update_timestamp);
    }

    void SystemManager::enable_event_logging(const bool enabled) {
        event_logging_enabled_ = enabled;
        Log::info("SystemManager: Event logging %s", enabled ? "enabled" : "disabled");
    }

    // Event Helper Functions
    void SystemManager::notify_unit_lifecycle(const UnitId unit_id, const RealmId realm_id, const bool created) {
        if (!manager_initialized_) return;

        SystemEvent event = {};
        event.type = created ? SystemEventType::UNIT_CREATED : SystemEventType::UNIT_DESTROYED;
        event.timestamp = get_current_timestamp();
        event.cpu_id = cpu_manager::get_current_cpu_id();
        event.data.unit_event.unit_id = unit_id;
        event.data.unit_event.realm_id = realm_id;

        publish_event(event);
    }

    void SystemManager::notify_realm_lifecycle(const RealmId realm_id, const char *name, const bool created) {
        if (!manager_initialized_) return;

        SystemEvent event = {};
        event.type = created ? SystemEventType::REALM_CREATED : SystemEventType::REALM_DESTROYED;
        event.timestamp = get_current_timestamp();
        event.cpu_id = cpu_manager::get_current_cpu_id();
        event.data.realm_event.realm_id = realm_id;

        if (name) {
            strncpy(event.data.realm_event.name, name, sizeof(event.data.realm_event.name) - 1);
            event.data.realm_event.name[sizeof(event.data.realm_event.name) - 1] = '\0';
        }

        publish_event(event);
    }

    void SystemManager::notify_device_lifecycle(const char *device_name, const u32 device_id, const bool registered) {
        if (!manager_initialized_) return;

        SystemEvent event = {};
        event.type = registered ? SystemEventType::DEVICE_REGISTERED : SystemEventType::DEVICE_REMOVED;
        event.timestamp = get_current_timestamp();
        event.cpu_id = cpu_manager::get_current_cpu_id();
        event.data.device_event.device_id = device_id;

        if (device_name) {
            strncpy(event.data.device_event.device_name, device_name, sizeof(event.data.device_event.device_name) - 1);
            event.data.device_event.device_name[sizeof(event.data.device_event.device_name) - 1] = '\0';
        }

        publish_event(event);
    }

    void SystemManager::notify_memory_pressure(const u64 available_bytes) {
        if (!manager_initialized_) return;

        SystemEvent event = {};
        event.type = SystemEventType::MEMORY_LOW;
        event.timestamp = get_current_timestamp();
        event.cpu_id = cpu_manager::get_current_cpu_id();
        event.data.memory_event.available_bytes = available_bytes;
        event.data.memory_event.threshold_bytes = MEMORY_LOW_BYTES_THRESHOLD;

        publish_event(event);
    }

    void SystemManager::notify_filesystem_mount(const char *path, const char *fs_type, const bool mounted) {
        if (!manager_initialized_) return;

        SystemEvent event = {};
        event.type = mounted ? SystemEventType::FILESYSTEM_MOUNT : SystemEventType::FILESYSTEM_UNMOUNT;
        event.timestamp = get_current_timestamp();
        event.cpu_id = cpu_manager::get_current_cpu_id();

        if (path) {
            strncpy(event.data.fs_event.fs_path, path, sizeof(event.data.fs_event.fs_path) - 1);
            event.data.fs_event.fs_path[sizeof(event.data.fs_event.fs_path) - 1] = '\0';
        }

        if (fs_type) {
            strncpy(event.data.fs_event.fs_type, fs_type, sizeof(event.data.fs_event.fs_type) - 1);
            event.data.fs_event.fs_type[sizeof(event.data.fs_event.fs_type) - 1] = '\0';
        }

        publish_event(event);
    }

    u64 SystemManager::get_current_timestamp() {
        return time::get_uptime_ms();
    }

    void SystemManager::update_cpu_stats() {
        // TODO: Implement CPU usage calculation mithilfe CPUManager
        memset(current_stats_.cpu_usage, 0, sizeof(current_stats_.cpu_usage));
        // Wenn CPUManager eine API bietet, hier auslesen:
        // u32 cpu_count = CPUManager::total_cpus;
        // for (u32 i=0; i<cpu_count && i<32; ++i) current_stats.cpu_usage[i] = CPUManager::get_usage(i);
    }

    void SystemManager::update_memory_stats() {
        current_stats_.total_memory = memory::get_total_ram();
        current_stats_.used_memory = memory::get_used_ram();
        current_stats_.free_memory = memory::get_free_ram();
        current_stats_.reserved_memory = memory::get_reserved_ram();
    }

    SystemManager::SystemChannel *SystemManager::find_channel_by_name(const char *name) {
        if (!name) return nullptr;

        for (usize i = 0; i < channel_count_; i++) {
            if (strcmp(system_channels_[i].name, name) == 0) {
                return &system_channels_[i];
            }
        }
        return nullptr;
    }

    void SystemManager::register_log_writer(ILogWriter *writer) {
        global_lock_.lock();
        log_writer_ = writer;
        global_lock_.unlock();
    }

    void SystemManager::unregister_log_writer() {
        global_lock_.lock();
        log_writer_ = nullptr;
        global_lock_.unlock();
    }

    void SystemManager::process_events_to_logs(const usize max_events_to_process) {
        if (!manager_initialized_ || !event_channel_) return;
        if (max_events_to_process == 0) return;

        SystemEvent evbuf{};
        usize processed = 0;

        while (processed < max_events_to_process) {
            if (const isize read_bytes = event_channel_->recv(&evbuf, sizeof(evbuf)); read_bytes <= 0) break;

            char line[512];
            int n = 0;
            switch (evbuf.type) {
                case SystemEventType::KERNEL_LOG:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] KERNEL_LOG cpu=%u msg=\"%s\" code=%u\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.log_event.message,
                        evbuf.data.log_event.error_code
                    );
                    break;
                case SystemEventType::UNIT_CREATED:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] UNIT_CREATED cpu=%u unit=%u realm=%u\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        static_cast<unsigned>(evbuf.data.unit_event.unit_id),
                        static_cast<unsigned>(evbuf.data.unit_event.realm_id)
                    );
                    break;
                case SystemEventType::UNIT_DESTROYED:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] UNIT_DESTROYED cpu=%u unit=%u realm=%u\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        static_cast<unsigned>(evbuf.data.unit_event.unit_id),
                        static_cast<unsigned>(evbuf.data.unit_event.realm_id)
                    );
                    break;
                case SystemEventType::REALM_CREATED:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] REALM_CREATED cpu=%u realm=%u name=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        static_cast<unsigned>(evbuf.data.realm_event.realm_id),
                        evbuf.data.realm_event.name
                    );
                    break;
                case SystemEventType::REALM_DESTROYED:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] REALM_DESTROYED cpu=%u realm=%u name=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        static_cast<unsigned>(evbuf.data.realm_event.realm_id),
                        evbuf.data.realm_event.name
                    );
                    break;
                case SystemEventType::DEVICE_REGISTERED:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] DEVICE_REGISTERED cpu=%u id=%u name=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.device_event.device_id,
                        evbuf.data.device_event.device_name
                    );
                    break;
                case SystemEventType::DEVICE_REMOVED:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] DEVICE_REMOVED cpu=%u id=%u name=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.device_event.device_id,
                        evbuf.data.device_event.device_name
                    );
                    break;
                case SystemEventType::MEMORY_LOW:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] MEMORY_LOW cpu=%u available=%lu threshold=%lu\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.memory_event.available_bytes,
                        evbuf.data.memory_event.threshold_bytes
                    );
                    break;
                case SystemEventType::CPU_HIGH_USAGE:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] CPU_HIGH_USAGE cpu=%u usage=%u%%\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.data.cpu_event.cpu_id,
                        static_cast<unsigned>(evbuf.data.cpu_event.usage_percent)
                    );
                    break;
                case SystemEventType::FILESYSTEM_MOUNT:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] FILESYSTEM_MOUNT cpu=%u path=\"%s\" type=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.fs_event.fs_path,
                        evbuf.data.fs_event.fs_type
                    );
                    break;
                case SystemEventType::FILESYSTEM_UNMOUNT:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] FILESYSTEM_UNMOUNT cpu=%u path=\"%s\" type=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.fs_event.fs_path,
                        evbuf.data.fs_event.fs_type
                    );
                    break;
                case SystemEventType::SYSTEM_SHUTDOWN:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] SYSTEM_SHUTDOWN cpu=%u msg=\"%s\"\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.log_event.message
                    );
                    break;
                case SystemEventType::SYSTEM_PANIC:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] SYSTEM_PANIC cpu=%u msg=\"%s\" code=%u\n",
                        static_cast<u64>(evbuf.timestamp),
                        evbuf.cpu_id,
                        evbuf.data.log_event.message,
                        evbuf.data.log_event.error_code
                    );
                    break;
                default:
                    n = snprintf(
                        line,
                        sizeof(line),
                        "[%llu] UNKNOWN_EVENT type=%u\n",
                        static_cast<u64>(evbuf.timestamp),
                        static_cast<unsigned>(evbuf.type)
                    );
                    break;
            }

            if (n > 0) {
                if (log_channel_) {
                    log_channel_->send(line, static_cast<usize>(n));
                }

                global_lock_.lock();
                ILogWriter *writer = log_writer_;
                global_lock_.unlock();

                if (writer) {
                    if (!writer->append_line(line, static_cast<usize>(n))) {
                        Log::warning("SystemManager: LogWriter append failed");
                    }
                }
            }

            processed++;
        }
    }
}  // namespace kernel
