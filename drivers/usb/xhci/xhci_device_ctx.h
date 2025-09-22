// xhci_device_ctx.h
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

#ifndef VESPERAOS_DEVICE_CTX_H
#define VESPERAOS_DEVICE_CTX_H
#include <cstdint>

struct xhci_slot_context32 {
    union {
        struct {
            uint32_t route_string: 20;
            uint32_t speed: 4;
            uint32_t rz: 1;
            uint32_t mtt: 1;
            uint32_t hub: 1;
            uint32_t context_entries: 5;
        };

        uint32_t dword0;
    };

    union {
        struct {
            uint16_t max_exit_latency;
            uint8_t root_hub_port_num;
            uint8_t port_count;
        };

        uint32_t dword1;
    };

    union {
        struct {
            uint32_t parent_hub_slot_id: 8;
            uint32_t parent_port_number: 8;
            uint32_t tt_think_time: 2;
            uint32_t rsvd0: 4;
            uint32_t interrupter_target: 10;
        };

        uint32_t dword2;
    };

    union {
        struct {
            uint32_t device_address: 8;
            uint32_t rsvd1: 19;

            /*
                Value Slot State
                    0 Disabled/Enabled
                    1 Default
                    2 Addressed
                    3 Configured
                    31-4 Reserved

                Refer to section 4.5.3 for more information on Slot State.
            */
            uint32_t slot_state: 5;
        };

        uint32_t dword3;
    };

    uint32_t rsvdz[4];
} __attribute__((packed));

struct xhci_endpoint_context32 {
    union {
        struct {
            /*
            0 Disabled The endpoint is not operational
            1 Running The endpoint is operational, either waiting for a doorbell ring or processing
                TDs.
            2 HaltedThe endpoint is halted due to a Halt condition detected on the USB. SW shall issue
                Reset Endpoint Command to recover from the Halt condition and transition to the Stopped
                state. SW may manipulate the Transfer Ring while in this state.
            3 Stopped The endpoint is not running due to a Stop Endpoint Command or recovering
                from a Halt condition. SW may manipulate the Transfer Ring while in this state.
            4 Error The endpoint is not running due to a TRB Error. SW may manipulate the Transfer
                Ring while in this state.
                5-7 Reserved
             */
            uint32_t endpoint_state: 3;
            uint32_t rsvd0: 5;
            uint32_t mult: 2;
            uint32_t max_primary_streams: 5;
            uint32_t linear_stream_array: 1;
            uint32_t interval: 8;
            uint32_t max_esit_payload_hi: 8;
        };

        uint32_t dword0;
    };

    union {
        struct {
            uint32_t rsvd1: 1;
            uint32_t error_count: 2;

            /*
                Endpoint Type (EP Type). This field identifies whether an Endpoint Context is Valid, and if so,
                what type of endpoint the context defines.

                Value  Endpoint      Type Direction
                0      Not Valid     N/A
                1      Isoch         Out
                2      Bulk          Out
                3      Interrupt     Out
                4      Control       Bidirectional
                5      Isoch         In
                6      Bulk          In
                7      Interrupt     In
            */
            uint32_t endpoint_type: 3;
            uint32_t rsvd2: 1;
            uint32_t host_initiate_disable: 1;
            uint32_t max_burst_size: 8;
            uint32_t max_packet_size: 16;
        };

        uint32_t dword1;
    };

    union {
        struct {
            uint64_t dcs: 1;
            uint64_t rsvd3: 3;
            uint64_t tr_dequeue_ptr_address_bits: 60;
        };

        struct {
            uint32_t dword2;
            uint32_t dword3;
        };

        uint64_t transfer_ring_dequeue_ptr;
    };

    union {
        struct {
            uint16_t average_trb_length;
            uint16_t max_esit_payload_lo;
        };

        uint32_t dword4;
    };

    uint32_t padding[3];
} __attribute__((packed));

struct xhci_device_context32 {
    xhci_slot_context32 slot_context;

    xhci_endpoint_context32 control_ep_context;

