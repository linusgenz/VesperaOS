// usb_descriptors.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 25.08.25.
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

#ifndef VESPERAOS_USB_DESCRIPTORS_H
#define VESPERAOS_USB_DESCRIPTORS_H

#include <stdint.h>
#include <stddef.h>

#define USB_DESCRIPTOR_DEVICE                          0x01
#define USB_DESCRIPTOR_CONFIGURATION                   0x02
#define USB_DESCRIPTOR_STRING                          0x03
#define USB_DESCRIPTOR_INTERFACE                       0x04
#define USB_DESCRIPTOR_ENDPOINT                        0x05
#define USB_DESCRIPTOR_DEVICE_QUALIFIER                0x06
#define USB_DESCRIPTOR_OTHER_SPEED_CONFIGURATION       0x07
#define USB_DESCRIPTOR_INTERFACE_POWER                 0x08
#define USB_DESCRIPTOR_OTG                             0x09
#define USB_DESCRIPTOR_DEBUG                           0x0A
#define USB_DESCRIPTOR_INTERFACE_ASSOCIATION           0x0B
#define USB_DESCRIPTOR_BOS                             0x0F
#define USB_DESCRIPTOR_DEVICE_CAPABILITY               0x10
#define USB_DESCRIPTOR_WIRELESS_ENDPOINT_COMPANION     0x11
#define USB_DESCRIPTOR_SUPERSPEED_ENDPOINT_COMPANION   0x30
#define USB_DESCRIPTOR_SUPERSPEEDPLUS_ISO_ENDPOINT_COMPANION 0x31

// HID Class-Specific Descriptor Types
#define USB_DESCRIPTOR_HID                             0x21
#define USB_DESCRIPTOR_HID_REPORT                      0x22
#define USB_DESCRIPTOR_HID_PHYSICAL_REPORT             0x23

// Hub Descriptor Types
#define USB_DESCRIPTOR_HUB                             0x29
#define USB_DESCRIPTOR_SUPERSPEED_HUB                  0x2A

// Billboarding Descriptor Type
#define USB_DESCRIPTOR_BILLBOARD                       0x0D

// Type-C Bridge Descriptor Type
#define USB_DESCRIPTOR_TYPE_C_BRIDGE                   0x0E

#define USB_DESCRIPTOR_REQUEST(type, index) (((type) << 8) | (index))

#define USB_GET_DESCRIPTOR_REQ 0x06
#define USB_SET_CONFIGURATION_REQ 0x9

struct USB_DESCRIPTOR_HEADER {
    uint8_t b_length;
    uint8_t b_descriptor_type;
} __attribute__((packed));
static_assert(sizeof(USB_DESCRIPTOR_HEADER) == 2);

struct USB_CONFIGURATION_DESCRIPTOR_HEADER {
    USB_DESCRIPTOR_HEADER header;
    uint16_t w_total_length;
    uint8_t b_nuinterface_s;
    uint8_t b_configuration_value;
    uint8_t i_configuration;
    uint8_t bm_attributes;
    uint8_t b_max_power;
} __attribute__((packed));
static_assert(sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER) == 9);

struct UsbConfigurationDescriptor {
    USB_CONFIGURATION_DESCRIPTOR_HEADER header{};
    uint8_t* data{nullptr};
    size_t data_size{0};

    UsbConfigurationDescriptor() {}
    ~UsbConfigurationDescriptor() {
            delete[] data;
    }
};

struct USB_INTERFACE_DESCRIPTOR {
    USB_DESCRIPTOR_HEADER header;
    uint8_t b_interface_number;
    uint8_t b_alternate_setting;
    uint8_t b_num_endpoints;
    uint8_t b_interface_class;
    uint8_t b_interface_sub_class;
    uint8_t b_interface_protocol;
    uint8_t i_interface;
} __attribute__((packed));
static_assert(sizeof(USB_INTERFACE_DESCRIPTOR) == 9);

struct USB_DEVICE_DESCRIPTOR {
    USB_DESCRIPTOR_HEADER header;
    uint16_t bcd_usb;
    uint8_t b_device_class;
    uint8_t b_device_sub_class;
    uint8_t b_device_protocol;
    uint8_t b_max_packet_size0;
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t i_manufacturer;
    uint8_t i_product;
    uint8_t i_serial_number;
    uint8_t b_num_configurations;
} __attribute__((packed));
static_assert(sizeof(USB_DEVICE_DESCRIPTOR) == 18);


struct USB_STRING_LANGUAGE_DESCRIPTOR {
    USB_DESCRIPTOR_HEADER header;
    uint16_t lang_ids[126];
} __attribute__((packed));
static_assert(sizeof(USB_STRING_LANGUAGE_DESCRIPTOR) == 254);

struct USB_STRING_DESCRIPTOR {
    USB_DESCRIPTOR_HEADER header;
    uint16_t unicode_string[126];
} __attribute__((packed));
static_assert(sizeof(USB_STRING_DESCRIPTOR) == 254);

struct USB_ENDPOINT_DESCRIPTOR {
    USB_DESCRIPTOR_HEADER header;
    uint8_t b_endpoint_address;
    uint8_t bm_attributes;
    uint16_t w_max_packet_size;
    uint8_t b_interval;
} __attribute__((packed));
static_assert(sizeof(USB_ENDPOINT_DESCRIPTOR) == 7);

struct USB_HID_DESCRIPTOR {
    USB_DESCRIPTOR_HEADER header;
    uint16_t bcd_hid;
    uint8_t  b_country_code;
    uint8_t  b_num_descriptors;
    struct {
        uint8_t  b_descriptor_type;
        uint16_t w_descriptor_length;
    } __attribute__((packed)) desc[1];
} __attribute__((packed));
static_assert(sizeof(USB_HID_DESCRIPTOR) == 9);

#endif //VESPERAOS_USB_DESCRIPTORS_H
