#include "pci.h"

#include <kernel/interrupts.h>
#include <kernel/kernel_utils.h>
#include <kernel/memory.h>
#include <log.h>

#include "../../kernel/graphics/display_manager.h"
#include "../../kernel/units/unit_manager.h"
#include "../ahci/ahci.h"
#include "../gpu/intel/intel_blt.h"
#include "../nvme/nvme.h"
#include "../usb/usb_manager.h"
#include "../usb/xhci/xhci.h"
#include "msix.h"

static atomic_u8 next_usb_bus_number;

void usb_enable(void* arg)
{
    const auto pci_device_header = static_cast<PCI::PCIDeviceHeader*>(arg);
    uint16_t command = PCI::pci_read16(pci_device_header, 0x04);
    command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
    command |= (1 << 10); // Disable INTx
    PCI::pci_write16(pci_device_header, 0x04, command);

    if (const uint8_t vector = kernel::interrupts::get_free_vector();
        try_enable_msi_or_msix(reinterpret_cast<PCI::PCIHeader0*>(pci_device_header),
                               vector))
    {
        char name[16];
        DeviceManager::AllocUniqueDeviceName("xhci", name, sizeof(name));

        auto usb_driver = new USB::xhciDriver(vector, name, next_usb_bus_number++);
        if (!usb_driver->init_device(pci_device_header))
        {
            Log::Error("Could not initalize xhci driver");
            return;
        }
        usb_driver->start_device();
    }
}

