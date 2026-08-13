#include "pci.h"

#include <drivers/pci/pci_driver.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "pci_device.h"
#include "vespera/time.h"

namespace pci {

    namespace {

        pci_device make_device(volatile PCI_HEADER0* header, const pci_id& id) {
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

    enum class BindFilter {
        XHCI_ONLY,
        SKIP_XHCI,
    };

    static constexpr usize MAX_DOMAINS = 4;
    static const volatile INTEL_HB_PCI_CONFIG* s_host_bridges[MAX_DOMAINS] = {};

    const volatile INTEL_HB_PCI_CONFIG* get_host_bridge(const u16 domain) {
        if (domain >= MAX_DOMAINS) return nullptr;
        return s_host_bridges[domain];
    }


    static void enumerate_function(const u64 device_address, const pci_id& id, const BindFilter filter) {
        const phys_addr_t func_phys = make_phys(device_address + (static_cast<u64>(id.function) << 12));
        const virt_addr_t func_virt = phys_to_virt(func_phys);

        kernel::memory::map_memory(func_virt, func_phys, (1ULL << PtFlag::ReadWrite));

        auto* header = virt_as<volatile PCI_HEADER0>(func_virt);

        if (header->header.device_id == 0 || header->header.device_id == 0xFFFF) return;
        if (header->header.vendor_id == 0 || header->header.vendor_id == 0xFFFF) return;

        pci_device dev = make_device(header, id);

        /*Log::log_msg("[ PCI ] %s (%llx) %s (%llx) %s %s", get_vendor_name(header->header.vendor_id), header->header.vendor_id,
                    get_device_name(header->header.vendor_id, header->header.device_id), header->header.device_id,
                    get_subclass_name(header->header._class, header->header.subclass),
                    get_prog_if_name(header->header._class, header->header.subclass,
                                     header->header.prog_if));*/

        const bool is_xhci = (dev.class_code == 0x0C && dev.subclass == 0x03 && dev.prog_if == 0x30);

        if (filter == BindFilter::XHCI_ONLY && !is_xhci) return;
        if (filter == BindFilter::SKIP_XHCI && is_xhci) return;

        if (id.bus == 0 && id.device == 0 && id.function == 0) {
            if (id.domain < MAX_DOMAINS) {
                s_host_bridges[id.domain] = reinterpret_cast<const volatile INTEL_HB_PCI_CONFIG*>(header);
                Log::debug("pci: cached host bridge for domain %u", id.domain);
            }
        }

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

    static void enumerate_device(
        const u64 bus_address, const u8 bus, const u8 device, const u16 domain, const BindFilter filter
    ) {
        const virt_addr_t dev_virt = virt_from_raw(bus_address + (static_cast<u64>(device) << 15));
        const phys_addr_t dev_phys = make_phys(virt_raw(dev_virt));

        kernel::memory::map_memory(dev_virt, dev_phys, (1ULL << PtFlag::ReadWrite));

        const auto* hdr = virt_as<PCI_DEVICE_HEADER>(dev_virt);
        if (hdr->device_id == 0 || hdr->device_id == 0xFFFF) return;

        for (u8 function = 0; function < 8; ++function) {
            const pci_id id = {.domain = domain, .bus = bus, .device = device, .function = function};
            enumerate_function(virt_raw(dev_virt), id, filter);
        }
    }

    static void enumerate_bus(const u64 base_address, const u8 bus, const u16 domain, const BindFilter filter) {
        const virt_addr_t bus_virt = virt_from_raw(base_address + (static_cast<u64>(bus) << 20));
        const phys_addr_t bus_phys = make_phys(virt_raw(bus_virt));

        kernel::memory::map_memory(bus_virt, bus_phys, (1ULL << PtFlag::ReadWrite));

        const auto* hdr = virt_as<PCI_DEVICE_HEADER>(bus_virt);
        if (hdr->device_id == 0 || hdr->device_id == 0xFFFF) return;

        for (u8 device = 0; device < 32; ++device) {
            enumerate_device(virt_raw(bus_virt), bus, device, domain, filter);
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
                enumerate_bus(cfg->base_address, bus, domain, BindFilter::XHCI_ONLY);
            }
        }

     //   kernel::time::sleep_ms(4000);

        for (usize t = 0; t < entries; ++t) {
            const auto* cfg = reinterpret_cast<kernel::acpi::DEVICE_CONFIG*>(
                reinterpret_cast<u64>(mcfg) + sizeof(kernel::acpi::MCFG_HEADER) +
                sizeof(kernel::acpi::DEVICE_CONFIG) * t
            );

            const u16 domain = static_cast<u16>(t);

            for (u8 bus = cfg->start_bus; bus < cfg->end_bus; ++bus) {
                enumerate_bus(cfg->base_address, bus, domain, BindFilter::SKIP_XHCI);
            }
        }
    }
}  // namespace pci