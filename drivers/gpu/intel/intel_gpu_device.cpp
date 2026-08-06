// intel_gpu_device.cpp
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

#include "intel_gpu_device.h"

#include <pci/msix.h>
#include <pci/pci_device.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/time.h>

namespace blt {

    IntelGpuDevice::IntelGpuDevice(const pci::pci_device& igpu_dev)
        : igp_cfg_(reinterpret_cast<volatile INTEL_IGP_PCI_CONFIG*>(igpu_dev.header)), pci_id_(igpu_dev.id) {
        const phys_addr_t bar0 =
            make_phys(static_cast<u64>(igp_cfg_->gttmmadr_hi) << 32 | (igp_cfg_->gttmmadr_lo & GTTMMADR_ADDR_MASK));

        kernel::memory::map_range(phys_to_virt(bar0), bar0, BAR0_SIZE, (1ULL << CacheDisabled));

        mmio_base_ = static_cast<volatile u8*>(virt_ptr(phys_to_virt(bar0)));
    }

    bool IntelGpuDevice::init() {
        return ggtt_alloc_.init_from_device(mmio_base_, igp_cfg_, pci_id_);
    }

    bool IntelGpuDevice::force_wake_enable(const ForceWakeDomain& domain) const {
        volatile auto* fw_set = reinterpret_cast<volatile u32*>(mmio_base_ + domain.request_reg);
        const volatile auto* fw_ack = reinterpret_cast<volatile u32*>(mmio_base_ + domain.ack_reg);

        *fw_set = domain.enable_value;

        u32 timeout = domain.timeout_us;

        while (timeout--) {
            if (*fw_ack & domain.ack_bit) {
                return true;
            }

            kernel::time::sleep_us(1);
        }

        Log::log_dbc(
            "intel-gpu: ForceWake timeout (request=0x%x ack=0x%x)", domain.request_reg, domain.ack_reg
        );
        return false;
    }

    u8 IntelGpuDevice::allocate_irq_vector(irq_handler_t handler, void* ctx) {
        if (irq_vector_ != INVALID_VECTOR) {
            return irq_vector_;
        }

        const u8 vec = kernel::interrupts::get_free_vector();
        kernel::interrupts::allocate_vector(vec, handler, ctx);

        if (!pci::try_enable_msi_or_msix(reinterpret_cast<volatile pci::PCI_HEADER0*>(igp_cfg_), vec, 1)) {
            Log::log_dbc("intel-gpu: MSI enable failed");
            return INVALID_VECTOR;
        }

        irq_vector_ = vec;
        return irq_vector_;
    }

}  // namespace blt