    xhci_endpoint_context32 ep[30];
} __attribute__((packed));

struct xhci_input_control_context32 {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[5];
    uint8_t config_value;
    uint8_t interface_number;
    uint8_t alternate_setting;

    uint8_t rsvdZ;
} __attribute__((packed));

struct xhci_input_context32 {
    xhci_input_control_context32 control_context;
    xhci_device_context32 device_context;
};


struct xhci_slot_context64 {
    union {
        struct {
            uint32_t route_string: 20;
            uint32_t speed: 4;
            uint32_t rz: 1;
            uint32_t mtt: 1;
            uint32_t hub: 1;
            uint32_t context_entries: 5;
        };

        uint32_t dword0;
    };

    union {
        struct {
            uint16_t max_exit_latency;
            uint8_t root_hub_port_num;
            uint8_t port_count;
        };

        uint32_t dword1;
    };

    union {
        struct {
            uint32_t parent_hub_slot_id: 8;
            uint32_t parent_port_number: 8;
            uint32_t tt_think_time: 2;
            uint32_t rsvd0: 4;
            uint32_t interrupter_target: 10;
        };

        uint32_t dword2;
    };

    union {
        struct {
            uint32_t device_address: 8;
            uint32_t rsvd1: 19;

            /*
                Value Slot State
                    0 Disabled/Enabled
                    1 Default
                    2 Addressed
                    3 Configured
                    31-4 Reserved

                Refer to section 4.5.3 for more information on Slot State.
            */
            uint32_t slot_state: 5;
        };

        uint32_t dword3;
    };

    uint32_t rsvdz[4];

    uint32_t padding[8];
} __attribute__((packed));

struct xhci_endpoint_context64 {
    union {
        struct {
            uint32_t endpoint_state: 3;
            uint32_t rsvd0: 5;
            uint32_t mult: 2;
            uint32_t max_primary_streams: 5;
            uint32_t linear_stream_array: 1;
            uint32_t interval: 8;
            uint32_t max_esit_payload_hi: 8;
        };

        uint32_t dword0;
    };

    union {
        struct {
            uint32_t rsvd1: 1;
            uint32_t error_count: 2;

            /*
                Endpoint Type (EP Type). This field identifies whether an Endpoint Context is Valid, and if so,
                what type of endpoint the context defines.

                Value  Endpoint      Type Direction
                0      Not Valid     N/A
                1      Isoch         Out
                2      Bulk          Out
                3      Interrupt     Out
                4      Control       Bidirectional
                5      Isoch         In
                6      Bulk          In
                7      Interrupt     In
            */
            uint32_t endpoint_type: 3;
            uint32_t rsvd2: 1;
            uint32_t host_initiate_disable: 1;
            uint32_t max_burst_size: 8;
            uint32_t max_packet_size: 16;
        };

        uint32_t dword1;
    };

    union {
        struct {
            uint64_t dcs: 1;
            uint64_t rsvd3: 3;
            uint64_t tr_dequeue_ptr_address_bits: 60;
        };

        struct {
            uint32_t dword2;
            uint32_t dword3;
        };

        uint64_t transfer_ring_dequeue_ptr;
    };

    union {
        struct {
            uint16_t average_trb_length;
            uint16_t max_esit_payload_lo;
        };

        uint32_t dword4;
    };

    uint32_t padding[11];
} __attribute__((packed));

struct xhci_device_context64 {
    xhci_slot_context64 slot_context;

    xhci_endpoint_context64 control_ep_context;

    xhci_endpoint_context64 ep[30];
} __attribute__((packed));

struct xhci_input_control_context64 {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[5];
    uint8_t config_value;
    uint8_t interface_number;
    uint8_t alternate_setting;

    uint8_t rsvdZ;
    uint32_t padding[8];
} __attribute__((packed));

struct xhci_input_context64 {
    xhci_input_control_context64 control_context;
    xhci_device_context64 device_context;
};

#endif //VESPERAOS_DEVICE_CTX_H
