// acpi.cpp
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

#include <acpi/acpi.h>

extern "C" {
#include "acpica/include/acpi.h"
}

#include <klib/string.h>
#include <vespera/log.h>

namespace kernel::acpi {
    static ACPI_HANDLE to_handle(acpi_handle_t h) {
        return static_cast<ACPI_HANDLE>(h.ptr);
    }

    static acpi_handle_t from_handle(ACPI_HANDLE h) {
        return acpi_handle_t{h};
    }

    static u32 acpi_int(const ACPI_OBJECT& obj) {
        return obj.Type == ACPI_TYPE_INTEGER
                   ? static_cast<u32>(obj.Integer.Value)
                   : 0xFFFFFFFFu;
    }

    static void copy_acpi_string(const ACPI_OBJECT& obj, char* dst, usize dst_size) {
        if (obj.Type == ACPI_TYPE_STRING && obj.String.Pointer) {
            const usize len = obj.String.Length < dst_size - 1
                                  ? obj.String.Length
                                  : dst_size - 1;
            memcpy(dst, obj.String.Pointer, len);
            dst[len] = '\0';
        } else {
            dst[0] = '\0';
        }
    }

    static UINT32 to_acpi_notify_type(notify_type t) {
        return t == notify_type::all ? ACPI_ALL_NOTIFY : ACPI_DEVICE_NOTIFY;
    }

    // Trampoline context: maps ACPICA callback signature → our typed one.
    struct notify_trampoline_ctx {
        acpi_notify_fn fn;
        void* user_ctx;
        acpi_handle_t device;
    };

    static void acpi_notify_trampoline(ACPI_HANDLE handle, UINT32 event, void* ctx) {
        auto* t = static_cast<notify_trampoline_ctx*>(ctx);
        t->fn(from_handle(handle), static_cast<u32>(event), t->user_ctx);
    }

    // ─── enumerate_devices ────────────────────────────────────────────────────

    struct enum_ctx {
        acpi_device_fn cb;
        void* user_ctx;
    };

    static ACPI_STATUS enum_callback(
        ACPI_HANDLE object, UINT32 /*nesting*/, void* context, void** /*ret*/
    ) {
        auto* ctx = static_cast<enum_ctx*>(context);
        const bool cont = ctx->cb(from_handle(object), ctx->user_ctx);
        return cont ? AE_OK : AE_CTRL_TERMINATE;
    }

    void enumerate_devices(const char* hid, acpi_device_fn cb, void* context) {
        enum_ctx ctx{cb, context};
        AcpiGetDevices(
            const_cast<char*>(hid),
            enum_callback,
            &ctx,
            nullptr
        );
    }

    // ─── install_notify / remove_notify ───────────────────────────────────────
    //
    // NOTE: The trampoline context is heap-allocated and intentionally leaked
    // on remove (ACPICA gives us no way to retrieve it on removal anyway, so
    // a remove_notify call must keep a pointer to the ctx alive separately if
    // precise cleanup is needed — for now kernel lifetime is sufficient).

    bool install_notify(
        acpi_handle_t device,
        notify_type type,
        acpi_notify_fn fn,
        void* context
    ) {
        auto* ctx = new notify_trampoline_ctx{fn, context, device};
        const ACPI_STATUS st = AcpiInstallNotifyHandler(
            to_handle(device),
            to_acpi_notify_type(type),
            acpi_notify_trampoline,
            ctx
        );
        if (ACPI_FAILURE(st)) {
            delete ctx;
            Log::warning("acpi_iface: install_notify failed: %s", AcpiFormatException(st));
            return false;
        }
        return true;
    }

    void remove_notify(acpi_handle_t device, notify_type type, acpi_notify_fn fn) {
        // We cannot recover the exact trampoline pointer here, so we uninstall
        // by passing nullptr as the handler — ACPICA will remove the entry that
        // matches the (handle, type) pair. This is safe for our single-handler
        // per-device pattern.
        (void)fn;
        AcpiRemoveNotifyHandler(
            to_handle(device),
            to_acpi_notify_type(type),
            acpi_notify_trampoline
        );
    }

    // ─── evaluate_integer ────────────────────────────────────────────────────

    eval_result evaluate_integer(acpi_handle_t device, const char* path) {
        ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
        const ACPI_STATUS st = AcpiEvaluateObject(to_handle(device), const_cast<char*>(path), nullptr, &buf);
        if (!buf.Pointer) return {false, {0}};
        if (ACPI_FAILURE(st)) {
            Log::warning("acpi: evaluate_integer(\"%s\") failed: %s", path, AcpiFormatException(st));
            return {false, {0}};
        }
        const auto* obj = static_cast<ACPI_OBJECT*>(buf.Pointer);
        eval_result res{};
        if (obj->Type == ACPI_TYPE_INTEGER) {
            res.ok = true;
            res.integer = obj->Integer.Value;
        }
        AcpiOsFree(buf.Pointer);
        return res;
    }

