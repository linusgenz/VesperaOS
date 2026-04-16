#include "pci.h"

#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "../../include/drivers/pci/pci_driver.h"
#include "pci_device.h"

namespace pci {

    namespace {

        pci_device make_device(PCI_HEADER0* header, const pci_id& id) {
            pci_device dev;
            dev.id = id;
            dev.header = header;
            dev.vendor_id = header->header.vendor_id;
            dev.device_id = header->header.device_id;
            dev.class_code = header->header._class;
            dev.subclass = header->header.subclass;
            dev.prog_if = header->header.prog_if;
            dev.revision = header->header.revision_id;
            return dev;
        }

    }  // namespace

    static void enumerate_function(const u64 device_address, const pci_id& id) {
        const phys_addr_t func_phys = make_phys(device_address + (static_cast<u64>(id.function) << 12));
        const virt_addr_t func_virt = phys_to_virt(func_phys);

        kernel::memory::map_memory(func_virt, func_phys);

        auto* header = virt_as<PCI_HEADER0>(func_virt);

        if (header->header.device_id == 0 || header->header.device_id == 0xFFFF) return;
        if (header->header.vendor_id == 0 || header->header.vendor_id == 0xFFFF) return;

        pci_device dev = make_device(header, id);

        if (!driver_registry::bind(dev)) {
          /*  Log::debug(
                "pci: no driver for %04x:%02x:%02x.%x (vid=%04x did=%04x class=%02x/%02x/%02x)",
                id.domain,
                id.bus,
                id.device,
                id.function,
                dev.vendor_id,
                dev.device_id,
                dev.class_code,
                dev.subclass,
                dev.prog_if
            );*/
        }
    }

    static void enumerate_device(const u64 bus_address, const u8 bus, const u8 device, const u16 domain) {
        const virt_addr_t dev_virt = virt_from_raw(bus_address + (static_cast<u64>(device) << 15));
        const phys_addr_t dev_phys = make_phys(virt_raw(dev_virt));

        kernel::memory::map_memory(dev_virt, dev_phys, 0);

        const auto* hdr = virt_as<PCI_DEVICE_HEADER>(dev_virt);
        if (hdr->device_id == 0 || hdr->device_id == 0xFFFF) return;

        for (u8 function = 0; function < 8; ++function) {
            const pci_id id = {.domain = domain, .bus = bus, .device = device, .function = function};
            enumerate_function(virt_raw(dev_virt), id);
        }
    }

    static void enumerate_bus(const u64 base_address, const u8 bus, const u16 domain) {
        const virt_addr_t bus_virt = virt_from_raw(base_address + (static_cast<u64>(bus) << 20));
        const phys_addr_t bus_phys = make_phys(virt_raw(bus_virt));

        kernel::memory::map_memory(bus_virt, bus_phys, 0);

        const auto* hdr = virt_as<PCI_DEVICE_HEADER>(bus_virt);
        if (hdr->device_id == 0 || hdr->device_id == 0xFFFF) return;

        for (u8 device = 0; device < 32; ++device) {
            enumerate_device(virt_raw(bus_virt), bus, device, domain);
        }
    }

    void enumerate_pci(kernel::acpi::MCFG_HEADER* mcfg) {
        const u32 entries =
            (mcfg->header.length - sizeof(kernel::acpi::MCFG_HEADER)) / sizeof(kernel::acpi::DEVICE_CONFIG);

        for (usize t = 0; t < entries; ++t) {
            const auto* cfg = reinterpret_cast<kernel::acpi::DEVICE_CONFIG*>(
                reinterpret_cast<u64>(mcfg) + sizeof(kernel::acpi::MCFG_HEADER) +
                sizeof(kernel::acpi::DEVICE_CONFIG) * t
            );

            const u16 domain = static_cast<u16>(t);

            for (u8 bus = cfg->start_bus; bus < cfg->end_bus; ++bus) {
                enumerate_bus(cfg->base_address, bus, domain);
            }
        }
    }
}  // namespace pci