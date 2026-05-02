// acpi.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.05.26.
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

#ifndef VESPERAOS_INCLUDE_ACPI_ACPI_H
#define VESPERAOS_INCLUDE_ACPI_ACPI_H

#include <vespera/types.h>

namespace kernel::acpi {

    struct acpi_handle_t {
        void* ptr = nullptr;

        [[nodiscard]] bool valid() const { return ptr != nullptr; }
    };


    // Signature of a device-specific notification callback.
    // event: raw ACPI notification code (e.g. 0x80 = status changed)
    using acpi_notify_fn = void (*)(acpi_handle_t device, u32 event, void* context);

    // Callback invoked once per matching ACPI device during enumerate_devices().
    // Return false to stop enumeration early.
    using acpi_device_fn = bool (*)(acpi_handle_t device, void* context);

    // Enumerate all ACPI devices with the given HID (e.g. "PNP0C0A").
    // Calls cb for each match.
    void enumerate_devices(const char* hid, acpi_device_fn cb, void* context);

    enum class notify_type : u8 {
        device  = 0,  // ACPI_DEVICE_NOTIFY  (0x00–0x7F)
        all     = 1,  // ACPI_ALL_NOTIFY     (both ranges)
    };

    // Install a notify handler on a device handle.
    // Returns true on success.
    [[nodiscard]] bool install_notify(
        acpi_handle_t device,
        notify_type type,
        acpi_notify_fn fn,
        void* context
    );

    // Remove a previously installed notify handler.
    void remove_notify(acpi_handle_t device, notify_type type, acpi_notify_fn fn);

    // Result type for evaluate_integer / evaluate_string.
    struct eval_result {
        bool ok = false;
        union {
            u64 integer;
            // For strings, see evaluate_string() below.
        };
    };

    // Evaluate an ACPI method/object that returns an integer.
    [[nodiscard]] eval_result evaluate_integer(acpi_handle_t device, const char* path);

    // Evaluate an ACPI method/object that returns a string.
    // Copies at most dst_size-1 characters into dst and null-terminates.
    // Returns true on success.
    [[nodiscard]] bool evaluate_string(
        acpi_handle_t device,
        const char* path,
        char* dst,
        usize dst_size
    );

    // Evaluate an ACPI method with no arguments and no return value interest.
    // Returns true if evaluation succeeded (or AE_NOT_FOUND is acceptable when
    // ignore_not_found is true).
    [[nodiscard]] bool evaluate_void(
        acpi_handle_t device,
        const char* path,
        bool ignore_not_found = false
    );

    // _BST / _BIF / _BIX return packages of integers and strings.
    // Rather than exposing ACPI_OBJECT to drivers, we provide typed structs.

    struct bst_data {
        u32 state;               // Battery State    (bit 0=discharging, 1=charging, 2=critical)
        u32 present_rate;        // mW or mA (depending on power_unit)
        u32 remaining_capacity;  // mWh or mAh
        u32 present_voltage;     // mV
    };

    struct bif_data {
        u32 design_capacity;
        u32 last_full_capacity;
        u32 design_voltage;
        u32 capacity_warning;
        char model[64];
        char serial[64];
        char type[16];
        char oem[64];
    };

    // Query _BST (battery status).  Returns true on success.
    [[nodiscard]] bool query_bst(acpi_handle_t battery, bst_data& out);

    // Query _BIX or fallback _BIF.  Returns true on success.
    [[nodiscard]] bool query_bif(acpi_handle_t battery, bif_data& out);

    // Check battery present via _STA.  Returns false if absent.
    [[nodiscard]] bool battery_present(acpi_handle_t battery);

    enum class acpi_object_type : u8 {
        thermal = 0,  // ACPI_TYPE_THERMAL
        device  = 1,  // ACPI_TYPE_DEVICE
        // extend as needed
    };

    // Callback for walk_namespace().  Return false to stop traversal early.
    using acpi_walk_fn = bool (*)(acpi_handle_t object, u32 nesting, void* context);

    // Walk the ACPI namespace for all objects of the given type.
    void walk_namespace(acpi_object_type type, acpi_walk_fn cb, void* context);

    // Retrieve the single-component name (4-char ACPI name) of an object.
    // Copies at most dst_size-1 characters into dst and null-terminates.
    [[nodiscard]] bool get_object_name(acpi_handle_t object, char* dst, usize dst_size);

    struct io_port_pair {
        u16 data;  // first IO port from _CRS
        u16 cmd;   // second IO port from _CRS
        bool valid = false;
    };

    // Read the first two IO port resources from a device's _CRS.
    [[nodiscard]] io_port_pair get_io_ports(acpi_handle_t device);

    [[noreturn]] void power_off();
    [[noreturn]] void reboot();

} // namespace kernel::acpi

#endif // VESPERAOS_INCLUDE_ACPI_ACPI_H
