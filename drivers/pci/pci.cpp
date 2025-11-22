#include "pci.h"

#include <interrupts.h>
#include <scheduling.h>

#include "../../include/log.h"
#include "../nvme/nvme.h"
#include "../usb/xhci/xhci.h"
#include "msix.h"
#include "../../kernel/cpu/cpu_manager.h"
#include "../../kernel/devices/device_manager.h"
#include "../../kernel/units/unit_manager.h"
#include "../usb/usb_manager.h"

static atomic_u8 next_usb_bus_number;

void usb_enable(void *arg) {
    auto pci_device_header = static_cast<PCI::PCIDeviceHeader *>(arg);
    uint16_t command = PCI::pci_read16(pci_device_header, 0x04);
    command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
    PCI::pci_write16(pci_device_header, 0x04, command);

    uint8_t vector = kernel::interrupts::get_free_vector();
    if (try_enable_msi_or_msix(reinterpret_cast<PCI::PCIHeader0 *>(pci_device_header),
                               vector)) {
        const char *dev_name = DevFS::alloc_unique_name("xhci");

        auto usb_driver = new USB::xhciDriver(vector, dev_name, next_usb_bus_number++);
        if (!usb_driver->init_device(pci_device_header)) {
            Log::Error("Could not initalize xhci driver");
            return;
        }
        usb_driver->start_device();
    }
}

namespace PCI {
    void enumerate_function(uint64_t device_address, uint64_t function) {
        uint64_t offset = function << 12;

        uint64_t function_address = device_address + offset;
        kernel::memory::map_memory(reinterpret_cast<void *>(function_address),
                                   reinterpret_cast<void *>(function_address));

        auto *pci_device_header = reinterpret_cast<PCIDeviceHeader *>(function_address);


        if (pci_device_header->device_id == 0) return;
        if (pci_device_header->device_id == 0xFFFF) return;

        /*     Log::LogMsg("[ PCI ] %s %s %s %s", get_vendor_name(pci_device_header->vendor_id),
                         get_device_name(pci_device_header->vendor_id, pci_device_header->device_id),
                         get_subclass_name(pci_device_header->_class, pci_device_header->subclass),
                         get_prog_if_Name(pci_device_header->_class, pci_device_header->subclass,
                                          pci_device_header->prog_if));*/

        switch (pci_device_header->_class) {
            case 0x01: // mass storage controller
                switch (pci_device_header->subclass) {
                    case 0x06: // serial ATA
                        switch (pci_device_header->prog_if) {
                            case 0x01:
                                break; // AHCI 1.0 device
                                /*     auto ahci = new AHCI::AHCIDriver(pci_device_header);
                                      if (!ahci->HasActivePorts()) {
                                          delete ahci;
                                          break;
                                      }

                                      for (int i = 0; i < ahci->portCount; ++i) {
                                          auto* port = ahci->ports[i];
                                          if (!port) continue;

                                          Log::debug("added device: %u", i);
                                          kernel::DeviceManager::AddDevice(static_cast<BlockDevice*>(port));
                                      }*/
                        }
                    case 0x08:
                        switch (pci_device_header->prog_if) {
                            case 0x02:
                                break;
                                /*    uint16_t command_register = pci_device_header->command;

                                    uint16_t command = pci_read16(pci_device_header, 0x04);
                                    command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
                                    pci_write16(pci_device_header, 0x04, command);

                                    auto driver = new NVMe::NvmeDriver(pci_device_header);
                                    Log::debug("namespaces: %u", driver->get_namespaces().size());
                                    if (driver->get_namespaces().size() < 0) {
                                        Log::debug("[Nvme] Namespaces not found");
                                        delete driver;
                                        break;
                                    }

                                    for (size_t i = 0; i < driver->get_namespaces().size(); ++i) {
                                        Log::debug("added device: %u", i);
                                        kernel::DeviceManager::AddDevice(
                                            static_cast<BlockDevice *>(driver->get_namespaces()[i]));
                                    }*/
                        }
                }
            case 0x0C:
                switch (pci_device_header->subclass) {
                    case 0x03:
                        switch (pci_device_header->prog_if) {
                            case 0x00:

                            case 0x10:

                            case 0x20:

                            case 0x30: {
                                USBManager::increment_expected_count();
                                char unit_name[32];
                                snprintf(unit_name, sizeof(unit_name), "xhci%u", next_usb_bus_number);

                                UnitConfig config = {
                                    .name = unit_name,
                                    .cpu_id = 2,
                                    .priority = 5,
                                    .stack_size = 0x4000,
                                    .initial_handles = nullptr,
                                    .initial_handle_count = 0,
                                    .is_idle = false,
                                    .is_user = false,
                                    .user_stack_size = 0
                                };

                                Unit *usb_unit = UnitManager::create(
                                    KERNEL_REALM_DRIVER, (void *) usb_enable, pci_device_header, &config);
                                if (!usb_unit) {
                                    Log::Error("Failed to create XHCI unit");
                                    USBManager::notify_controller_ready();
                                }
                                break;
                            }
                            case 0x80:

                            case 0xFE:
                                break;
                        }
                }
                //   case 0x03:
        }
    }

