#ifndef XHCI_H
#define XHCI_H

#include "xhci_regs.h"
#include "xhci_rings.h"
#include "../../../include/vector.h"
#include "../../pci/pci.h"
#include "../../../arch/x86_64/interrupts/idt.h"
#include "xhci_ext_cap.h"
#include "xhci_device.h"
#include "../usb_descriptors.h"

inline bool flag;

namespace USB {
    class xhciDriver {
    public:
        xhciDriver();

        ~xhciDriver() = default;

        bool init_device(PCI::PCIDeviceHeader *pci_base_address);

        bool start_device();

        bool shutdown_device();

        xhci_port_register_manager get_port_register_set(uint8_t port_num);

        struct xhci_port_connection_event {
            uint8_t port_id;        // 1-based port number
            bool device_connected;  // true=connected false=disconnected
        };
        Vector<xhci_port_connection_event> m_port_connection_events;

        xhciDevice* m_connected_devices[64]{};
    private:
        uintptr_t m_xhc_base{};

        volatile xhci_capability_registers *m_cap_regs{};
        volatile xhci_operational_registers *m_op_regs{};
        volatile xhci_runtime_registers *m_runtime_regs{};

        xhci_extended_capability *extended_capabilities_head{};

        // CAPLENGTH
        uint8_t m_capability_regs_length{};

        // HCSPARAMS1
        uint8_t m_max_device_slots{};
        uint8_t m_max_interrupters{};
        uint8_t m_max_ports{};

        // HCSPARAMS2
        uint8_t m_isochronous_scheduling_threshold{};
        uint8_t m_erst_max{};
        uint8_t m_max_scratchpad_buffers{};

        // hccparams1
        bool m_64bit_addressing_capability{};
        bool m_bandwidth_negotiation_capability{};
        bool m_64byte_context_size{};
        bool m_port_power_control{};
        bool m_port_indicators{};
        bool m_light_reset_capability{};
        uint32_t m_extended_capabilities_offset{};

        // Device context base address array's virtual address
        uint64_t *m_dcbaa{};

        // Since DCBAA stores physical addresses, we want to keep
        // track of the virtual pointers to the output device contexts.
        uint64_t *m_dcbaa_virtual_addresses{};

        // Main command ring
        xhciCommandRing *m_command_ring{};

        // Main event ring
        xhciEventRing *m_event_ring{};

        // Doorbell register array manager
        xhci_doorbell_manager *m_doorbell_manager{};

        // Command completion events
        Vector<xhci_command_completion_trb_t *> m_command_completion_events;

        Vector<xhci_transfer_completion_trb_t*> m_transfer_completion_events;

        Vector<xhci_port_status_change_trb_t*> m_port_status_change_events;

        volatile uint8_t m_command_irq_completed = 0;
        volatile uint8_t m_transfer_irq_completed = 0;

        Vector<uint8_t> m_usb3_ports;

        void process_events();

        void parse_capability_registers();
        void parse_extended_capabilities();

        void log_capability_registers();

        void log_operational_registers();

        void log_usbsts() const;

        static void claim_legacy_ownership(xhci_legacy_support_capability* legacy);

        bool is_usb3_port(uint8_t port_num);

        bool reset_host_controller() const;

        bool crcr_is_running();

        bool start_host_controller() const;

        void configure_operational_registers();

        void setup_dcbaa();

        void set_dcbaa_entry(uint8_t slot_id, uint64_t output_ctx) const;

        void configure_runtime_registers();

        void acknowledge_irq(uint8_t interrupter) const;

        static irqreturn_t xhci_irq_handler(xhciDriver* driver);

        xhci_command_completion_trb_t *_send_command(xhci_trb_t *cmd_trb, uint32_t timeout_ms = 200);

        // port number is 0-based
        bool reset_port(uint8_t port_num);

        static const char* usb_speed_to_string(uint8_t speed);

        bool send_usb_request_packet(xhciDevice *device, xhci_device_request_packet &req, void *output_buffer,
                                     uint32_t length);

        bool send_usb_no_data_request_packet(xhciDevice *dev, xhci_device_request_packet &req);

        xhci_transfer_completion_trb_t *start_control_endpoint_transfer(const xhciTransferRing *transfer_ring);

        bool setup_device(uint8_t port, xhci_portsc_register portsc);

        bool evaluate_context(const xhciDevice *dev);

        uint8_t assign_slot();

        bool create_device_context(uint8_t slot_id) const;

        static void configure_control_ep_input_context(xhciDevice *dev, uint16_t max_packet_size);

        void configure_ep_input_context(xhciDevice *dev, xhciEndpoint *endpoint);

        static uint16_t get_max_initial_packet_size(uint8_t port_speed);

        bool address_device_command(xhciDevice* device, bool bsr);

        bool get_device_descriptor(xhciDevice* device, usb_device_descriptor* desc, uint32_t length);

        bool get_string_language_descriptor(xhciDevice *device, usb_string_language_descriptor *desc);

        bool get_string_descriptor(xhciDevice *device, uint8_t descriptor_index, uint8_t langid,
                                   usb_string_descriptor *desc);

        bool get_configuration_descriptor(xhciDevice *device, usb_configuration_descriptor *desc);

        bool get_hid_report_descriptor(xhciDevice *device, uint8_t interface_number, uint8_t descriptor_index,
                                       uint8_t *report_buffer, uint16_t report_length);

        bool set_device_configuration(xhciDevice *device, uint16_t configuration_value);
    };

} // namespace drivers

#endif // XHCI_H
