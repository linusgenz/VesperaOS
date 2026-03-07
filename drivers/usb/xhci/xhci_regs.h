#ifndef XHCI_REGS_H
#define XHCI_REGS_H
#include "xhci_mem.h"

struct XHCI_CAPABILITY_REGISTERS {
    const u8 caplength;    // Capability Register Length
    const u8 reserved0;
    const u16 hciversion;  // Interface Version Number
    const u32 hcsparams1;  // Structural Parameters 1
    const u32 hcsparams2;  // Structural Parameters 2
    const u32 hcsparams3;  // Structural Parameters 3
    const u32 hccparams1;  // Capability Parameters 1
    const u32 dboff;       // Doorbell Offset
    const u32 rtsoff;      // Runtime Register Space Offset
    const u32 hccparams2;  // Capability Parameters 2
};
static_assert(sizeof(XHCI_CAPABILITY_REGISTERS) == 32);

struct XHCI_OPERATIONAL_REGISTERS {
    u32 usbcmd;        // USB Command
    u32 usbsts;        // USB Status
    u32 pagesize;      // Page Size
    u32 reserved0[2];
    u32 dnctrl;        // Device Notification Control
    volatile u64 crcr; // Command Ring Control
    u32 reserved1[4];
    u64 dcbaap;        // Device Context Base Address Array Pointer
    u32 config;        // Configure
    u32 reserved2[49];
    // Port Register Set offset has to be calculated dynamically based on MAXPORTS
};
static_assert(sizeof(XHCI_OPERATIONAL_REGISTERS) == 256);

/*
// xHci Spec Section 5.5.2 (page 389)

Note: All registers of the Primary Interrupter shall be initialized before
setting the Run/Stop (RS) flag in the USBCMD register to ‘1’. Secondary
Interrupters may be initialized after RS = ‘1’, however all Secondary
Interrupter registers shall be initialized before an event that targets them is
generated. Not following these rules, shall result in undefined xHC behavior.
*/
struct XHCI_INTERRUPTER_REGISTERS {
    union {
        struct {
            u32 ip : 1;
            u32 ie : 1;
            u32 rsv: 30;
        };
        u32 iman;
    };
    u32 imod;         // Interrupter Moderation
    u32 erstsz;       // Event Ring Segment Table Size
    u32 rsvd;         // Reserved
    u64 erstba;       // Event Ring Segment Table Base Address
    union {
        struct {
            // This index is used to accelerate the checking of
            // an Event Ring Full condition. This field can be 0.
            u64 dequeue_erst_segment_index : 3;

            // This bit is set by the controller when it sets the
            // Interrupt Pending bit. Then once your handler is finished
            // handling the event ring, you clear it by writing a '1' to it.
            u64 event_handler_busy         : 1;

            // Physical address of the _next_ item in the event ring
            u64 event_ring_dequeue_pointer : 60;
        };
        u64 erdp;     // Event Ring Dequeue Pointer (offset 18h)
    };
};

/*
// xHci Spec Section 5.5 Table 5-35: Host Controller Runtime Registers (page 388)

This section defines the xHCI Runtime Register space. The base address of this
register space is referred to as Runtime Base. The Runtime Base shall be 32-
byte aligned and is calculated by adding the value Runtime Register Space
Offset register (refer to Section 5.3.8) to the Capability Base address. All
Runtime registers are multiples of 32 bits in length.
Unless otherwise stated, all registers should be accessed with Dword references
on reads, with an appropriate software mask if needed. A software
read/modify/write mechanism should be invoked for partial writes.
Software should write registers containing a Qword address field using only
Qword references. If a system is incapable of issuing Qword references, then 
388 Document Number: 625472, Revision: 1.2b Intel Confidential
writes to the Qword address fields shall be performed using 2 Dword
references; low Dword-first, high-Dword second.
*/
struct XHCI_RUNTIME_REGISTERS {
    u32 mf_index;                      // Microframe Index (offset 0000h)
    u32 rsvdz[7];                      // Reserved (offset 001Fh:0004h)
    XHCI_INTERRUPTER_REGISTERS ir[1024];    // Interrupter Register Sets (offset 0020h to 8000h)
};

/*
// xHci Spec Section 5.6 Figure 5-29: Doorbell Register (page 394)

The Doorbell Array is organized as an array of up to 256 Doorbell Registers.
One 32-bit Doorbell Register is defined in the array for each Device Slot.
System software utilizes the Doorbell Register to notify the xHC that it has
Device Slot related work for the xHC to perform.
The number of Doorbell Registers implemented by a particular instantiation of a
host controller is documented in the Number of Device Slots (MaxSlots) field of
the HCSPARAMS1 register (section 5.3.3).
These registers are pointed to by the Doorbell Offset Register (DBOFF) in the
xHC Capability register space. The Doorbell Array base address shall be Dword
aligned and is calculated by adding the value in the DBOFF register (section
5.3.7) to “Base” (the base address of the xHCI Capability register address
space).

All registers are 32 bits in length. Software should read and write these
registers using only Dword accesses

Note: Software shall not write the Doorbell of an endpoint until after it has issued a
Configure Endpoint Command for the endpoint and received a successful
Command Completion Event.
*/
struct XHCI_DOORBELL_REGISTER {
    union {
        struct {
            u8     db_target;
            u8     rsvd;
            u16    db_stream_id;
        };

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));

