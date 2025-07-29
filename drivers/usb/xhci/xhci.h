#ifndef XHCI_H
#define XHCI_H

#include "xhci_regs.h"
#include "xhci_rings.h"
#include "../../../include/vector.h"
#include "../../pci/pci.h"
#include "../../../arch/x86_64/interrupts/idt.h"
#include "xhci_ext_cap.h"

namespace USB {
    class xhciDriver {
    public:
        xhciDriver();

        ~xhciDriver() = default;

        bool init_device(PCI::PCIDeviceHeader *pci_base_address);

        bool start_device();

        bool shutdown_device();


    private:
        uintptr_t m_xhc_base;

        volatile xhci_capability_registers *m_cap_regs;
        volatile xhci_operational_registers *m_op_regs;
        volatile xhci_runtime_registers *m_runtime_regs;

        xhci_extended_capability *extended_capabilities_head;

        // CAPLENGTH
        uint8_t m_capability_regs_length;

        // HCSPARAMS1
        uint8_t m_max_device_slots;
        uint8_t m_max_interrupters;
        uint8_t m_max_ports;

        // HCSPARAMS2
        uint8_t m_isochronous_scheduling_threshold;
        uint8_t m_erst_max;
        uint8_t m_max_scratchpad_buffers;

        // hccparams1
        bool m_64bit_addressing_capability;
        bool m_bandwidth_negotiation_capability;
        bool m_64byte_context_size;
        bool m_port_power_control;
        bool m_port_indicators;
        bool m_light_reset_capability;
        uint32_t m_extended_capabilities_offset;

        // Device context base address array's virtual address
        uint64_t *m_dcbaa;

        // Since DCBAA stores physical addresses, we want to keep
        // track of the virtual pointers to the output device contexts.
        uint64_t *m_dcbaa_virtual_addresses;

        // Main command ring
        xhciCommandRing *m_command_ring;

        // Main event ring
        xhciEventRing *m_event_ring;

        // Doorbell register array manager
        xhci_doorbell_manager *m_doorbell_manager;

        // Command completion events
        Vector<xhci_command_completion_trb_t *> m_command_completion_events;

        // Flag indicating we have a command completion event
        volatile uint8_t m_command_irq_completed = 0;

        Vector<uint8_t> m_usb3_ports;

        void process_events();

        void parse_capability_registers();
        void parse_extended_capabilities();

        void log_capability_registers();

        void log_operational_registers();

        void log_usbsts();

        static void claim_legacy_ownership(xhci_legacy_support_capability* legacy);

        bool is_usb3_port(uint8_t port_num);

        bool reset_host_controller();

        bool crcr_is_running();

        bool start_host_controller();

        void configure_operational_registers();

        void setup_dcbaa();

        void configure_runtime_registers();

        void acknowledge_irq(uint8_t interrupter);

        static irqreturn_t xhci_irq_handler(xhciDriver* driver);

        xhci_command_completion_trb_t *_send_command_trb(xhci_trb_t *cmd_trb, uint32_t timeout_ms = 200);
    };

} // namespace drivers

#endif // XHCI_H
