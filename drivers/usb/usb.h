//
// Created by linus on 14.10.24.
//

#ifndef USB_H
#define USB_H
#include <stdint.h>
#include "../pci/pci.h"
#include "../../kernel/memory/heap.h"

namespace USB {
// https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf
    #define FLADJ_OFFSET 0x61
    #define PORT_REGISTER_OFFSET 0x400
    #define DEFAULT_FRAME_LENGTH_TIMING_VALUE 0x20
    #define RING_SIZE 256
    // 5.3.3 Structural Parameters 1 (HCSPARAMS1)
    struct xHCIHcsParams1 {
        uint32_t max_slots : 8;
        uint32_t max_intrs : 11;
        uint32_t rsv0      : 5;
        uint32_t max_ports : 8;
    };

    // 5.3.4 Structural Parameters 2 (HCSPARAMS2)
    struct xHCIHcsParams2 {
        uint32_t ist            : 4;       
        uint32_t erst_max       : 4;       
        uint32_t rsv0           : 13;      
        uint32_t max_sp_bufs_hi : 5;       
        uint32_t sp_restore     : 1;      
        uint32_t max_sp_bufs_lo : 5;     
    };

    // 5.3.5 Structural Parameters 3 (HCSPARAMS3)
    struct xHCIHcsParams3 {
        uint32_t u1_exit_latency : 8;  
        uint32_t rsv0            : 8;  
        uint32_t u2_exit_latency : 16; 
    };

    // 5.3.6 Capability Parameters 1 (HCCPARAMS1)
    struct xHCIHccParams1 {
        uint32_t ac64          : 1;
        uint32_t bnc           : 1; 
       uint32_t csz            : 1;
        uint32_t ppc           : 1; 
        uint32_t pind          : 1;
        uint32_t lhrc          : 1;
        uint32_t ltc           : 1; 
        uint32_t nss           : 1;  
        uint32_t pae           : 1;  
        uint32_t spc           : 1; 
        uint32_t sec           : 1; 
        uint32_t cfc           : 1; 
        uint32_t max_psa_size  : 4; 
        uint32_t xecp          : 16; 
    };

    // 5.3 Host Controller Capability Registers
    struct xHCICapabilityRegisters {
        uint8_t caplength;
        uint8_t rsv0;
        uint16_t hci_version;
        xHCIHcsParams1 hcs_params_1;
        xHCIHcsParams2 hcs_params_2;
        xHCIHcsParams3 hcs_params_3;
        xHCIHccParams1 hcc_params_1;
        uint32_t db_off;
        uint32_t rts_off;
        uint32_t hcc_params_2;
    };

    // 5.4.1 USB Command Register (USBCMD)
    struct USBCMD {
        uint32_t RS     : 1;              
        uint32_t HCR    : 1;
        uint32_t INTE   : 1;   
        uint32_t HSEE   : 1;
        uint32_t rsv0   : 3; 
        uint32_t LHCRST : 1;
        uint32_t CSS    : 1;  
        uint32_t CRS    : 1; 
        uint32_t EWE    : 1;     
        uint32_t EU3S   : 1; 
        uint32_t rsv1   : 1;            
        uint32_t CME    : 1;             
        uint32_t ETE    : 1;    
        uint32_t TSC_EN : 1;
        uint32_t VTIOE  : 1;           
        uint32_t rsv2   : 15;
    };

    // 5.4.2 USB Status Register (USBSTS)
    struct USBSTATUS {
        uint32_t HCH  : 1;
        uint32_t rsv0 : 1;
        uint32_t HSE  : 1;
        uint32_t EINT : 1;
        uint32_t PCD  : 1;
        uint32_t rsv1 : 3;
        uint32_t SSS  : 1;
        uint32_t RSS  : 1;
        uint32_t SRE  : 1;
        uint32_t CNR  : 1;
        uint32_t HCE  : 1;
        uint32_t rsv2 : 19;
    };

