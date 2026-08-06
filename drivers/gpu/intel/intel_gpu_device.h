// intel_gpu_device.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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
#ifndef VESPERAOS_INTEL_GPU_DEVICE_H
#define VESPERAOS_INTEL_GPU_DEVICE_H

#include <pci/pci_id.h>
#include <vespera/interrupts.h>
#include <vespera/mm/addr.h>

#include "ggtt_allocator.h"
#include "pci_config_regs.h"

namespace pci {
    struct pci_device;
}

namespace blt {

    constexpr u64 GTTMMADR_ADDR_MASK = ~0xFULL;
    constexpr usize BAR0_SIZE = 16ull * 1024 * 1024;

    /**
     * @brief Descriptor for one Gen9 ForceWake power domain.
     *
     * Gen9 splits ForceWake per engine — Render, Blitter, and Media each have
     * their own request+ack MMIO register pair (see FORCEWAKE_RENDER in
     * intel_rcs.h, FORCEWAKE_BLITTER in intel_bcs.h). IntelGpuDevice itself
     * has no opinion on which domains exist or what their offsets are — it
     * just drives whichever descriptor the calling IntelEngine hands it.
     */
    struct ForceWakeDomain {
        u32 request_reg;  ///< MMIO offset of the request register
        u32 ack_reg;       ///< MMIO offset of the ack register
        u32 enable_value;  ///< value to write to the request register
        u32 ack_bit;       ///< bit to poll for in the ack register
        u32 timeout_us;
    };

    /**
     * @brief Everything shared by every command-streamer engine on one
     *        physical Intel iGPU: BAR0 mapping, PCI config, and the single
     *        GGTT.
     *
     * One instance per physical GPU, constructed and init()'d by
     * IntelBltPciDriver before any engine (IntelBcs, IntelRcs) is
     * constructed — every engine's constructor takes a reference to this and
     * borrows its MMIO base / GGTT via IntelEngine's protected accessors.
     * Neither engine owns this device, and neither engine owns the other.
     */
    class IntelGpuDevice {
       public:
        /// Sentinel returned by allocate_irq_vector() on failure.
        static constexpr u8 INVALID_VECTOR = 0xFF;

        explicit IntelGpuDevice(const pci::pci_device& igpu_dev);

        IntelGpuDevice(const IntelGpuDevice&) = delete;
        IntelGpuDevice& operator=(const IntelGpuDevice&) = delete;

        /// Sets up the GGTT (host bridge GGC + GMADR/MSAC discovery). BAR0 is
        /// already mapped by the constructor. Must succeed before any
        /// engine's init_device() is called.
        [[nodiscard]] bool init();

        [[nodiscard]] volatile u8* mmio_base() const {
            return mmio_base_;
        }
        [[nodiscard]] GgttAllocator& ggtt() {
            return ggtt_alloc_;
        }
        [[nodiscard]] volatile INTEL_IGP_PCI_CONFIG* pci_cfg() const {
            return igp_cfg_;
        }
        [[nodiscard]] const pci::pci_id& pci_id() const {
            return pci_id_;
        }

        /// Drives one ForceWake domain: write request, poll ack, timeout.
        /// Stateless with respect to which domain is being woken — the
        /// caller supplies its own descriptor (see IntelEngine::engine_force_wake_enable()).
        [[nodiscard]] bool force_wake_enable(const ForceWakeDomain& domain) const;

        /// Lazily allocates one shared MSI/MSI-X vector for this device and
        /// wires it up via try_enable_msi_or_msix().
        [[nodiscard]] u8 allocate_irq_vector(irq_handler_t handler, void* ctx);

       private:
        volatile INTEL_IGP_PCI_CONFIG* igp_cfg_;
        pci::pci_id pci_id_;
        volatile u8* mmio_base_ = nullptr;
        GgttAllocator ggtt_alloc_;
        u8 irq_vector_ = INVALID_VECTOR;
    };

}  // namespace blt

#endif  // VESPERAOS_INTEL_GPU_DEVICE_H
