// intel_blt_pci_driver.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.05.26.
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

#include "intel_blt_pci_driver.h"

#include <klib/string.h>
#include <pci/pci.h>
#include <vespera/graphics/display_manager.h>
#include <vespera/log.h>
#include <vespera/time.h>

#include "bcs/intel_bcs.h"
#include "core/intel_gpu_device.h"
#include "rcs/intel_rcs.h"

namespace gpu::intel {
    /**
     * @brief Device-ID match table for Intel integrated GPUs (Gen9.5).
     *
     * Covers Kaby Lake, Kaby Lake-R, Coffee Lake, Whiskey Lake, and Amber
     * Lake, whose Blitter and Render engines share the same programming
     * model as documented in the Kaby Lake PRM.
     *
     * Generations outside this range (Gen9.0 Skylake and earlier, Gen11 Ice
     * Lake and later) differ in MMIO layout, command-stream submission, or
     * render compression and are intentionally excluded until a
     * per-generation abstraction layer is in place.
     *
     * @note Intel integrated GPUs may appear as PCI class 0x03 subclass 0x00
     *       (VGA-compatible) or subclass 0x02 (3D controller) depending on
     *       generation and firmware - match by device ID, not by class code.
     */
    static constexpr pci::pci_device_match INTEL_BLT_ID_MATCH[] = {
        // --- Kaby Lake (Gen9.5) ---
        {0x8086, 0x5902}, // HD 610
        {0x8086, 0x5906}, // HD 610
        {0x8086, 0x590B}, // HD 620
        {0x8086, 0x5912}, // HD 630
        {0x8086, 0x5916}, // HD 620
        {0x8086, 0x5917}, // UHD 620
        {0x8086, 0x591B}, // HD P630
        {0x8086, 0x591D}, // Xeon E3 P630

        // --- Coffee Lake (Gen9.5 - same PRM-Familie) ---
        {0x8086, 0x3E90}, // UHD 610
        {0x8086, 0x3E91}, // UHD 630
        {0x8086, 0x3E92}, // UHD 630
        {0x8086, 0x3E96}, // P630
        {0x8086, 0x3E98}, // UHD 630
        {0x8086, 0x3E9B}, // UHD 630

        // --- Whiskey Lake / Amber Lake (Gen9.5) ---
        {0x8086, 0x3EA0}, // UHD 620 (Whiskey Lake-U)
        {0x8086, 0x3EA5}, // UHD 620
        {0x8086, 0x87CA}, // UHD 617 (Amber Lake-Y / Kaby Lake-G)

        // Sentinel
        {pci::PCI_ID_ANY, pci::PCI_ID_ANY},
    };

    const pci::pci_device_match* IntelBltPciDriver::id_match() const {
        return INTEL_BLT_ID_MATCH;
    }

    int IntelBltPciDriver::probe(pci::pci_device& dev) {
        // Only bind function 0; other endpoints on higher functions are
        // handled by dedicated drivers.
        if (dev.id.function != 0) {
            Log::debug(
                "intel-blt: skipping function %u on %04x:%02x:%02x",
                dev.id.function,
                dev.id.domain,
                dev.id.bus,
                dev.id.device
            );
            return -1;
        }

        // Guard against a second GPU being probed (integrated + discrete mixed).
        if (device_ != nullptr) {
            Log::warning(
                "intel-blt: second Intel display controller detected on "
                "%04x:%02x:%02x.%u — skipping",
                dev.id.domain,
                dev.id.bus,
                dev.id.device,
                dev.id.function
            );
            return -1;
        }

        auto* pci_hdr = &dev.header->header;

        // Enable Bus Master + Memory Space; disable legacy INTx.
        u16 command = pci::pci_read16(pci_hdr, 0x04);
        command |= (1u << 2) | (1u << 1); // Bus Master + Memory Space
        command |= (1u << 10);            // Disable INTx
        pci::pci_write16(pci_hdr, 0x04, command);

        Log::info(
            "intel-blt: found Intel display controller at %04x:%02x:%02x.%u "
            "(vendor=%04x device=%04x)",
            dev.id.domain,
            dev.id.bus,
            dev.id.device,
            dev.id.function,
            dev.vendor_id,
            dev.device_id
        );

        const auto [drv, kd] = DisplayManager::primary();
        Resolution resolution = {
            drv ? drv->screen_width_px() : 1920,
            drv ? drv->screen_height_px() : 1080
        };

        // One device: owns BAR0 mapping and the single GGTT. Every engine
        // constructed below borrows this same instance — neither owns it,
        // neither owns the other.
        device_ = new core::IntelGpuDevice(dev);
        if (!device_->init()) {
            Log::error("intel-blt: device init (BAR0/GGTT) failed");
            delete device_;
            device_ = nullptr;
            return -1;
        }

        bcs_ = new bcs::IntelBcs(*device_);
        if (!bcs_->init_device()) {
            Log::error("intel-blt: BCS init failed");
            delete bcs_;
            bcs_ = nullptr;
            delete device_;
            device_ = nullptr;
            return -2;
        }

        rcs_ = new rcs::IntelRcs(*device_);
        if (!rcs_->init_device(resolution)) {
            Log::warning("intel-blt: RCS init failed, continuing with BCS only");
            delete rcs_;
            rcs_ = nullptr;
        } else {
            rcs_->set_bcs(bcs_);
        }

        bcs_->start_device(resolution);

        device_->add_bcs(bcs_);

        const DisplayBackend be{device_, device_->get_kd()};
        DisplayManager::set_primary(be);

        if (rcs_) {
            rcs_->present_to_screen(resolution);
        }

        while (1); // as long as we develop rcs

        return 0;
    }

    void IntelBltPciDriver::remove(pci::pci_device& /*dev*/) {
        // IDK how this should happen, when you remove the iGPU you remove the processor, so how should this code even
        // be executed lol

        if (rcs_ != nullptr) {
            delete rcs_;
            rcs_ = nullptr;
        }
        if (bcs_ != nullptr) {
            delete bcs_;
            bcs_ = nullptr;
        }
        if (device_ != nullptr) {
            delete device_;
            device_ = nullptr;
        }
    }

    static IntelBltPciDriver g_intel_blt_pci_driver;
    PCI_DRIVER_REGISTER(g_intel_blt_pci_driver);
} // namespace blt
