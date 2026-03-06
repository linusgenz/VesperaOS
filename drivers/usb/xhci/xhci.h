#ifndef XHCI_H
#define XHCI_H

#include "xhci_device.h"
#include "xhci_regs.h"
#include "xhci_rings.h"
#include <vector.h>
#include "../../pci/pci.h"
#include "../../../arch/x86_64/interrupts/idt.h"
#include "xhci_ext_cap.h"
#include "../../../filesystem/devfs/devfs.h"
#include "../../../include/kernel/devices/char_device.h"
#include "../../../include/kernel/sync/atomic.h"
#include "kernel/devices/device_manager.h"

namespace usb
{
    class XhciDriver final : public CharDevice
    {
    public:
        explicit XhciDriver(uint8_t vector_num, const char* name, uint8_t bus_number);

        [[nodiscard]] KernelDevice* get_device() const { return kd_; }

        ~XhciDriver() override = default;

        bool init_device(pci::PCI_DEVICE_HEADER* pci_base_address);

        XhciDevice* find_by_slot(uint8_t slot_id);

        bool start_device();

        static bool shutdown_device();

        void ring_doorbell(uint8_t slot, uint8_t ep) const;

        Vector<XhciDevice*> m_connected_devices;

        // Char device

        int open(CharFile** out_cf) override;
        int release(CharFile* cf) override;

        int ioctl(CharFile* cf, uint32_t cmd, void* arg) override;

        ssize_t read(CharFile* cf, void* buffer, size_t count, size_t offset) override;
        ssize_t write(CharFile* cf, const void* buffer, size_t count) override;

    private:
        KernelDevice* kd_;

        Spinlock devices_lock_{};
        Spinlock command_lock_{};
        Spinlock transfer_lock_{};
        Spinlock port_connection_lock_{};

        uint8_t bus_number_;
        uint8_t vector_num_{};

        uintptr_t xhc_base_{};

        volatile XHCI_CAPABILITY_REGISTERS* cap_regs_{};
        volatile XHCI_OPERATIONAL_REGISTERS* op_regs_{};
        volatile XHCI_RUNTIME_REGISTERS* runtime_regs_{};

        XhciExtendedCapability* extended_capabilities_head_{};

        // CAPLENGTH
        uint8_t capability_regs_length_{};

        // HCSPARAMS1
        uint8_t max_device_slots_{};
        uint8_t max_interrupters_{};
        uint8_t max_ports_{};

        // HCSPARAMS2
        uint8_t isochronous_scheduling_threshold_{};
        uint8_t erst_max_{};
        uint8_t max_scratchpad_buffers_{};

        // hccparams1
        bool _64bit_addressing_capability_{};
        bool bandwidth_negotiation_capability_{};
        bool _64byte_context_size_{};
        bool port_power_control_{};
        bool port_indicators_{};
        bool light_reset_capability_{};
        uint32_t extended_capabilities_offset_{};

        // Device context base address array's virtual address
        uint64_t* dcbaa_{};

        // Since DCBAA stores physical addresses, we want to keep
        // track of the virtual pointers to the output device contexts.
        uint64_t* dcbaa_virtual_addresses_{};

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

        struct XhciPortConnectionEvent
        {
            uint8_t port_id; // 1-based
            bool device_connected;
        };

        Vector<XhciPortConnectionEvent> port_connection_events_;

        atomic_flag_t command_irq_completed_;
        atomic_flag_t transfer_irq_completed_;

        Vector<uint8_t> usb3_ports_;

        void process_events();

        static const char* usb_speed_to_string(uint8_t speed);

        XHCI_PORTSC_REGISTER read_portsc_reg(uint8_t port_num);

        void write_portsc_reg(XHCI_PORTSC_REGISTER reg, uint8_t port_num);

        void clear_port(uint8_t port_num);

        bool reset_port(uint8_t port_num);

        static uint16_t get_max_initial_packet_size(uint8_t port_speed);

        [[nodiscard]] bool create_device_context(uint8_t slot_id) const;

        static void configure_control_ep_input_context(const XhciDevice* dev, uint16_t max_packet_size);

        static void configure_ep_input_context(const XhciDevice* dev, XhciEndpoint* endpoint);

        bool send_usb_request_packet(XhciDevice* device, XHCI_DEVICE_REQUEST_PACKET& req, void* output_buffer,
                                     uint32_t length);

        bool send_usb_no_data_request_packet(const XhciDevice* dev, const XHCI_DEVICE_REQUEST_PACKET& req);

        xhci_transfer_completion_trb_t* start_control_endpoint_transfer(const XhciTransferRing* transfer_ring);

        bool get_device_descriptor(XhciDevice* device, USB_DEVICE_DESCRIPTOR* desc, uint32_t length);

        bool evaluate_context(const XhciDevice* dev);

        bool get_string_descriptor(XhciDevice* device, uint8_t descriptor_index, uint8_t langid,
                                   USB_STRING_DESCRIPTOR* desc);

        bool get_string_language_descriptor(XhciDevice* device, USB_STRING_LANGUAGE_DESCRIPTOR* desc);

        bool get_configuration_descriptor(XhciDevice* device, UsbConfigurationDescriptor* desc);

        bool set_device_configuration(const XhciDevice* device, uint16_t configuration_value);

        bool get_hid_report_descriptor(XhciDevice* device, uint8_t interface_number, uint8_t descriptor_index,
                                       uint8_t* report_buffer, uint16_t report_length);

        bool configure_endpoint(const XhciDevice* device);

        bool setup_device(uint8_t port);

        bool address_device_command(const XhciDevice* dev, bool bsr);

        uint8_t assign_slot();

        void parse_capability_registers();

        void parse_extended_capabilities();

        void log_capability_registers();

        void log_operational_registers();

        void log_usbsts() const;

        static void claim_legacy_ownership(XHCI_LEGACY_SUPPORT_CAPABILITY* legacy);

        bool is_usb3_port(uint8_t port_num);

        XhciPortRegisterManager get_port_register_set(uint8_t port_num);

        uint8_t get_port_speed(uint8_t port);

        [[nodiscard]] bool reset_host_controller() const;

        [[nodiscard]] bool start_host_controller() const;

        void configure_operational_registers();

        void setup_dcbaa();

        void configure_runtime_registers();

        void acknowledge_irq(uint8_t interrupter) const;

        static Irqreturn xhci_irq_handler(XhciDriver* driver);

        xhci_command_completion_trb_t* send_command(xhci_trb_t* cmd_trb, uint32_t timeout_ms = 200);
    };
} // namespace USB

#endif // XHCI_H
