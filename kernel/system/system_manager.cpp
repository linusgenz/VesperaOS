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

#include <kernel/system/system_manager.h>
#include "../cpu/cpu_manager.h"
#include <kernel/time.h>
#include <kernel/sync/spinlock.h>
#include "../devices/device_manager.h"
#include <log.h>
#include <string.h>
#include "../utils/panic.h"

namespace kernel {
    bool SystemManager::manager_initialized = false;
    bool SystemManager::system_initialized = false;
    spinlock_t SystemManager::global_lock;

    mutex_t SystemManager::stats_mutex;

    Channel *SystemManager::event_channel = nullptr;
    Channel *SystemManager::log_channel = nullptr;

    SystemManager::SystemChannel SystemManager::system_channels[MAX_SYSTEM_CHANNELS];
    size_t SystemManager::channel_count = 0;

    SystemStats SystemManager::current_stats = {};
    uint64_t SystemManager::boot_timestamp = 0;
    bool SystemManager::event_logging_enabled = true;

    ILogWriter *SystemManager::log_writer = nullptr;

    void SystemManager::initialize() {
        global_lock.init("system_manager_lock");
        global_lock.lock();

        if (manager_initialized) {
            global_lock.unlock();
            return;
        }

        boot_timestamp = get_current_timestamp();

        event_channel = Channel::create(EVENT_CHANNEL_SIZE);
        if (!event_channel) {
            Log::Error("SystemManager: Failed to create event channel");
            global_lock.unlock();
            return;
        }

        log_channel = Channel::create(LOG_CHANNEL_SIZE);
        if (!log_channel) {
            Log::Error("SystemManager: Failed to create log channel");
            Channel::destroy(event_channel);
            event_channel = nullptr;
            global_lock.unlock();
            return;
        }

        memset(system_channels, 0, sizeof(system_channels));
        channel_count = 0;

        memset(&current_stats, 0, sizeof(current_stats));
        current_stats.last_update_timestamp = boot_timestamp;

        global_lock.unlock();

        Log::Info("SystemManager: Initialized successfully");

        manager_initialized = true;

        SystemEvent init_event = {};
        init_event.type = SystemEventType::KERNEL_LOG;
        init_event.timestamp = boot_timestamp;
        init_event.cpu_id = 0;
        strncpy(init_event.data.log_event.message, "SystemManager initialized",
                sizeof(init_event.data.log_event.message) - 1);
        init_event.data.log_event.message[sizeof(init_event.data.log_event.message) - 1] = '\0';
        init_event.data.log_event.error_code = 0;

        internal_publish_event(init_event);
    }

    void SystemManager::set_system_initialized() {
        system_initialized = true;
    }

    bool SystemManager::is_system_initialized() {
        return system_initialized;
    }

    void SystemManager::publish_event(const SystemEvent &event) {
        if (!manager_initialized) return;

        internal_publish_event(event);

        if (event_logging_enabled) {
            switch (event.type) {
                case SystemEventType::SYSTEM_PANIC:
                    Log::Error("SystemManager: PANIC - %s (code: %u)",
                               event.data.log_event.message,
                               event.data.log_event.error_code);
                    break;
                case SystemEventType::MEMORY_LOW:
                    Log::Warning("SystemManager: Low memory - %lu bytes available",
                                 event.data.memory_event.available_bytes);
                    break;
                case SystemEventType::CPU_HIGH_USAGE:
                    Log::Warning("SystemManager: High CPU usage on CPU %u - %u%%",
                                 event.data.cpu_event.cpu_id,
                                 static_cast<unsigned>(event.data.cpu_event.usage_percent));
                    break;
                case SystemEventType::SYSTEM_SHUTDOWN:
                    Log::Info("SystemManager: System shutdown initiated - %s",
                              event.data.log_event.message);
                    break;
                default:
                    break;
            }
        }
    }

    void SystemManager::internal_publish_event(const SystemEvent &event) {
        if (!event_channel) return;


        if (!event_channel->send(&event, sizeof(SystemEvent))) {
            if (log_channel) {
                char buf[128];
                int n = snprintf(buf, sizeof(buf), "SystemManager: Dropped event type %u\n",
                                 static_cast<unsigned>(event.type));
                log_channel->send(buf, n);
            }
        }
    }