    // ─── evaluate_string ─────────────────────────────────────────────────────

    bool evaluate_string(acpi_handle_t device, const char* path, char* dst, usize dst_size) {
        ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
        const ACPI_STATUS st = AcpiEvaluateObject(to_handle(device), const_cast<char*>(path), nullptr, &buf);
        if (ACPI_FAILURE(st) || !buf.Pointer) {
            if (dst && dst_size > 0) dst[0] = '\0';
            return false;
        }
        const auto* obj = static_cast<ACPI_OBJECT*>(buf.Pointer);
        bool ok = false;
        if (obj->Type == ACPI_TYPE_STRING && obj->String.Pointer && dst) {
            copy_acpi_string(*obj, dst, dst_size);
            ok = true;
        }
        AcpiOsFree(buf.Pointer);
        return ok;
    }

    // ─── evaluate_void ───────────────────────────────────────────────────────

    bool evaluate_void(acpi_handle_t device, const char* path, bool ignore_not_found) {
        const ACPI_STATUS st = AcpiEvaluateObject(
            to_handle(device),
            const_cast<char*>(path),
            nullptr,
            nullptr
        );
        if (ACPI_FAILURE(st)) {
            if (ignore_not_found && st == AE_NOT_FOUND) return true;
            return false;
        }
        return true;
    }

    // ─── battery_present (_STA) ───────────────────────────────────────────────

    bool battery_present(acpi_handle_t battery) {
        ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
        if (ACPI_FAILURE(AcpiEvaluateObject(to_handle(battery), "_STA", nullptr, &buf)) || !buf.Pointer) {
            return true; // Assume present if _STA unavailable
        }
        const auto* obj = static_cast<ACPI_OBJECT*>(buf.Pointer);
        const bool present = (obj->Type == ACPI_TYPE_INTEGER) && (obj->Integer.Value & 0x10);
        AcpiOsFree(buf.Pointer);
        return present;
    }

    // ─── query_bst ────────────────────────────────────────────────────────────

    bool query_bst(acpi_handle_t battery, bst_data& out) {
        ACPI_BUFFER result = {ACPI_ALLOCATE_BUFFER, nullptr};
        const ACPI_STATUS st = AcpiEvaluateObject(to_handle(battery), "_BST", nullptr, &result);
        if (ACPI_FAILURE(st) || !result.Pointer) {
            return false;
        }
        const auto* pkg = static_cast<ACPI_OBJECT*>(result.Pointer);
        if (pkg->Type != ACPI_TYPE_PACKAGE || pkg->Package.Count < 4) {
            AcpiOsFree(result.Pointer);
            return false;
        }
        const ACPI_OBJECT* e = pkg->Package.Elements;
        out.state = acpi_int(e[0]);
        out.present_rate = acpi_int(e[1]);
        out.remaining_capacity = acpi_int(e[2]);
        out.present_voltage = acpi_int(e[3]);
        AcpiOsFree(result.Pointer);
        return true;
    }

    // ─── query_bif (tries _BIX first, falls back to _BIF) ────────────────────

    bool query_bif(acpi_handle_t battery, bif_data& out) {
        ACPI_BUFFER result = {ACPI_ALLOCATE_BUFFER, nullptr};
        bool use_bix = ACPI_SUCCESS(AcpiEvaluateObject(to_handle(battery), "_BIX", nullptr, &result))
            && result.Pointer;
        if (!use_bix) {
            result = {ACPI_ALLOCATE_BUFFER, nullptr};
            if (ACPI_FAILURE(AcpiEvaluateObject(to_handle(battery), "_BIF", nullptr, &result)) || !result.Pointer) {
                return false;
            }
        }
        const auto* pkg = static_cast<ACPI_OBJECT*>(result.Pointer);
        if (pkg->Type != ACPI_TYPE_PACKAGE) {
            AcpiOsFree(result.Pointer);
            return false;
        }
        const ACPI_OBJECT* e = pkg->Package.Elements;
        const u32 n = pkg->Package.Count;
        out = {};

        if (use_bix) {
            if (n > 2) out.design_capacity = acpi_int(e[2]);
            if (n > 3) out.last_full_capacity = acpi_int(e[3]);
            if (n > 5) out.design_voltage = acpi_int(e[5]);
            if (n > 6) out.capacity_warning = acpi_int(e[6]);
            if (n > 12) copy_acpi_string(e[12], out.model, sizeof(out.model));
            if (n > 13) copy_acpi_string(e[13], out.serial, sizeof(out.serial));
            if (n > 14) copy_acpi_string(e[14], out.type, sizeof(out.type));
            if (n > 15) copy_acpi_string(e[15], out.oem, sizeof(out.oem));
        } else {
            if (n > 1) out.design_capacity = acpi_int(e[1]);
            if (n > 2) out.last_full_capacity = acpi_int(e[2]);
            if (n > 4) out.design_voltage = acpi_int(e[4]);
            if (n > 5) out.capacity_warning = acpi_int(e[5]);
            if (n > 9) copy_acpi_string(e[9], out.model, sizeof(out.model));
            if (n > 10) copy_acpi_string(e[10], out.serial, sizeof(out.serial));
            if (n > 11) copy_acpi_string(e[11], out.type, sizeof(out.type));
            if (n > 12) copy_acpi_string(e[12], out.oem, sizeof(out.oem));
        }
        AcpiOsFree(result.Pointer);
        return true;
    }

