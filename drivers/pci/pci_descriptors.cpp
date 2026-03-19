#include <vespera/types.h>

#include <klib/string.h>
#include "pci_devices.h"

namespace pci
{
    const char* device_classes[]{
        "Unclassified",
        "Mass Storage Controller",
        "Network Controller",
        "Display Controller",
        "Multimedia Controller",
        "Memory Controller",
        "Bridge Device",
        "Simple Communication Controller",
        "Base System Peripheral",
        "Input Device Controller",
        "Docking Station",
        "Processor",
        "Serial Bus Controller",
        "Wireless Controller",
        "Intelligent Controller",
        "Satellite Communication Controller",
        "Encryption Controller",
        "Signal Processing Controller",
        "Processing Accelerator",
        "Non Essential Instrumentation"
    };

    const char* get_vendor_name(const u16 vendor_id)
    {
        switch (vendor_id)
        {
        case 0x8086:
                return "Intel Corporation";
        case 0x1002:
        case 0x1022:
                return "Advanced Micro Devices, Inc. [AMD/ATI]";
        case 0x10DE:
                return "Nvidia Corporation";
        case 0x144D:
                return "Samsung Electronics Co Ltd";
        case 0x10EC:
                return "Realtek Semiconductor Co., Ltd.";
        case 0x1B21:
                return "ASMedia Technology Inc.";
        case 0x1b36:
                return"Red Hat, Inc.";
        default:
            break;
        }

        static char buffer[HEX_BUFFER_U16];
        u16_tohex(vendor_id, buffer, sizeof(buffer));
        return buffer;
    }

    const char* get_device_name(const u16 vendor_id, const u16 device_id)
    {
        for (auto [vid, did, name] : PCI_DEVICES)
        {
            if (vid == vendor_id && did == device_id)
            {
                return name;
            }
        }

        static char buffer[HEX_BUFFER_U16];
        u16_tohex(device_id, buffer, sizeof(buffer));
        return buffer;
    }

    const char* mass_storage_controller_subclass_name(const u8 subclass_code)
    {
        switch (subclass_code)
        {
        case 0x00:
            return "SCSI Bus Controller";
        case 0x01:
            return "IDE Controller";
        case 0x02:
            return "Floppy Disk Controller";
        case 0x03:
            return "IPI Bus Controller";
        case 0x04:
            return "RAID Controller";
        case 0x05:
            return "ATA Controller";
        case 0x06:
            return "Serial ATA";
        case 0x07:
            return "Serial Attached SCSI";
        case 0x08:
            return "Non-Volatile Memory Controller";
        case 0x80:
            return "Other";
        default:
            break;
        }

        static char buffer[HEX_BUFFER_U8];
        u8_tohex(subclass_code, buffer, sizeof(buffer));
        return buffer;
    }

    const char* serial_bus_controller_subclass_name(const u8 subclass_code)
    {
        switch (subclass_code)
        {
        case 0x00:
            return "FireWire (IEEE 1394) Controller";
        case 0x01:
            return "ACCESS Bus";
        case 0x02:
            return "SSA";
        case 0x03:
            return "USB Controller";
        case 0x04:
            return "Fibre Channel";
        case 0x05:
            return "SMBus Controller";
        case 0x06:
            return "Infiniband Controller";
        case 0x07:
            return "IPMI Interface";
        case 0x08:
            return "SERCOS Interface (IEC 61491)";
        case 0x09:
            return "CANbus Controller";
        case 0x80:
            return "SerialBusController - Other";
        default:
            break;
        }

        static char buffer[HEX_BUFFER_U8];
        u8_tohex(subclass_code, buffer, sizeof(buffer));
        return buffer;
    }

    const char* bridge_device_subclass_name(const u8 subclass_code)
    {
        switch (subclass_code)
        {
        case 0x00:
            return "Host Bridge";
        case 0x01:
            return "ISA Bridge";
        case 0x02:
            return "EISA Bridge";
        case 0x03:
            return "MCA Bridge";
        case 0x04:
            return "PCI-to-PCI Bridge";
        case 0x05:
            return "PCMCIA Bridge";
        case 0x06:
            return "NuBus Bridge";
        case 0x07:
            return "CardBus Bridge";
        case 0x08:
            return "RACEway Bridge";
        case 0x09:
            return "PCI-to-PCI Bridge";
        case 0x0a:
            return "InfiniBand-to-PCI Host Bridge";
        case 0x80:
            return "Other";
        default:
            break;
        }

        static char buffer[HEX_BUFFER_U8];
        u8_tohex(subclass_code, buffer, sizeof(buffer));
        return buffer;
    }

    const char* get_subclass_name(const u8 class_code, const u8 subclass_code)
    {
        switch (class_code)
        {
        case 0x01:
            return mass_storage_controller_subclass_name(subclass_code);
        case 0x03:
            if (subclass_code == 0x00)
            {
                return "VGA Compatible Controller";
            }
            break;
        case 0x06:
            return bridge_device_subclass_name(subclass_code);
        case 0x0C:
            return serial_bus_controller_subclass_name(subclass_code);
        default:
            break;
        }

        static char buffer[HEX_BUFFER_U8];
        u8_tohex(subclass_code, buffer, sizeof(buffer));
        return buffer;
    }

    const char* get_prog_if_name(const u8 class_code, const u8 subclass_code, const u8 prog_if)
    {
        switch (class_code)
        {
        case 0x01:
            switch (subclass_code)
            {
            case 0x06:
                switch (prog_if)
                {
                case 0x00:
                    return "Vendor Specific Interface";
                case 0x01:
                    return "AHCI 1.0";
                case 0x02:
                    return "Serial Storage Bus";
                default:
                    break;
                }
                break;
            case 0x08:
                switch (prog_if)
                {
                case 0x01:
                    return "NVMHCI";
                case 0x02:
                    return "NVM Express";
                default:
                    break;
                }
                break;
            default:
                break;
            }
            break;
        case 0x03:
            if (subclass_code == 0x00)
            {
                switch (prog_if)
                {
                case 0x00:
                    return "VGA Controller";
                case 0x01:
                    return "8514-Compatible Controller";
                default:
                    break;
                }
            }
            break;
        case 0x0C:
            if (subclass_code == 0x03)
            {
                switch (prog_if)
                {
                case 0x00:
                    return "UHCI Controller";
                case 0x10:
                    return "OHCI Controller";
                case 0x20:
                    return "EHCI (USB2) Controller";
                case 0x30:
                    return "XHCI (USB3) Controller";
                case 0x80:
                    return "Unspecified";
                case 0xFE:
                    return "USB Device (Not a Host Controller)";
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }

        static char buffer[HEX_BUFFER_U8];
        u8_tohex(prog_if, buffer, sizeof(buffer));
        return buffer;
    }
}