    Channel *SystemManager::get_event_channel() {
        return event_channel;
    }

    Channel *SystemManager::get_log_channel() {
        return log_channel;
    }

    SystemStats SystemManager::get_system_stats() {
        // Kopie zurückgeben (thread-safe)
        //  mutex_acquire(&stats_mutex);
        SystemStats copy = current_stats;
        //  mutex_release(&stats_mutex);
        return copy;
    }

    void SystemManager::update_system_stats() {
        if (!manager_initialized) return;

        //  mutex_acquire(&stats_mutex);

        uint64_t now = get_current_timestamp();
        current_stats.uptime_ms = now - boot_timestamp;
        current_stats.last_update_timestamp = now;

        // CPU/Mem/Device/Unit/Realm Stat Updates
        update_memory_stats();
        update_cpu_stats();

        // Unit/Realm Statistics updaten (TODO: Implementationen in jeweiligen Managern)
        // current_stats.total_units = UnitManager::get_total_count();
        // current_stats.active_units = UnitManager::get_active_count();
        // current_stats.total_realms = RealmManager::get_total_count();

        // Device Statistics
        current_stats.total_devices = DeviceManager::GetDeviceCount();

        //  mutex_release(&stats_mutex);
    }

    Channel *SystemManager::create_system_channel(const char *name, size_t buffer_size) {
        if (!manager_initialized || !name) return nullptr;

        global_lock.lock();

        if (find_channel_by_name(name) != nullptr) {
            global_lock.unlock();
            return nullptr;
        }

        if (channel_count >= MAX_SYSTEM_CHANNELS) {
            global_lock.unlock();
            return nullptr;
        }

        Channel *new_channel = Channel::create(buffer_size);
        if (!new_channel) {
            global_lock.unlock();
            return nullptr;
        }

        // Channel registrieren
        SystemChannel *sys_chan = &system_channels[channel_count];
        strncpy(sys_chan->name, name, sizeof(sys_chan->name) - 1);
        sys_chan->name[sizeof(sys_chan->name) - 1] = '\0';
        sys_chan->channel = new_channel;
        sys_chan->created_timestamp = get_current_timestamp();

        channel_count++;

        global_lock.unlock();

        Log::Info("SystemManager: Created system channel '%s' (size: %zu)", name, buffer_size);
        return new_channel;
    }

    bool SystemManager::destroy_system_channel(const char *name) {
        if (!manager_initialized || !name) return false;

        global_lock.lock();

        for (size_t i = 0; i < channel_count; i++) {
            if (strcmp(system_channels[i].name, name) == 0) {
                Channel::destroy(system_channels[i].channel);

                // Array kompaktieren
                for (size_t j = i; j < channel_count - 1; j++) {
                    system_channels[j] = system_channels[j + 1];
                }
                channel_count--;

                global_lock.unlock();
                Log::Info("SystemManager: Destroyed system channel '%s'", name);
                return true;
            }
        }

        global_lock.unlock();
        return false;
    }

    Channel *SystemManager::get_system_channel(const char *name) {
        if (!manager_initialized || !name) return nullptr;

        global_lock.lock();
        SystemChannel *sys_chan = find_channel_by_name(name);
        Channel *result = sys_chan ? sys_chan->channel : nullptr;
        global_lock.unlock();

        return result;
    }

    void SystemManager::initiate_shutdown(const char *reason, bool reboot) {
        SystemEvent shutdown_event = {};
        shutdown_event.type = SystemEventType::SYSTEM_SHUTDOWN;
        shutdown_event.timestamp = get_current_timestamp();
        shutdown_event.cpu_id = CPUManager::get_current_cpu_id();

        if (reason) {
            strncpy(shutdown_event.data.log_event.message, reason,
                    sizeof(shutdown_event.data.log_event.message) - 1);
            shutdown_event.data.log_event.message[sizeof(shutdown_event.data.log_event.message) - 1] = '\0';
        } else {
            strncpy(shutdown_event.data.log_event.message, "User initiated shutdown",
                    sizeof(shutdown_event.data.log_event.message) - 1);
            shutdown_event.data.log_event.message[sizeof(shutdown_event.data.log_event.message) - 1] = '\0';
        }
        shutdown_event.data.log_event.error_code = 0;

        publish_event(shutdown_event);

        if (reboot) {
            ACPI::acpi_reboot();
        } else {
            ACPI::acpi_power_off();
        }
    }