class XhciDoorbellManager {
public:
    explicit XhciDoorbellManager(uptr base);

    // TargeValue = 2 + (ZeroBasedEndpoint * 2) + (isOutEp ? 0 : 1)
    void ring_doorbell(u8 doorbell, u8 target) const;

    void ring_command_doorbell() const;
    void ring_control_endpoint_doorbell(u8 doorbell) const;

private:
    XHCI_DOORBELL_REGISTER* doorbell_registers_;
};

struct XHCI_EXTENDED_CAPABILITY_ENTRY {
    union {
        struct {
            u8 id;
            u8 next;

            u16 cap_specific;
        };
        u32 raw;
    };
};
#define XHCI_NEXT_EXT_CAP_PTR(ptr, next) (volatile u32*)((char*)(ptr) + ((next) * sizeof(u32)))

struct XHCI_PORTSC_REGISTER {
    union {
        struct {
            // Current connect status (RO), if PP is 0, this bit is also 0
            u32    ccs         : 1;

            // Port Enable/Disable (R/WC), if PP is 0, this bit is also 0
            u32    ped         : 1;

            // Reserved and zeroed
            u32    rsvd0       : 1;

            // Over-current active (RO)
            u32    oca         : 1;

            // Port reset (R/W), if PP is 0, this bit is also 0
            u32    pr          : 1;

            // Port link state (R/W), if PP is 0, this bit is also 0
            u32    pls         : 4;

            // Port power (R/W)
            u32    pp          : 1;

            // Port speed (RO)
            u32    port_speed  : 4;

            // Port indicator control (R/W), if PP is 0, this bit is also 0
            u32    pic         : 2;

            // Port link state write strobe (R/W), if PP is 0, this bit is also 0
            u32    lws         : 1;

            // Connect status change (R/WC), if PP is 0, this bit is also 0.
            // ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            // ** Clear this bit by writing a '1' to it.
            u32    csc         : 1;

            /*
            Port enable/disable change (R/WC), if PP is 0, this bit is also 0.
            ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            ** For a USB2 protocol port, this bit shall be set to ‘1’ only when the port is disabled (EOF2)
            ** For a USB3 protocol port, this bit shall never be set to ‘1’
            ** Software shall clear this bit by writing a ‘1’ to it. Refer to section 4.19.2
            */
            u32    pec         : 1;

            // Warm port reset change (R/WC), if PP is 0, this bit is also 0.
            // ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            // ** Reserved and zeroed on USB2 ports.
            // ** Software shall clear this bit by writing a '1' to it.
            u32    wrc         : 1;

            // Over-current change (R/WC), if PP is 0, this bit is also 0.
            // ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            // ** Software shall clear this bit by writing a '1' to it.
            u32    occ         : 1;

            // Port reset change (R/WC), if PP is 0, this bit is also 0.
            // ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            // ** Software shall clear this bit by writing a '1' to it.
            u32    prc         : 1;

            // Port link state change (R/WC), if PP is 0, this bit is also 0.
            // ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            u32    plc         : 1;

            // Port config error change (R/WC), if PP is 0, this bit is also 0.
            // ** When transitioning from 0 to a 1, will trigger a Port Status Change Event.
            // ** Reserved and zeroed on USB2 ports.
            // ** Software shall clear this bit by writing a '1' to it.
            u32    cec         : 1;

            // Cold attach status (R/O), if PP is 0, this bit is also 0.
            u32    cas         : 1;

            // Wake on connect enable (R/W)
            u32    wce         : 1;

            // Wake on disconnect enable (R/W)
            u32    wde         : 1;

            // Wake on over-current enable (R/W)
            u32    woe         : 1;

            // Reserved and zeroed
            u32    rsvd1        : 2;

            // Device removable (RO)
            u32    dr          : 1;

            // Warm port reset (R/WC).
            // ** Reserved and zeroed on USB2 ports.
            u32    wpr         : 1;
        } __attribute__((packed));

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));
static_assert(sizeof(XHCI_PORTSC_REGISTER) == sizeof(u32));

