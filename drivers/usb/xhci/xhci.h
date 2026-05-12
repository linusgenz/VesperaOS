#ifndef XHCI_H
#define XHCI_H

#include <klib/vector.h>
#include <vespera/sync/atomic.h>

#include <arch/x86_64/interrupts/idt.h>
#include <drivers/pci/pci.h>
#include "vespera/devices/device_info.h"
#include "vespera/devices/device_manager.h"
#include "xhci_device.h"
#include "xhci_ext_cap.h"
#include "xhci_regs.h"
#include "xhci_rings.h"

namespace usb {
    class XhciDriver final : public IDeviceInfo{
       public:
        explicit XhciDriver(u8 vector_num, const char* name, u8 bus_number);

        [[nodiscard]] KernelDevice* get_device() const {
            return kd_;
        }

        ~XhciDriver() override = default;

        bool init_device(pci::PCI_DEVICE_HEADER* pci_base_address);

        XhciDevice* find_by_slot(u8 slot_id);

        bool start_device();

        static bool shutdown_device();

        void ring_doorbell(u8 slot, u8 ep) const;

        Vector<XhciDevice*> m_connected_devices;

        bool get_vendor(char* out, usize len) override;
        bool get_model(char* out, usize len) override;

       private:
        KernelDevice* kd_;

        UsbDeviceInfo* controller_info_;

        pci::PCI_HEADER0* pci_header_;

        Spinlock devices_lock_{};
        Spinlock command_lock_{};
        Spinlock transfer_lock_{};
        Spinlock port_connection_lock_{};

        u8 bus_number_;
        u8 vector_num_{};

        uptr xhc_base_{};

        volatile XHCI_CAPABILITY_REGISTERS* cap_regs_{};
        volatile XHCI_OPERATIONAL_REGISTERS* op_regs_{};
        volatile XHCI_RUNTIME_REGISTERS* runtime_regs_{};

        XhciExtendedCapability* extended_capabilities_head_{};

        // CAPLENGTH
        u8 capability_regs_length_{};

        // HCSPARAMS1
        u8 max_device_slots_{};
        u8 max_interrupters_{};
        u8 max_ports_{};

        // HCSPARAMS2
        u8 isochronous_scheduling_threshold_{};
        u8 erst_max_{};
        u8 max_scratchpad_buffers_{};

        // hccparams1
        bool _64bit_addressing_capability_{};
        bool bandwidth_negotiation_capability_{};
        bool _64byte_context_size_{};
        bool port_power_control_{};
        bool port_indicators_{};
        bool light_reset_capability_{};
        u32 extended_capabilities_offset_{};

        // Device context base address array's virtual address
        u64* dcbaa_{};

        // Since DCBAA stores physical addresses, we want to keep
        // track of the virtual pointers to the output device contexts.
        u64* dcbaa_virtual_addresses_{};

        // Main command ring
        XhciCommandRing* command_ring_{};

        // Main event ring
        XhciEventRing* event_ring_{};

        // Doorbell register array manager
        XhciDoorbellManager* doorbell_manager_{};

        // Command completion events
        Vector<xhci_command_completion_trb_t*> command_completion_events_;
        Vector<xhci_transfer_completion_trb_t*> transfer_completion_events_;
        Vector<xhci_port_status_change_trb_t*> port_status_change_events_;

        struct XhciPortConnectionEvent {
            u8 port_id;  // 1-based
            bool device_connected;
        };

        Vector<XhciPortConnectionEvent> port_connection_events_;

        atomic_flag_t command_irq_completed_;
        atomic_flag_t transfer_irq_completed_;

        Vector<u8> usb3_ports_;

        void process_events();

        static const char* usb_speed_to_string(u8 speed);

        XHCI_PORTSC_REGISTER read_portsc_reg(u8 port_num);

        void write_portsc_reg(XHCI_PORTSC_REGISTER reg, u8 port_num);

        void clear_port(u8 port_num);

        bool reset_port(u8 port_num);

        static u16 get_max_initial_packet_size(u8 port_speed);

        [[nodiscard]] bool create_device_context(u8 slot_id) const;

        static void configure_control_ep_input_context(const XhciDevice* dev, u16 max_packet_size);

        static void configure_ep_input_context(const XhciDevice* dev, const XhciEndpoint* endpoint);

        bool send_usb_request_packet(
            XhciDevice* device, XHCI_DEVICE_REQUEST_PACKET& req, void* output_buffer, u32 length
        );

        bool send_usb_no_data_request_packet(const XhciDevice* dev, const XHCI_DEVICE_REQUEST_PACKET& req);

        xhci_transfer_completion_trb_t* start_control_endpoint_transfer(const XhciTransferRing* transfer_ring);

        bool get_device_descriptor(XhciDevice* device, USB_DEVICE_DESCRIPTOR* desc, u32 length);

        bool evaluate_context(const XhciDevice* dev);

        bool get_string_descriptor(
            XhciDevice* device, u8 descriptor_index, u8 langid, USB_STRING_DESCRIPTOR* desc
        );

        bool get_string_language_descriptor(XhciDevice* device, USB_STRING_LANGUAGE_DESCRIPTOR* desc);

        bool get_configuration_descriptor(XhciDevice* device, UsbConfigurationDescriptor* desc);

        bool set_device_configuration(const XhciDevice* device, u16 configuration_value);

        bool get_hid_report_descriptor(
            XhciDevice* device, u8 interface_number, u8 descriptor_index, u8* report_buffer,
            u16 report_length
        );

        bool configure_endpoint(const XhciDevice* device);

        bool setup_device(u8 port);

        bool address_device_command(const XhciDevice* dev, bool bsr);

        u8 assign_slot();

        void parse_capability_registers();

        void parse_extended_capabilities();

        void log_capability_registers();

        void log_operational_registers();

        void log_usbsts() const;

        static void claim_legacy_ownership(XHCI_LEGACY_SUPPORT_CAPABILITY* legacy);

        bool is_usb3_port(u8 port_num);

        XhciPortRegisterManager get_port_register_set(u8 port_num);

        u8 get_port_speed(u8 port);

        [[nodiscard]] bool reset_host_controller() const;

        [[nodiscard]] bool start_host_controller() const;

        void configure_operational_registers();

        void setup_dcbaa();

        void configure_runtime_registers();

        void acknowledge_irq(u8 interrupter) const;

        static Irqreturn xhci_irq_handler(XhciDriver* driver);

        xhci_command_completion_trb_t* send_command(xhci_trb_t* cmd_trb, u32 timeout_ms = 200);
    };
}  // namespace usb

#endif  // XHCI_H