    struct OR_CONFIG {
        uint32_t MaxSlotsEn : 8;
        uint32_t U3E : 1;
        uint32_t CIE : 1;
        uint32_t rsv0 : 22;
    };

    struct CRCR {
        uint32_t RCS : 1;
        uint32_t CS  : 1;
        uint32_t CA  : 1;
        uint32_t CRR : 1;
        uint32_t rsv0 : 2;
        uint64_t cmd_ring_p : 58;
    };

    // 5.4 Host Controller Operational Registers
    struct xHCIOperationalRegisters {
        USBCMD usbcmd; // command register
        USBSTATUS usbsts; // status register
        uint32_t pagesize; // pagesize register
        uint32_t rsv0[2];
        uint32_t dnctrl; // device notification control
        CRCR crcr; // command ring control register
        uint32_t rsv1[4];
        uint64_t dcbaap; // device context base address array pointer
        OR_CONFIG config; // configure register
    };

    // 5.4.8 Port Status and Control Register (PORTSC)
    struct PORTSC {
        uint32_t CCS        : 1;
        uint32_t PED        : 1;
        uint32_t rsv0       : 1;
        uint32_t OCA        : 1;
        uint32_t PR         : 1;
        uint32_t PLS        : 4;
        uint32_t PP         : 1;
        uint32_t port_speed : 4;
        uint32_t PIC        : 2;
        uint32_t LWS        : 1;
        uint32_t CSC        : 1;
        uint32_t PEC        : 1;
        uint32_t WRC        : 1;
        uint32_t OCC        : 1;
        uint32_t PRC        : 1;
        uint32_t PLC        : 1;
        uint32_t CEC        : 1;
        uint32_t CAS        : 1;
        uint32_t WCE        : 1;
        uint32_t WDE        : 1;
        uint32_t WOE        : 1;
        uint32_t rsv1       : 2;
        uint32_t DR         : 1;
        uint32_t WPR        : 1;
    };

    // 6.5 Event Ring Segment Table
    struct ERSTEntry { 
        uint32_t rsv0 : 6;       
        uint32_t ring_segment_base_address : 26; 
        uint16_t ring_segment_size : 16;   
        uint16_t rsv1 : 16; 
    };

    struct xHCIPortRegisters {
        PORTSC portsc;
        uint32_t portpmsc;
        uint32_t portli;
        uint32_t rsv0;       
    };

    struct MFINDEX {
            uint32_t microframe_index : 14;  
            uint32_t reserved : 18;
    };

    // 5.5 Host Controller Runtime Registers
    struct xHCIRuntimeRegisters {

        struct MFINDEX {
            uint32_t microframe_index : 14;  
            uint32_t reserved         : 18;
        } mfindex;
        uint32_t rsv0[7];

        struct InterrupterRegister {
            struct IMAN {
                uint32_t IP   : 1;
                uint32_t IE   : 1;
                uint32_t rsv0 : 30;
            } iman;

            struct IMOD {
                uint32_t imod_interval : 16;
                uint32_t imod_counter  : 16;
            } imod;

            struct ERSTSZ {
                uint32_t segment_count : 16;
                uint32_t rsv0          : 16;
            } erstsz;

            uint32_t rsv1[2];

            struct ERSTBA {
                uint64_t rsv0       : 6;
                uint64_t erstba_reg : 58;
            } erstba;

            struct ERDP {
                uint64_t DESI : 3;
                uint64_t EHB  : 1;
                uint64_t ERDP : 60; 
            } erdp;
        } ir[1024];
    };

    struct Doorbell {
        uint32_t db_target    : 8;
        uint32_t rsv0         : 8;
        uint32_t db_stream_id : 16;  
    };

    struct xHCIDoorbellRegisters {
        Doorbell* doorbell;
    };


    struct xHCIEventTrb {
        uint64_t parameter;
        uint32_t status;

        uint32_t control;
    };