enum class XHCI_EXTENDED_CAPABILITY_CODE {
    REVD = 0,
    USB_LEGACY_SUPPORT = 1,
    SUPPORT_PROTOCOL = 2,
    EXTENDED_POWER_MANAGEMENT = 3,
    IOVIRTULIZATION_SUPPORT = 4,
    MESSAGE_INTERRUPT_SUPPORT = 5,
    LOCAL_MEMORY_SUPPORT = 6,
    USB_DEBUG_CAPABILITY_SUPPORT = 10,
    EXTENDED_MESSAGE_INTERRUPT_SUPPORT = 17,
};

class XhciExtendedCapability {
public:
    explicit XhciExtendedCapability(volatile u32* cap_ptr);

    [[nodiscard]] volatile u32* base() const {return base_;}

    [[nodiscard]]  XHCI_EXTENDED_CAPABILITY_CODE id() const {
        return static_cast<XHCI_EXTENDED_CAPABILITY_CODE>(entry_.id);
    }
    [[nodiscard]] XhciExtendedCapability* next() const {return next_;}
private:
    volatile u32* base_;
    XHCI_EXTENDED_CAPABILITY_ENTRY entry_{};

    XhciExtendedCapability* next_;

    void read_next_ext_caps();
};

// For USB2.0 this register is reserved and preserved
struct XHCI_PORTLI_REGISTER {
    union {
        struct {
            u32 link_error_count   : 16;
            u32 rx_lane_count      : 4;
            u32 tx_lane_count      : 4;
            u32 rsvd               : 8;
        } __attribute__((packed));

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));
static_assert(sizeof(XHCI_PORTLI_REGISTER) == sizeof(u32));

struct XHCI_PORTPMSC_REGISTER_USB2 {
    union {
        struct {
            u32 l1_status                       : 3;
            u32 remote_wake_enable             : 1;
            u32 host_initiated_resume_duration : 4;
            u32 l1device_slot                  : 8;
            u32 hardware_lpm_enable            : 1;
            u32 rsvd                           : 11;
            u32 port_test_control                : 4;
        } __attribute__((packed));

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));
static_assert(sizeof(XHCI_PORTPMSC_REGISTER_USB2) == sizeof(u32));

struct XHCI_PORTPMSC_REGISTER_USB3 {
    union {
        struct {
            u32 u1_timeout              : 8;
            u32 u2_timeout              : 8;
            u32 force_link_pm_accept   : 1;
            u32 rsvd                   : 15;
        } __attribute__((packed));

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));
static_assert(sizeof(XHCI_PORTPMSC_REGISTER_USB3) == sizeof(u32));

// Port Hardware LPM Control Register
struct XHCI_PORTHLPMC_REGISTER_USB2 {
    union {
        struct {
            u32 hirdm      : 2;
            u32 l1_timeout  : 8;
            u32 besld      : 4;
            u32 rsvd       : 18;
        } __attribute__((packed));

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));
static_assert(sizeof(XHCI_PORTHLPMC_REGISTER_USB2) == sizeof(u32));

struct XHCI_PORTHLPMC_REGISTER_USB3 {
    union {
        struct {
            u16 link_soft_error_count;
            u16 rsvd;
        } __attribute__((packed));

        // Must be accessed using 32-bit dwords
        u32 raw;
    };
} __attribute__((packed));
static_assert(sizeof(XHCI_PORTHLPMC_REGISTER_USB3) == sizeof(u32));

class XhciPortRegisterManager {
public:
    explicit XhciPortRegisterManager(uptr base) : base_(base) {}

    void read_portsc_reg(XHCI_PORTSC_REGISTER& reg) const;
    void write_portsc_reg(const XHCI_PORTSC_REGISTER& reg) const;

    void read_portpmsc_reg_usb2(XHCI_PORTPMSC_REGISTER_USB2& reg) const;
    void write_portpmsc_reg_usb2(const XHCI_PORTPMSC_REGISTER_USB2& reg) const;

    void read_portpmsc_reg_usb3(XHCI_PORTPMSC_REGISTER_USB3& reg) const;
    void write_portpmsc_reg_usb3(const XHCI_PORTPMSC_REGISTER_USB3& reg) const;

    void read_portli_reg(XHCI_PORTLI_REGISTER& reg) const;
    void write_portli_reg(const XHCI_PORTLI_REGISTER& reg) const;

    void read_porthlpmc_reg_usb2(XHCI_PORTHLPMC_REGISTER_USB2& reg) const;
    void write_porthlpmc_reg_usb2(const XHCI_PORTHLPMC_REGISTER_USB2& reg) const;

    void read_porthlpmc_reg_usb3(XHCI_PORTHLPMC_REGISTER_USB3& reg) const;
    void write_porthlpmc_reg_usb3(const XHCI_PORTHLPMC_REGISTER_USB3& reg) const;

private:
    uptr base_;

    const usize portsc_offset_     = 0x00;
    const usize portpmsc_offset_   = 0x04;
    const usize portli_offset_     = 0x08;
    const usize porthlpmc_offset_  = 0x0C;
};


#endif // XHCI_REGS_H