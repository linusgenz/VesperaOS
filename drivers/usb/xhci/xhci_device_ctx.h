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
#include <vespera/types.h>

struct XHCI_SLOT_CONTEXT32 {
    union {
        struct {
            u32 route_string: 20;
            u32 speed: 4;
            u32 rz: 1;
            u32 mtt: 1;
            u32 hub: 1;
            u32 context_entries: 5;
        };

        u32 dword0;
    };

    union {
        struct {
            u16 max_exit_latency;
            u8 root_hub_port_num;
            u8 port_count;
        };

        u32 dword1;
    };

    union {
        struct {
            u32 parent_hub_slot_id: 8;
            u32 parent_port_number: 8;
            u32 tt_think_time: 2;
            u32 rsvd0: 4;
            u32 interrupter_target: 10;
        };

        u32 dword2;
    };

    union {
        struct {
            u32 device_address: 8;
            u32 rsvd1: 19;

            /*
                Value Slot State
                    0 Disabled/Enabled
                    1 Default
                    2 Addressed
                    3 Configured
                    31-4 Reserved

                Refer to section 4.5.3 for more information on Slot State.
            */
            u32 slot_state: 5;
        };

        u32 dword3;
    };

    u32 rsvdz[4];
} __attribute__((packed));

struct XHCI_ENDPOINT_CONTEXT32 {
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
            u32 endpoint_state: 3;
            u32 rsvd0: 5;
            u32 mult: 2;
            u32 max_primary_streams: 5;
            u32 linear_stream_array: 1;
            u32 interval: 8;
            u32 max_esit_payload_hi: 8;
        };

        u32 dword0;
    };

    union {
        struct {
            u32 rsvd1: 1;
            u32 error_count: 2;

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
            u32 endpoint_type: 3;
            u32 rsvd2: 1;
            u32 host_initiate_disable: 1;
            u32 max_burst_size: 8;
            u32 max_packet_size: 16;
        };

        u32 dword1;
    };

    union {
        struct {
            u64 dcs: 1;
            u64 rsvd3: 3;
            u64 tr_dequeue_ptr_address_bits: 60;
        };

        struct {
            u32 dword2;
            u32 dword3;
        };

        u64 transfer_ring_dequeue_ptr;
    };

    union {
        struct {
            u16 average_trb_length;
            u16 max_esit_payload_lo;
        };

        u32 dword4;
    };

    u32 padding[3];
} __attribute__((packed));

struct XHCI_DEVICE_CONTEXT32 {
    XHCI_SLOT_CONTEXT32 slot_context;

    XHCI_ENDPOINT_CONTEXT32 control_ep_context;

    XHCI_ENDPOINT_CONTEXT32 ep[30];
} __attribute__((packed));

struct XHCI_INPUT_CONTROL_CONTEXT32 {
    u32 drop_flags;
    u32 add_flags;
    u32 rsvd[5];
    u8 config_value;
    u8 interface_number;
    u8 alternate_setting;

    u8 rsvd_z;
} __attribute__((packed));

struct XhciInputContext32 {
    XHCI_INPUT_CONTROL_CONTEXT32 control_context;
    XHCI_DEVICE_CONTEXT32 device_context;
};


struct XHCI_SLOT_CONTEXT64 {
    union {
        struct {
            u32 route_string: 20;
            u32 speed: 4;
            u32 rz: 1;
            u32 mtt: 1;
            u32 hub: 1;
            u32 context_entries: 5;
        };

        u32 dword0;
    };

    union {
        struct {
            u16 max_exit_latency;
            u8 root_hub_port_num;
            u8 port_count;
        };

        u32 dword1;
    };

    union {
        struct {
            u32 parent_hub_slot_id: 8;
            u32 parent_port_number: 8;
            u32 tt_think_time: 2;
            u32 rsvd0: 4;
            u32 interrupter_target: 10;
        };

        u32 dword2;
    };

    union {
        struct {
            u32 device_address: 8;
            u32 rsvd1: 19;

            /*
                Value Slot State
                    0 Disabled/Enabled
                    1 Default
                    2 Addressed
                    3 Configured
                    31-4 Reserved

                Refer to section 4.5.3 for more information on Slot State.
            */
            u32 slot_state: 5;
        };

        u32 dword3;
    };

    u32 rsvdz[4];

    u32 padding[8];
} __attribute__((packed));

struct XHCI_ENDPOINT_CONTEXT64 {
    union {
        struct {
            u32 endpoint_state: 3;
            u32 rsvd0: 5;
            u32 mult: 2;
            u32 max_primary_streams: 5;
            u32 linear_stream_array: 1;
            u32 interval: 8;
            u32 max_esit_payload_hi: 8;
        };

        u32 dword0;
    };

    union {
        struct {
            u32 rsvd1: 1;
            u32 error_count: 2;

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
            u32 endpoint_type: 3;
            u32 rsvd2: 1;
            u32 host_initiate_disable: 1;
            u32 max_burst_size: 8;
            u32 max_packet_size: 16;
        };

        u32 dword1;
    };

    union {
        struct {
            u64 dcs: 1;
            u64 rsvd3: 3;
            u64 tr_dequeue_ptr_address_bits: 60;
        };

        struct {
            u32 dword2;
            u32 dword3;
        };

        u64 transfer_ring_dequeue_ptr;
    };

    union {
        struct {
            u16 average_trb_length;
            u16 max_esit_payload_lo;
        };

        u32 dword4;
    };

    u32 padding[11];
} __attribute__((packed));

struct XHCI_DEVICE_CONTEXT64 {
    XHCI_SLOT_CONTEXT64 slot_context;

    XHCI_ENDPOINT_CONTEXT64 control_ep_context;

    XHCI_ENDPOINT_CONTEXT64 ep[30];
} __attribute__((packed));

struct XHCI_INPUT_CONTROL_CONTEXT64 {
    u32 drop_flags;
    u32 add_flags;
    u32 rsvd[5];
    u8 config_value;
    u8 interface_number;
    u8 alternate_setting;

    u8 rsvd_z;
    u32 padding[8];
} __attribute__((packed));

struct XhciInputContext64 {
    XHCI_INPUT_CONTROL_CONTEXT64 control_context;
    XHCI_DEVICE_CONTEXT64 device_context;
};

#endif //VESPERAOS_DEVICE_CTX_H