namespace PCI
{
    void enumerate_function(const uint64_t device_address, const uint64_t function)
    {
        virt_addr_t func_virt = virt_from_raw(device_address + (function << 12));
        phys_addr_t func_phys = make_phys(virt_raw(func_virt));  // identity mapped

        kernel::memory::map_memory(func_virt, func_phys, 0);

        auto* pci_device_header = virt_as<PCIDeviceHeader>(func_virt);


        if (pci_device_header->device_id == 0) return;
        if (pci_device_header->device_id == 0xFFFF) return;
/*
        Log::LogMsg("[ PCI ] %s %s %s %s", get_vendor_name(pci_device_header->vendor_id),
                    get_device_name(pci_device_header->vendor_id, pci_device_header->device_id),
                    get_subclass_name(pci_device_header->_class, pci_device_header->subclass),
                    get_prog_if_Name(pci_device_header->_class, pci_device_header->subclass,
                                     pci_device_header->prog_if));
*/
        switch (pci_device_header->_class)
        {
        case 0x01: // mass storage controller
            switch (pci_device_header->subclass)
            {
            case 0x06: // serial ATA
                switch (pci_device_header->prog_if)
                {
                case 0x01: // AHCI 1.0 device
                    {
                        if (function != 0) return; // only accept function 0 (main function) for now
                        uint16_t command = PCI::pci_read16(pci_device_header, 0x04);
                        command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
                        command |= (1 << 10); // Disable INTx
                        PCI::pci_write16(pci_device_header, 0x04, command);

                        new AHCI::AHCIDriver(pci_device_header);
                        break;
                    }
                default: ;
                }
            case 0x08:
                switch (pci_device_header->prog_if)
                {
                case 0x02:
                    {
                        uint16_t command = pci_read16(pci_device_header, 0x04);
                        command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
                        pci_write16(pci_device_header, 0x04, command);

                        new NVMe::NvmeDriver(pci_device_header);
                        break;
                    }
                default: ;
                }
            default: ;
            }
        case 0x3: // Display Controller
            switch (pci_device_header->subclass)
            {
            case 0x00:
                switch (pci_device_header->prog_if)
                {
                case 0x0:
                    switch (pci_device_header->vendor_id)
                    {
                    case 0x8086:
                        {
                            /*auto* driver = new IntelBlt(pci_device_header);
                            driver->start_device(TargetFramebuffer->width, TargetFramebuffer->height);

                            DisplayBackend be{ driver, driver->get_kd() };
                            DisplayManager::set_primary(be);

                            auto terminal = new Terminal(driver, system_font->width, system_font->height);
                            Log::SetTerminal(terminal);
                            global_terminal = terminal;
                            break;*/
                        }
                    default: ;
                    }
                    break;
                default: ;
                }
            default: ;
            }
        case 0x0C:
            switch (pci_device_header->subclass)
            {
            case 0x03:
                switch (pci_device_header->prog_if)
                {
                case 0x00:

                case 0x10:

                case 0x20:

                case 0x30:
                    {
                        USBManager::increment_expected_count();
                        char unit_name[32];
                        snprintf(unit_name, sizeof(unit_name), "xhci%u", next_usb_bus_number);

                        const UnitConfig config = {
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

                        const Unit* usb_unit = UnitManager::create(
                            KERNEL_REALM_DRIVER, usb_enable, pci_device_header, &config);
                        if (!usb_unit)
                        {
                            Log::Error("Failed to create XHCI unit");
                            USBManager::notify_controller_ready();
                        }
                        break;
                    }
                case 0x80:

                case 0xFE:
                    break;
                default: ;
                }
            default: ;
            }
        default: ;
            //   case 0x03:
        }
    }


    void enumerate_device(uint64_t bus_address, uint64_t device) {
        virt_addr_t dev_virt = virt_from_raw(bus_address + (device << 15));
        phys_addr_t dev_phys = make_phys(virt_raw(dev_virt));

        kernel::memory::map_memory(dev_virt, dev_phys, 0);

        const auto* pci_device_header = virt_as<PCIDeviceHeader>(dev_virt);

        if (pci_device_header->device_id == 0)      return;
        if (pci_device_header->device_id == 0xFFFF) return;

        for (uint64_t function = 0; function < 8; function++)
            enumerate_function(virt_raw(dev_virt), function);
    }

    void enumerate_bus(uint64_t base_address, uint64_t bus) {
        virt_addr_t bus_virt = virt_from_raw(base_address + (bus << 20));
        phys_addr_t bus_phys = make_phys(virt_raw(bus_virt));

        kernel::memory::map_memory(bus_virt, bus_phys, 0);

        const auto* pci_device_header = virt_as<PCIDeviceHeader>(bus_virt);

        if (pci_device_header->device_id == 0)      return;
        if (pci_device_header->device_id == 0xFFFF) return;

        for (uint64_t device = 0; device < 32; device++)
            enumerate_device(virt_raw(bus_virt), device);
    }

    void enumerate_pci(ACPI::MCFGHeader* mcfg)
    {
        next_usb_bus_number.init(1);
        const uint32_t entries = ((mcfg->header.length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);

        USBManager::init();

        for (size_t t = 0; t < entries; t++)
        {
            const auto* new_device_config = reinterpret_cast<ACPI::DeviceConfig*>(reinterpret_cast<uint64_t>(mcfg) + sizeof(
                ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * t));
            for (uint64_t bus = new_device_config->start_bus; bus < new_device_config->end_bus; bus++)
            {
                enumerate_bus(new_device_config->base_address, bus);
            }
        }

        if (const uint8_t count = USBManager::get_expected_count(); count > 0)
        {
            Log::Info("Found %u XHCI controller(s)", count);
        }
    }

    /**
 * Prüft ob eine BAR 64-bit ist
 * @param bar_value Der Wert der BAR (z.B. header->BAR0)
 * @return true wenn 64-bit BAR, false wenn 32-bit BAR oder I/O BAR
 */
    bool is_bar_64bit(uint32_t bar_value)
    {
        // Erstmal prüfen ob es eine Memory BAR ist (Bit 0 = 0)
        if (bar_value & PCI_BAR_MEMORY_MASK)
        {
            // I/O BAR - niemals 64-bit
            return false;
        }

        // Bits 2:1 prüfen für Memory BAR Type
        uint32_t bar_type = (bar_value >> 1) & 0x3;
        return (bar_type == 0x2); // 0x2 = 64-bit Memory BAR
    }


    BarInfo get_bar_info(PCIHeader0* header, uint8_t bar_index)
    {
        BarInfo info = {};

        if (bar_index > 5)
        {
            return info; // is_valid = false
        }

        volatile uint32_t* bar_registers = &header->BAR0;
        uint32_t bar_value = bar_registers[bar_index];

        if (bar_value == 0)
        {
            return info; // BAR not implemented
        }

        info.is_valid = true;
        info.is_memory = !(bar_value & PCI_BAR_MEMORY_MASK);

        if (!info.is_memory)
        {
            // I/O BAR
            info.address = bar_value & ~0x3ULL;
            info.is_64bit = false;
            info.is_prefetchable = false;

            // Calculate I/O BAR size
            uint32_t original = bar_registers[bar_index];
            bar_registers[bar_index] = 0xFFFFFFFF;
            uint32_t size_mask = bar_registers[bar_index] & ~0x3;
            bar_registers[bar_index] = original;
            info.size = ~size_mask + 1;
        }
        else
        {
            // Memory BAR
            info.is_64bit = is_bar_64bit(bar_value);
            info.is_prefetchable = (bar_value >> 3) & 1;

            if (info.is_64bit)
            {
                if (bar_index >= 5)
                {
                    info.is_valid = false;
                    return info;
                }

                uint32_t bar_high = bar_registers[bar_index + 1];
                info.address = (static_cast<uint64_t>(bar_high) << 32) | (bar_value & ~0xFULL);

                // Calculate 64-bit BAR size
                uint32_t original_low = bar_registers[bar_index];
                uint32_t original_high = bar_registers[bar_index + 1];

                bar_registers[bar_index] = 0xFFFFFFFF;
                bar_registers[bar_index + 1] = 0xFFFFFFFF;

                uint32_t size_low = bar_registers[bar_index] & ~0xF;
                uint32_t size_high = bar_registers[bar_index + 1];

                bar_registers[bar_index] = original_low;
                bar_registers[bar_index + 1] = original_high;

                uint64_t size_mask = (static_cast<uint64_t>(size_high) << 32) | size_low;
                info.size = ~size_mask + 1;
            }
            else
            {
                info.address = bar_value & ~0xFULL;

                // Calculate 32-bit BAR size
                uint32_t original = bar_registers[bar_index];
                bar_registers[bar_index] = 0xFFFFFFFF;
                uint32_t size_mask = bar_registers[bar_index] & ~0xF;
                bar_registers[bar_index] = original;
                info.size = ~size_mask + 1;
            }
        }

        return info;
    }
}