    struct walk_ctx {
        acpi_walk_fn cb;
        void* user_ctx;
    };

    static ACPI_STATUS walk_cb(ACPI_HANDLE object, UINT32 nesting, void* context, void** /*ret*/) {
        auto* ctx = static_cast<walk_ctx*>(context);
        const bool cont = ctx->cb(from_handle(object), static_cast<u32>(nesting), ctx->user_ctx);
        return cont ? AE_OK : AE_CTRL_TERMINATE;
    }

    static ACPI_OBJECT_TYPE to_acpi_type(acpi_object_type t) {
        switch (t) {
        case acpi_object_type::thermal: return ACPI_TYPE_THERMAL;
        case acpi_object_type::device: return ACPI_TYPE_DEVICE;
        default: return ACPI_TYPE_ANY;
        }
    }

    void walk_namespace(acpi_object_type type, acpi_walk_fn cb, void* context) {
        walk_ctx ctx{cb, context};
        AcpiWalkNamespace(
            to_acpi_type(type),
            ACPI_ROOT_OBJECT,
            ACPI_UINT32_MAX,
            walk_cb,
            nullptr,
            &ctx,
            nullptr
        );
    }

    bool get_object_name(acpi_handle_t object, char* dst, usize dst_size) {
        ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
        if (ACPI_FAILURE(AcpiGetName(to_handle(object), ACPI_SINGLE_NAME, &buf)) || !buf.Pointer) {
            if (dst && dst_size > 0) dst[0] = '\0';
            return false;
        }
        const auto src = static_cast<const char*>(buf.Pointer);
        const usize len = strlen(src);
        const usize copy = len < dst_size - 1 ? len : dst_size - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
        AcpiOsFree(buf.Pointer);
        return true;
    }


    io_port_pair get_io_ports(acpi_handle_t device) {
        io_port_pair result{};
        ACPI_BUFFER buf = {ACPI_ALLOCATE_BUFFER, nullptr};
        if (ACPI_FAILURE(AcpiGetCurrentResources(to_handle(device), &buf)) || !buf.Pointer) {
            return result;
        }
        auto* res = static_cast<ACPI_RESOURCE*>(buf.Pointer);
        u16 ports[2] = {0, 0};
        int port_idx = 0;
        while (res->Type != ACPI_RESOURCE_TYPE_END_TAG && port_idx < 2) {
            if (res->Type == ACPI_RESOURCE_TYPE_IO) {
                ports[port_idx++] = res->Data.Io.Minimum;
            }
            res = ACPI_NEXT_RESOURCE(res);
        }
        AcpiOsFree(buf.Pointer);
        if (port_idx == 2) {
            result.data = ports[0];
            result.cmd = ports[1];
            result.valid = true;
        }
        return result;
    }

    [[noreturn]] void power_off() {
        ACPI_STATUS st = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
        if (ACPI_FAILURE(st)) {
            Log::error("acpi: power_off: AcpiEnterSleepStatePrep failed: %s", AcpiFormatException(st));
        }
        asm volatile("cli");
        st = AcpiEnterSleepState(ACPI_STATE_S5);
        if (ACPI_FAILURE(st)) {
            Log::error("acpi: power_off: AcpiEnterSleepState failed: %s", AcpiFormatException(st));
        }
        while (true) asm volatile("cli; hlt");
    }

    [[noreturn]] void reboot() {
        const ACPI_STATUS st = AcpiReset();
        if (ACPI_FAILURE(st)) {
            Log::error("acpi: reboot: AcpiReset failed - keyboard controller fallback");
            // PS/2 controller reset
            asm volatile("outb %0, %1" : : "a"(static_cast<u8>(0xFE)), "d"(static_cast<u16>(0x64)));
        }
        while (true) asm volatile("cli; hlt");
    }
} // namespace kernel::acpi