    void enumerate_device(uint64_t bus_address, uint64_t device) {
        uint64_t offset = device << 15;

        uint64_t device_address = bus_address + offset;
        kernel::memory::map_memory((void *) device_address, (void *) device_address);

        PCI::PCIDeviceHeader *pci_device_header = (PCIDeviceHeader *) device_address;

        if (pci_device_header->device_id == 0) return;
        if (pci_device_header->device_id == 0xFFFF) return;

        for (uint64_t function = 0; function < 8; function++) {
            enumerate_function(device_address, function);
        }
    }

    void enumerate_bus(uint64_t base_address, uint64_t bus) {
        uint64_t offset = bus << 20;

        uint64_t bus_address = base_address + offset;
        kernel::memory::map_memory((void *) bus_address, (void *) bus_address);

        PCI::PCIDeviceHeader *pci_device_header = (PCIDeviceHeader *) bus_address;

        if (pci_device_header->device_id == 0) return;
        if (pci_device_header->device_id == 0xFFFF) return;

        for (uint64_t device = 0; device < 32; device++) {
            enumerate_device(bus_address, device);
        }
    }

    void enumerate_pci(ACPI::MCFGHeader *mcfg) {
        next_usb_bus_number.init(1);
        int entries = ((mcfg->header.length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);

        USBManager::init();

        for (int t = 0; t < entries; t++) {
            ACPI::DeviceConfig *new_device_config = (ACPI::DeviceConfig *) (
                (uint64_t) mcfg + sizeof(ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * t));
            for (uint64_t bus = new_device_config->start_bus; bus < new_device_config->end_bus; bus++) {
                enumerate_bus(new_device_config->base_address, bus);
            }
        }

        uint8_t count = USBManager::get_expected_count();
        if (count > 0) {
            Log::Info("Found %u XHCI controller(s)", count);
        }
    }

    /**
 * Prüft ob eine BAR 64-bit ist
 * @param bar_value Der Wert der BAR (z.B. header->BAR0)
 * @return true wenn 64-bit BAR, false wenn 32-bit BAR oder I/O BAR
 */
    bool is_bar_64bit(uint32_t bar_value) {
        // Erstmal prüfen ob es eine Memory BAR ist (Bit 0 = 0)
        if (bar_value & PCI_BAR_MEMORY_MASK) {
            // I/O BAR - niemals 64-bit
            return false;
        }

        // Bits 2:1 prüfen für Memory BAR Type
        uint32_t bar_type = (bar_value >> 1) & 0x3;
        return (bar_type == 0x2); // 0x2 = 64-bit Memory BAR
    }


    BarInfo get_bar_info(PCIHeader0 *header, uint8_t bar_index) {
        BarInfo info = {0};

        if (bar_index > 5) {
            return info; // is_valid = false
        }

        uint32_t bar_values[6] = {
            header->BAR0, header->BAR1, header->BAR2,
            header->BAR3, header->BAR4, header->BAR5
        };

        uint32_t bar_value = bar_values[bar_index];

        if (bar_value == 0) {
            return info; // BAR not implemented
        }

        info.is_valid = true;
        info.is_memory = !(bar_value & PCI_BAR_MEMORY_MASK);

        if (!info.is_memory) {
            // I/O BAR
            info.address = bar_value & ~0x3ULL;
            info.is_64bit = false;
            info.is_prefetchable = false;
        } else {
            // Memory BAR
            info.is_64bit = is_bar_64bit(bar_value);
            info.is_prefetchable = (bar_value >> 3) & 1;

            if (info.is_64bit) {
                if (bar_index >= 5) {
                    info.is_valid = false;
                    return info;
                }

                uint32_t bar_high = bar_values[bar_index + 1];
                info.address = ((uint64_t) bar_high << 32) | (bar_value & ~0xFULL);
            } else {
                info.address = bar_value & ~0xFULL;
            }
        }

        return info;
    }
}