    [[noreturn]] void SystemManager::system_panic(const char *message, int32_t error_code) {
        SystemEvent panic_event = {};
        panic_event.type = SystemEventType::SYSTEM_PANIC;
        panic_event.timestamp = get_current_timestamp();
        panic_event.cpu_id = CPUManager::get_current_cpu_id();

        if (message) {
            strncpy(panic_event.data.log_event.message, message,
                    sizeof(panic_event.data.log_event.message) - 1);
            panic_event.data.log_event.message[sizeof(panic_event.data.log_event.message) - 1] = '\0';
        } else {
            strncpy(panic_event.data.log_event.message, "Unknown panic",
                    sizeof(panic_event.data.log_event.message) - 1);
            panic_event.data.log_event.message[sizeof(panic_event.data.log_event.message) - 1] = '\0';
        }
        panic_event.data.log_event.error_code = error_code;

        internal_publish_event(panic_event);

        Log::Error("KERNEL PANIC: %s (Error Code: %d)",
                   message ? message : "Unknown", error_code);

        panic(message);
    }

    void SystemManager::list_system_channels() {
        if (!manager_initialized) return;

        global_lock.lock();

        Log::Info("SystemManager: Active system channels (%u/%u):",
                  channel_count, MAX_SYSTEM_CHANNELS);

        for (size_t i = 0; i < channel_count; i++) {
            SystemChannel *chan = &system_channels[i];
            Log::Info("  [%zu] %s (created: % ms ago)",
                      i, chan->name,
                      get_current_timestamp() - chan->created_timestamp);
        }

        global_lock.unlock();
    }

    void SystemManager::dump_system_stats() {
        if (!manager_initialized) return;

        update_system_stats();
        SystemStats stats = get_system_stats();

        Log::Info("=== System Statistics ===");
        Log::Info("Uptime: %lu ms", stats.uptime_ms);
        Log::Info("Memory: %lu MB total, %lu MB used, %lu MB free",
                  stats.total_memory / 1024 / 1024,
                  stats.used_memory / 1024 / 1024,
                  stats.free_memory / 1024 / 1024);
        Log::Info("Units: %u total, %u active", stats.total_units, stats.active_units);
        Log::Info("Realms: %u total", stats.total_realms);
        Log::Info("Devices: %u total", stats.total_devices);
        Log::Info("Interrupts: %u total", stats.total_interrupts);
        Log::Info("Last updated: %lu ms ago",
                  get_current_timestamp() - stats.last_update_timestamp);
    }

    void SystemManager::enable_event_logging(bool enabled) {
        event_logging_enabled = enabled;
        Log::Info("SystemManager: Event logging %s", enabled ? "enabled" : "disabled");
    }

    // Event Helper Functions
    void SystemManager::notify_unit_lifecycle(UnitID unit_id, RealmID realm_id, bool created) {
        if (!manager_initialized) return;

        SystemEvent event = {};
        event.type = created ? SystemEventType::UNIT_CREATED : SystemEventType::UNIT_DESTROYED;
        event.timestamp = get_current_timestamp();
        event.cpu_id = CPUManager::get_current_cpu_id();
        event.data.unit_event.unit_id = unit_id;
        event.data.unit_event.realm_id = realm_id;

        publish_event(event);
    }

    void SystemManager::notify_realm_lifecycle(RealmID realm_id, const char *name, bool created) {
        if (!manager_initialized) return;

        SystemEvent event = {};
        event.type = created ? SystemEventType::REALM_CREATED : SystemEventType::REALM_DESTROYED;
        event.timestamp = get_current_timestamp();
        event.cpu_id = CPUManager::get_current_cpu_id();
        event.data.realm_event.realm_id = realm_id;

        if (name) {
            strncpy(event.data.realm_event.name, name,
                    sizeof(event.data.realm_event.name) - 1);
            event.data.realm_event.name[sizeof(event.data.realm_event.name) - 1] = '\0';
        }

        publish_event(event);
    }