    struct xHCIDeviceContext {
        uint32_t slot_context[8]; // slot context for device
        uint32_t endpoint_context[31][8]; // endpoint contexts, up to 31
    };

    struct xHCIEndpointContext {
        uint32_t ep_info; // endpoint information
        uint32_t ep_info2; 
        uint64_t tr_dequeue_ptr; // transfer ring dequeue pointer
        uint32_t ep_state; //endpoint state
        uint32_t rsv0;
    };

   /* struct xHCIScratchpadBuffer {
        uint64_t scratchpad_buffers[];
    };*/
    
    struct xHCISlotContext {
        uint32_t route_string;
        uint32_t speed;
        uint32_t device_info;
        uint32_t slot_state;
    };

    struct FLADJ {
        uint32_t FLTV : 6;
        uint32_t NFC  : 1;
        uint32_t rsv0 : 1;
    };

/*****************************************************************
 *****************************************************************
 *****************************************************************/

    struct TRB {
        uint32_t reservedZ3 : 21;  // Reserved bits
        uint32_t trbType : 6;      // TRB Type
        uint32_t cycleBit : 1;     // Cycle Bit (C)
    } __attribute__((packed));

    // Derived NoOpCommandTRB struct
    struct NoOpCommandTRB : public TRB {
        NoOpCommandTRB() {
            trbType = 0x17;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived EnableSlotCommandTRB struct
    struct EnableSlotCommandTRB : public TRB {
        uint32_t slotType : 5;

        EnableSlotCommandTRB() {
            trbType = 0x9;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived DisableSlotCommandTRB struct
    struct DisableSlotCommandTRB : public TRB {
        uint32_t slotID : 8;

        DisableSlotCommandTRB() {
            trbType = 0xA;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived AddressDeviceCommandTRB struct
    struct AddressDeviceCommandTRB : public TRB {
        uint64_t inputContextPtr;

        AddressDeviceCommandTRB() {
            trbType = 0xB;
            cycleBit = 0;
            inputContextPtr = 0;
        }
    } __attribute__((packed));

    // Derived ConfigureEndpointCommandTRB struct
    struct ConfigureEndpointCommandTRB : public TRB {
        uint64_t inputContextPtr;

        ConfigureEndpointCommandTRB() {
            trbType = 0xC;
            cycleBit = 0;
            inputContextPtr = 0;
        }
    } __attribute__((packed));

    // Derived EvaluateContextCommandTRB struct
    struct EvaluateContextCommandTRB : public TRB {
        uint64_t inputContextPtr;

        EvaluateContextCommandTRB() {
            trbType = 0xD;
            cycleBit = 0;
            inputContextPtr = 0;
        }
    } __attribute__((packed));

    // Derived ResetEndpointCommandTRB struct
    struct ResetEndpointCommandTRB : public TRB {
        uint32_t endpointID : 8;
        uint32_t transferStatePreserve : 1;

        ResetEndpointCommandTRB() {
            trbType = 0xE;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived StopEndpointCommandTRB struct
    struct StopEndpointCommandTRB : public TRB {
        uint32_t endpointID : 8;
        uint32_t suspend : 1;

        StopEndpointCommandTRB() {
            trbType = 0xF;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived SetTRDequeuePointerCommandTRB struct
    struct SetTRDequeuePointerCommandTRB : public TRB {
        uint32_t streamID : 16;
        uint64_t newTRDequeuePtr;

        SetTRDequeuePointerCommandTRB() {
            trbType = 0x10;
            cycleBit = 0;
            newTRDequeuePtr = 0;
        }
    } __attribute__((packed));

    // Derived ResetDeviceCommandTRB struct
    struct ResetDeviceCommandTRB : public TRB {
        ResetDeviceCommandTRB() {
            trbType = 0x11;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived ForceEventCommandTRB struct
    struct ForceEventCommandTRB : public TRB {
        uint32_t eventTRBPointerLo;
        uint32_t eventTRBPointerHi;
        uint32_t vfID : 8;

        ForceEventCommandTRB() {
            trbType = 0x12;
            cycleBit = 0;
            eventTRBPointerLo = 0;
            eventTRBPointerHi = 0;
        }
    } __attribute__((packed));

    // Derived NegotiateBandwidthCommandTRB struct
    struct NegotiateBandwidthCommandTRB : public TRB {
        uint32_t slotID : 8;

        NegotiateBandwidthCommandTRB() {
            trbType = 0x13;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived SetLatencyToleranceValueCommandTRB struct
    struct SetLatencyToleranceValueCommandTRB : public TRB {
        uint32_t beltValue : 12;

        SetLatencyToleranceValueCommandTRB() {
            trbType = 0x14;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived GetPortBandwidthCommandTRB struct
    struct GetPortBandwidthCommandTRB : public TRB {
        uint32_t portBandwidthContextPtrLo;
        uint32_t portBandwidthContextPtrHi;
        uint32_t devSpeed : 4;
        uint32_t hubSlotID : 8;

        GetPortBandwidthCommandTRB() {
            trbType = 0x15;
            cycleBit = 0;
            portBandwidthContextPtrLo = 0;
            portBandwidthContextPtrHi = 0;
        }
    } __attribute__((packed));

    // Derived GetExtendedPropertyCommandTRB struct
    struct GetExtendedPropertyCommandTRB : public TRB {
        uint32_t extendedPropertyContextPtrLo;
        uint32_t extendedPropertyContextPtrHi;
        uint32_t commandSubType : 3;
        uint32_t endpointID : 8;
        uint32_t slotID : 8;

        GetExtendedPropertyCommandTRB() {
            trbType = 0x18;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived ForceHeaderCommandTRB struct
    struct ForceHeaderCommandTRB : public TRB {
        uint32_t headerInfoLo;
        uint32_t headerInfoMid;
        uint32_t headerInfoHi;
        uint32_t rootHubPortNumber : 8;

        ForceHeaderCommandTRB() {
            trbType = 0x16;
            cycleBit = 0;
        }
    } __attribute__((packed));

    // Derived SetExtendedPropertyCommandTRB struct
    struct SetExtendedPropertyCommandTRB : public TRB {
        uint32_t capabilityData : 16;

        SetExtendedPropertyCommandTRB() {
            trbType = 0x19;
            cycleBit = 0;
        }
    } __attribute__((packed));


/*****************************************************************
 *****************************************************************
 *****************************************************************/

    struct xHCICommandRing {
        TRB *trbs;
        uint32_t cycle_state;
        uint32_t enqueue_index;
        uint32_t dequeue_index;
    };

    class xHCIDriver {
        public:
        xHCIDriver(PCI::PCIDeviceHeader* pciBaseAddress);
        void stop();
        void reset();
        void wait_controller_ready();
        PCI::PCIDeviceHeader* PCIBaseAddress;
        xHCICapabilityRegisters* bar;
        xHCIOperationalRegisters* op_regs;
        xHCIRuntimeRegisters* runtime_regs;
        Doorbell* doorbell_regs;
        xHCIPortRegisters* ports;
        FLADJ* fladj_reg;
        void stop_command_ring();
        uint8_t initialize_port(uint32_t port_number);
        void initialize_command_ring();
        void initialize_event_ring();
        void enumerate_devices(uint32_t max_ports);
        uint8_t enumerate_device(uint32_t port_number);
        void initialize_FLADJ();
        void initialize_dcbaa();
        void initialize_doorbell_registers(uint32_t max_slots);
        void add_trb_to_command_ring(xHCICommandRing &command_ring, TRB* trb);
        private:
        xHCICommandRing* command_ring;
        TRB* event_ring_segment;
        uint64_t *event_ring;
        uint64_t *erst;
        uint64_t dcbaa[256];
    };

    
};

#endif //USB_H