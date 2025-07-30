#include "pci.h"
#include "../ahci/ahci.h"
#include "../../include/log.h"
#include "../nvme/nvme.h"
#include "../../filesystem/fat32.h"
#include "../usb/xhci/xhci.h"
#include "../../kernel/include/interrupts.h"
#include "msix.h"
#include "../../kernel/time/time.h"

namespace PCI {
    void enumerate_function(uint64_t device_address, uint64_t function) {
        uint64_t offset = function << 12;

        uint64_t function_address = device_address + offset;
        kernel::memory::map_memory(reinterpret_cast<void *>(function_address),
                                             reinterpret_cast<void *>(function_address));

        auto *pci_device_header = reinterpret_cast<PCIDeviceHeader *>(function_address);


        if (pci_device_header->device_id == 0) return;
        if (pci_device_header->device_id == 0xFFFF) return;

        Log::LogMsg("[ PCI ] %s %s %s %s", get_vendor_name(pci_device_header->vendor_id),
                    get_device_name(pci_device_header->vendor_id, pci_device_header->device_id),
                    get_subclass_name(pci_device_header->_class, pci_device_header->subclass),
                    get_prog_if_Name(pci_device_header->_class, pci_device_header->subclass,
                                     pci_device_header->prog_if));

        switch (pci_device_header->_class) {
            case 0x01: // mass storage controller
                switch (pci_device_header->subclass) {
                    case 0x06: // serial ATA
                        switch (pci_device_header->prog_if) {
                           /*   case 0x01: // AHCI 1.0 device
                               auto x = new AHCI::AHCIDriver(pci_device_header);
                                AHCI::Port *p = x->ports[0];
                                auto *dev = static_cast<BlockDevice *>(p);
                                FAT32::FileSystem fs(p);
                           //     auto s =fs.CreateDirectory("TESTDIR");
                           //     Log::LogMsg("File created? %s", s ? "true" : "false");
                                   size_t entryCount = 0;
                                  FAT32::FileEntry* entries = fs.ReadDirectory("/EFI/BOOT", entryCount);

                                   if (entries == nullptr) {
                                       // Fehler beim Lesen
                                       global_renderer->print("Verzeichnis konnte nicht gelesen werden.\n");
                                   } else {
                                       for (size_t i = 0; i < entryCount; i++) {
                                           global_renderer->print("Name: ");
                                           global_renderer->print(entries[i].GetName());
                                           global_renderer->print(" isDir: ");
                                           global_renderer->print(entries[i].isDir() ? "True" : "False");
                                           global_renderer->new_line();
                                       }
                                       free(entries);
                                   }

                                   char buffer[4096];  // Beispiel: 4 KB
                                   size_t size = 0;
                                   if (bool ok = fs.ReadFile("t.txt", buffer, sizeof(buffer), size)) {
                                       global_renderer->print(buffer);
                                   } else {
                                       global_renderer->print("Fehler beim Lesen der Datei\n");
                                   }
                                   global_renderer->new_line();
                                   if (fs.is_valid()) {
                                       global_renderer->print("FAT32 erkannt.");
                                       global_renderer->new_line();
                                   } else {
                                       global_renderer->print("Kein FAT32 auf Gerät.");
                                       global_renderer->new_line();
                                   }

                                   fs.CreateDirectory("TESTDIR");
                                   auto s =fs.CreateFile("testfile.txt");
                                   global_renderer->print(s ? "true" : "false");*/
                        }
                    case 0x08:
                        switch (pci_device_header->prog_if) {
                            /* case 0x02:
                               uint16_t command_register = pci_device_header->command;

                                uint16_t command = pci_read16(pci_device_header, 0x04);
                                command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
                                pci_write16(pci_device_header, 0x04, command);

                                auto driver = new NVMe::NvmeDriver(pci_device_header);
                                auto dev = static_cast<BlockDevice *>(driver->get_namespaces()[0]);
                                FAT32::FileSystem fs(dev);
                                if (fs.is_valid()) {
                                    Log::Info("FAT32 erkannt.");
                                } else {
                                    Log::Info("Kein FAT32 auf Geraet.");
                                }

                                size_t entryCount = 0;
                                FAT32::FileEntry *entries = fs.ReadDirectory("/", entryCount);

                                if (entries == nullptr) {
                                    // Fehler beim Lesen
                                    Log::Warning("Verzeichnis konnte nicht gelesen werden.");
                                } else {
                                    for (size_t i = 0; i < entryCount; i++) {
                                        Log::LogMsg("Name: %s isDir: %s", entries[i].GetName(), entries[i].isDir() ? "true" : "false");
                                    }
                                    free(entries);
                                }

                                char buffer[4096]; // Beispiel: 4 KB
                                size_t size = 0;
                                if (bool ok = fs.ReadFile("t.txt", buffer, sizeof(buffer), size)) {
                                    Log::LogMsg(buffer);
                                } else {
                                    Log::Warning("Fehler beim Lesen der Datei");
                                }*/
                            //
                        //        fs.CreateDirectory("TESTDIR");
                        //        auto s = fs.CreateFile("testfileNVME.txt");
                        //        Log::LogMsg("TestDir created? %s", s ? "true" : "false");
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
                                uint16_t command = pci_read16(pci_device_header, 0x04);
                                command |= (1 << 2) | (1 << 1); // Bus Master + Memory Space Enable
                                pci_write16(pci_device_header, 0x04, command);
                                if (try_enable_msi_or_msix(reinterpret_cast<PCI::PCIHeader0*>(pci_device_header), IRQ_XHCI_VECTOR)) Log::debug("enabled msi(x)");
                                    auto usb_driver = new USB::xhciDriver();
                                if (!usb_driver->init_device(pci_device_header)) {
                                    Log::Error("Could not initalize xhci driver");
                                    return;
                                }
                                usb_driver->start_device();
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
        int entries = ((mcfg->header.length) - sizeof(ACPI::MCFGHeader)) / sizeof(ACPI::DeviceConfig);

        for (int t = 0; t < entries; t++) {
            ACPI::DeviceConfig *new_device_config = (ACPI::DeviceConfig *) (
                (uint64_t) mcfg + sizeof(ACPI::MCFGHeader) + (sizeof(ACPI::DeviceConfig) * t));
            for (uint64_t bus = new_device_config->start_bus; bus < new_device_config->end_bus; bus++) {
                enumerate_bus(new_device_config->base_address, bus);
            }
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