    void SystemManager::notify_device_lifecycle(const char *device_name, uint32_t device_id, bool registered) {
        if (!manager_initialized) return;

        SystemEvent event = {};
        event.type = registered ? SystemEventType::DEVICE_REGISTERED : SystemEventType::DEVICE_REMOVED;
        event.timestamp = get_current_timestamp();
        event.cpu_id = CPUManager::get_current_cpu_id();
        event.data.device_event.device_id = device_id;

        if (device_name) {
            strncpy(event.data.device_event.device_name, device_name,
                    sizeof(event.data.device_event.device_name) - 1);
            event.data.device_event.device_name[sizeof(event.data.device_event.device_name) - 1] = '\0';
        }

        publish_event(event);
    }

    void SystemManager::notify_memory_pressure(uint64_t available_bytes) {
        if (!manager_initialized) return;

        SystemEvent event = {};
        event.type = SystemEventType::MEMORY_LOW;
        event.timestamp = get_current_timestamp();
        event.cpu_id = CPUManager::get_current_cpu_id();
        event.data.memory_event.available_bytes = available_bytes;
        event.data.memory_event.threshold_bytes = 64 * 1024 * 1024; // 64MB threshold

        publish_event(event);
    }

    void SystemManager::notify_filesystem_mount(const char *path, const char *fs_type, bool mounted) {
        if (!manager_initialized) return;

        SystemEvent event = {};
        event.type = mounted ? SystemEventType::FILESYSTEM_MOUNT : SystemEventType::FILESYSTEM_UNMOUNT;
        event.timestamp = get_current_timestamp();
        event.cpu_id = CPUManager::get_current_cpu_id();

        if (path) {
            strncpy(event.data.fs_event.fs_path, path,
                    sizeof(event.data.fs_event.fs_path) - 1);
            event.data.fs_event.fs_path[sizeof(event.data.fs_event.fs_path) - 1] = '\0';
        }

        if (fs_type) {
            strncpy(event.data.fs_event.fs_type, fs_type,
                    sizeof(event.data.fs_event.fs_type) - 1);
            event.data.fs_event.fs_type[sizeof(event.data.fs_event.fs_type) - 1] = '\0';
        }

        publish_event(event);
    }

    uint64_t SystemManager::get_current_timestamp() {
        return kernel::time::get_uptime_ms();
    }

    void SystemManager::update_cpu_stats() {
        // TODO: Implement CPU usage calculation mithilfe CPUManager
        memset(current_stats.cpu_usage, 0, sizeof(current_stats.cpu_usage));
        // Wenn CPUManager eine API bietet, hier auslesen:
        // uint32_t cpu_count = CPUManager::total_cpus;
        // for (uint32_t i=0; i<cpu_count && i<32; ++i) current_stats.cpu_usage[i] = CPUManager::get_usage(i);
    }

    void SystemManager::update_memory_stats() {
        current_stats.total_memory = memory::get_total_ram();
        current_stats.used_memory = memory::get_used_ram();
        current_stats.free_memory = memory::get_free_ram();
        current_stats.reserved_memory = memory::get_reserved_ram();
    }

    SystemManager::SystemChannel *SystemManager::find_channel_by_name(const char *name) {
        if (!name) return nullptr;

        for (size_t i = 0; i < channel_count; i++) {
            if (strcmp(system_channels[i].name, name) == 0) {
                return &system_channels[i];
            }
        }
        return nullptr;
    }

    void SystemManager::register_log_writer(ILogWriter *writer) {
        global_lock.lock();
        log_writer = writer;
        global_lock.unlock();
    }

    void SystemManager::unregister_log_writer() {
        global_lock.lock();
        log_writer = nullptr;
        global_lock.unlock();
    }

    void SystemManager::process_events_to_logs(size_t max_events_to_process) {
        if (!manager_initialized || !event_channel) return;
        if (max_events_to_process == 0) return;

        SystemEvent evbuf{};
        size_t processed = 0;

        while (processed < max_events_to_process) {
            ssize_t read_bytes = event_channel->recv(&evbuf, sizeof(evbuf));

            if (read_bytes <= 0) break;

            char line[512];
            int n = 0;
            switch (evbuf.type) {
                case SystemEventType::KERNEL_LOG:
                    n = snprintf(line, sizeof(line), "[%llu] KERNEL_LOG cpu=%u msg=\"%s\" code=%u\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.log_event.message,
                                 evbuf.data.log_event.error_code);
                    break;
                case SystemEventType::UNIT_CREATED:
                    n = snprintf(line, sizeof(line), "[%llu] UNIT_CREATED cpu=%u unit=%u realm=%u\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 static_cast<unsigned>(evbuf.data.unit_event.unit_id),
                                 static_cast<unsigned>(evbuf.data.unit_event.realm_id));
                    break;
                case SystemEventType::UNIT_DESTROYED:
                    n = snprintf(line, sizeof(line), "[%llu] UNIT_DESTROYED cpu=%u unit=%u realm=%u\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 static_cast<unsigned>(evbuf.data.unit_event.unit_id),
                                 static_cast<unsigned>(evbuf.data.unit_event.realm_id));
                    break;
                case SystemEventType::REALM_CREATED:
                    n = snprintf(line, sizeof(line), "[%llu] REALM_CREATED cpu=%u realm=%u name=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 static_cast<unsigned>(evbuf.data.realm_event.realm_id),
                                 evbuf.data.realm_event.name);
                    break;
                case SystemEventType::REALM_DESTROYED:
                    n = snprintf(line, sizeof(line), "[%llu] REALM_DESTROYED cpu=%u realm=%u name=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 static_cast<unsigned>(evbuf.data.realm_event.realm_id),
                                 evbuf.data.realm_event.name);
                    break;
                case SystemEventType::DEVICE_REGISTERED:
                    n = snprintf(line, sizeof(line), "[%llu] DEVICE_REGISTERED cpu=%u id=%u name=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.device_event.device_id,
                                 evbuf.data.device_event.device_name);
                    break;
                case SystemEventType::DEVICE_REMOVED:
                    n = snprintf(line, sizeof(line), "[%llu] DEVICE_REMOVED cpu=%u id=%u name=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.device_event.device_id,
                                 evbuf.data.device_event.device_name);
                    break;
                case SystemEventType::MEMORY_LOW:
                    n = snprintf(line, sizeof(line), "[%llu] MEMORY_LOW cpu=%u available=%lu threshold=%lu\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.memory_event.available_bytes,
                                 evbuf.data.memory_event.threshold_bytes);
                    break;
                case SystemEventType::CPU_HIGH_USAGE:
                    n = snprintf(line, sizeof(line), "[%llu] CPU_HIGH_USAGE cpu=%u usage=%u%%\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.data.cpu_event.cpu_id,
                                 static_cast<unsigned>(evbuf.data.cpu_event.usage_percent));
                    break;
                case SystemEventType::FILESYSTEM_MOUNT:
                    n = snprintf(line, sizeof(line), "[%llu] FILESYSTEM_MOUNT cpu=%u path=\"%s\" type=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.fs_event.fs_path,
                                 evbuf.data.fs_event.fs_type);
                    break;
                case SystemEventType::FILESYSTEM_UNMOUNT:
                    n = snprintf(line, sizeof(line), "[%llu] FILESYSTEM_UNMOUNT cpu=%u path=\"%s\" type=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.fs_event.fs_path,
                                 evbuf.data.fs_event.fs_type);
                    break;
                case SystemEventType::SYSTEM_SHUTDOWN:
                    n = snprintf(line, sizeof(line), "[%llu] SYSTEM_SHUTDOWN cpu=%u msg=\"%s\"\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.log_event.message);
                    break;
                case SystemEventType::SYSTEM_PANIC:
                    n = snprintf(line, sizeof(line), "[%llu] SYSTEM_PANIC cpu=%u msg=\"%s\" code=%u\n",
                                 (uint64_t) evbuf.timestamp,
                                 evbuf.cpu_id,
                                 evbuf.data.log_event.message,
                                 evbuf.data.log_event.error_code);
                    break;
                default:
                    n = snprintf(line, sizeof(line), "[%llu] UNKNOWN_EVENT type=%u\n",
                                 (uint64_t) evbuf.timestamp,
                                 static_cast<unsigned>(evbuf.type));
                    break;
            }

            if (n > 0) {
                if (log_channel) {
                    log_channel->send(line, static_cast<size_t>(n));
                }

                global_lock.lock();
                ILogWriter *writer = log_writer;
                global_lock.unlock();

                if (writer) {
                    if (!writer->append_line(line, static_cast<size_t>(n))) {
                        Log::Warning("SystemManager: LogWriter append failed");
                    }
                }
            }

            processed++;
        }
    }
} // namespace kernel
