#include "xhci.h"

#include <klib/encoding.h>
#include <klib/vector.h>
#include <vespera/devices/device_manager.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/system/system_manager.h>
#include <vespera/time.h>

#include "../../../filesystem/devfs/devfs.h"
#include "../../../kernel/cpu/cpu.h"
#include "../../pci/pci.h"
#include "../usb_manager.h"
#include "xhci_common.h"
#include "xhci_device.h"
#include "xhci_device_ctx.h"
#include "xhci_ext_cap.h"
#include "xhci_keyboard_driver.h"
#include "xhci_mass_storage_driver.h"

namespace usb {
    XhciDriver::XhciDriver(u8 vector_num, const char* name, u8 bus_number)
        : pci_hdr_(nullptr)
        , bus_number_(bus_number)
        , vector_num_(vector_num) {
        kd_ = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name)
                .set_type(DeviceType::Controller)
                .set_class(DeviceClass::Usb)
                .set_bus(BusType::Usb)
                .set_controller(ControllerType::Xhci)
            //    .with_lifecycle(this)
        );
        devices_lock_.init("xhci_device_lock");
        command_lock_.init("xhci_command_lock");
        transfer_lock_.init("xhci_transfer_lock");
        port_connection_lock_.init();
        command_irq_completed_.init();
        transfer_irq_completed_.init();

        DevFs::register_device(kd_);
    }

    bool XhciDriver::init_device(pci::PCI_DEVICE_HEADER* pci_base_address) {
        pci_hdr_ = reinterpret_cast<pci::PCI_HEADER0*>(pci_base_address);
        u64 bar0 = pci_hdr_->bar0 & ~0xF;
        u64 bar1 = pci_hdr_->bar1;
        u64 bar = ((bar1 << 32) | bar0);

        u32 original_bar0 = pci_hdr_->bar0;
        u32 original_bar1 = pci_hdr_->bar1;

        pci_hdr_->bar0 = 0xFFFFFFFF;
        pci_hdr_->bar1 = 0xFFFFFFFF;

        u32 size_mask_lo = pci_hdr_->bar0;
        u32 size_mask_hi = pci_hdr_->bar1;

        pci_hdr_->bar0 = original_bar0;
        pci_hdr_->bar1 = original_bar1;

        u64 mask = (static_cast<u64>(size_mask_hi) << 32) | (size_mask_lo & ~0xF);
        if (mask == 0) {
            return false;
        }

        u64 bar_size = ~(mask) + 1;

        xhc_base_ = xhci_map_mmio(bar, bar_size);

        // Read capability registers
        parse_capability_registers();
        //   log_capability_registers();
        parse_extended_capabilities();

        // Reset the host controller
        if (!reset_host_controller()) {
            return false;
        }

        // Setup operational registers
        configure_operational_registers();
        //   log_operational_registers();

        kernel::interrupts::allocate_vector(vector_num_, reinterpret_cast<irq_handler_t>(xhci_irq_handler), this);

        // Setup runtime registers
        configure_runtime_registers();
        return true;
    }

    XhciDevice* XhciDriver::find_by_slot(const u8 slot_id) {
        SpinlockGuardIrq guard(devices_lock_);
        for (auto* dev : m_connected_devices) {
            if (dev && dev->get_slot_id() == slot_id) {
                return dev;
            }
        }
        return nullptr;
    }

    bool XhciDriver::start_device() {
        if (!start_host_controller()) {
            Log::print_ln("Failed to start the host controller");
            UsbManager::notify_controller_ready();
            return false;
        }

        kernel::time::sleep_ms(100);

        if (in_qemu()) {
            for (u8 i = 0; i < max_ports_; i++) {
                XhciPortRegisterManager regman = get_port_register_set(i);
                XHCI_PORTSC_REGISTER portsc{};
                regman.read_portsc_reg(portsc);

                if (portsc.csc && portsc.ccs) {
                    XhciPortConnectionEvent conn_evt{};
                    conn_evt.port_id = i + 1;
                    conn_evt.device_connected = (portsc.ccs == 1);
                    port_connection_events_.push_back(conn_evt);
                }
            }
        }

        if (!port_connection_events_.empty()) {
            for (auto event : port_connection_events_) {
                if (event.device_connected) {
                    const u8 port_reg_idx = event.port_id - 1;

                    const bool reset_successful = reset_port(port_reg_idx);
                    kernel::time::sleep_ms(100);

                    if (reset_successful) {
                        XhciPortRegisterManager regman = get_port_register_set(port_reg_idx);
                        XHCI_PORTSC_REGISTER portsc{};
                        regman.read_portsc_reg(portsc);

                        Log::info("Device on port %u - %s", event.port_id, usb_speed_to_string(portsc.port_speed));
                        setup_device(port_reg_idx);
                    }
                }
            }
            port_connection_events_.clear();
        }

        UsbManager::notify_controller_ready();

        while (true) {
            kernel::time::sleep_ms(100);

            if (port_connection_events_.empty()) {
                continue;
            }

            for (auto [port_id, device_connected] : port_connection_events_) {
                const u8 port = port_id;
                const u8 port_reg_idx = port - 1;

                XhciPortRegisterManager regman = get_port_register_set(port_reg_idx);
                XHCI_PORTSC_REGISTER portsc{};
                regman.read_portsc_reg(portsc);

                if (device_connected) {
                    const bool reset_successful = reset_port(port_reg_idx);
                    kernel::time::sleep_ms(100);

                    if (reset_successful) {
                        Log::info("Device connected on port %u - %s", port, usb_speed_to_string(portsc.port_speed));
                        setup_device(port_reg_idx);
                    } else {
                        Log::warning("Failed to reset port %u after connection detection", port);
                    }
                } else {
                    Log::info("Device disconnected from port %u", port);

                    {
                        SpinlockGuardIrq guard(devices_lock_);
                        for (usize i = 0; i < m_connected_devices.size(); i++) {
                            auto* dev = m_connected_devices[i];

                            SYS_EVENT_DEVICE_REMOVED(dev->get_model_name(), port);

                            if (dev && dev->get_port_id() == port) {
                                for (auto* iface : dev->interfaces) {
                                    if (iface->driver) {
                                        iface->driver->detach();
                                        delete iface->driver;
                                        iface->driver = nullptr;
                                    }
                                }

                                delete dev;
                                m_connected_devices.erase(i);
                                break;
                            }
                        }
                    }

                    clear_port(port_reg_idx);
                }
            }

            port_connection_events_.clear();
        }
    }

    bool XhciDriver::shutdown_device() {
        return true;
    }

    void XhciDriver::parse_capability_registers() {
        cap_regs_ = reinterpret_cast<volatile XHCI_CAPABILITY_REGISTERS*>(xhc_base_);

        capability_regs_length_ = cap_regs_->caplength;

        max_device_slots_ = XHCI_MAX_DEVICE_SLOTS(cap_regs_);
        max_interrupters_ = XHCI_MAX_INTERRUPTERS(cap_regs_);
        max_ports_ = XHCI_MAX_PORTS(cap_regs_);

        isochronous_scheduling_threshold_ = XHCI_IST(cap_regs_);
        erst_max_ = XHCI_ERST_MAX(cap_regs_);
        max_scratchpad_buffers_ = XHCI_MAX_SCRATCHPAD_BUFFERS(cap_regs_);

        _64bit_addressing_capability_ = XHCI_AC64(cap_regs_);
        bandwidth_negotiation_capability_ = XHCI_BNC(cap_regs_);
        _64byte_context_size_ = XHCI_CSZ(cap_regs_);
        port_power_control_ = XHCI_PPC(cap_regs_);
        port_indicators_ = XHCI_PIND(cap_regs_);
        light_reset_capability_ = XHCI_LHRC(cap_regs_);
        extended_capabilities_offset_ = XHCI_XECP(cap_regs_) * sizeof(u32);

        // Update the base pointer to operational register set
        op_regs_ = reinterpret_cast<volatile XHCI_OPERATIONAL_REGISTERS*>(xhc_base_ + capability_regs_length_);

        // Update the base pointer to the runtime register set
        runtime_regs_ = reinterpret_cast<volatile XHCI_RUNTIME_REGISTERS*>(xhc_base_ + cap_regs_->rtsoff);

        // Construct a manager class instance for the doorbell register array
        doorbell_manager_ = new XhciDoorbellManager(xhc_base_ + cap_regs_->dboff);
    }

    void XhciDriver::parse_extended_capabilities() {
        volatile auto* head_cap_ptr = reinterpret_cast<volatile u32*>(xhc_base_ + extended_capabilities_offset_);

        extended_capabilities_head_ = new XhciExtendedCapability(head_cap_ptr);

        auto node = extended_capabilities_head_;
        while (node) {
            if (node->id() == XHCI_EXTENDED_CAPABILITY_CODE::SUPPORT_PROTOCOL) {
                XhciUsbSupportedProtocolCapability cap(node->base());

                u8 first_port = cap.compatible_port_offset - 1;
                u8 last_port = cap.compatible_port_offset - 1;

                if (cap.major_revision_version == 3) {
                    for (u8 port = first_port; port <= last_port; port++) {
                        usb3_ports_.push_back(port);
                    }
                }
            }

            if (node->id() == XHCI_EXTENDED_CAPABILITY_CODE::USB_LEGACY_SUPPORT) {
                XHCI_LEGACY_SUPPORT_CAPABILITY legacy(node->base());
                claim_legacy_ownership(&legacy);
            }

            if (node->next() == nullptr) break;
            node = node->next();
        }
    }

    void XhciDriver::log_capability_registers() {
        Log::print_ln("===== Xhci Capability Registers (0x%llx) =====", reinterpret_cast<u64>(cap_regs_));
        Log::print_ln("    Length                : %u", capability_regs_length_);
        Log::print_ln("    Max Device Slots      : %u", max_device_slots_);
        Log::print_ln("    Max Interrupters      : %u", max_interrupters_);
        Log::print_ln("    Max Ports             : %u", max_ports_);
        Log::print_ln("    IST                   : %u", isochronous_scheduling_threshold_);
        Log::print_ln("    ERST Max Size         : %u", erst_max_);
        Log::print_ln("    Scratchpad Buffers    : %u", max_scratchpad_buffers_);
        Log::print_ln("    64-bit Addressing     : %s", _64bit_addressing_capability_ ? "yes" : "no");
        Log::print_ln("    Bandwidth Negotiation : %u", bandwidth_negotiation_capability_);
        Log::print_ln("    64-byte Context Size  : %s", _64byte_context_size_ ? "yes" : "no");
        Log::print_ln("    Port Power Control    : %u", port_power_control_);
        Log::print_ln("    Port Indicators       : %u", port_indicators_);
        Log::print_ln("    Light Reset Available : %u", light_reset_capability_);
        Log::print_ln("");
    }

    void XhciDriver::log_operational_registers() {
        Log::print_ln("===== Xhci Operational Registers (0x%llx) =====", reinterpret_cast<u64>(op_regs_));
        Log::print_ln("    usbcmd     : 0x%x", op_regs_->usbcmd);
        Log::print_ln("    usbsts     : 0x%x", op_regs_->usbsts);
        Log::print_ln("    pagesize   : 0x%x", op_regs_->pagesize);
        Log::print_ln("    dnctrl     : 0x%x", op_regs_->dnctrl);
        Log::print_ln("    crcr       : 0x%llx", op_regs_->crcr);
        Log::print_ln("    dcbaap     : 0x%llx", op_regs_->dcbaap);
        Log::print_ln("    config     : 0x%x", op_regs_->config);
        Log::print_ln("");
    }

    void XhciDriver::log_usbsts() const {
        u32 status = op_regs_->usbsts;
        Log::print_ln("===== USBSTS =====");
        if (status & XHCI_USBSTS_HCH) Log::print_ln("    Host Controlled Halted");
        if (status & XHCI_USBSTS_HSE) Log::print_ln("    Host System Error");
        if (status & XHCI_USBSTS_EINT) Log::print_ln("    Event Interrupt");
        if (status & XHCI_USBSTS_PCD) Log::print_ln("    Port Change Detect");
        if (status & XHCI_USBSTS_SSS) Log::print_ln("    Save State Status");
        if (status & XHCI_USBSTS_RSS) Log::print_ln("    Restore State Status");
        if (status & XHCI_USBSTS_SRE) Log::print_ln("    Save/Restore Error");
        if (status & XHCI_USBSTS_CNR) Log::print_ln("    Controller Not Ready");
        if (status & XHCI_USBSTS_HCE) Log::print_ln("    Host Controller Error");
        Log::print_ln("");
    }

    void XhciDriver::ring_doorbell(u8 slot, u8 ep) const {
        doorbell_manager_->ring_doorbell(slot, ep);
    }

    void XhciDriver::claim_legacy_ownership(XHCI_LEGACY_SUPPORT_CAPABILITY* legacy) {
        // Set OS_OWNED bit (bit 24)
        legacy->usblegsup.os_owned = 1;

        // Wait for BIOS to clear BIOS_OWNED (bit 16)
        int waited = 0;

        while (legacy->usblegsup.bios_owned == 1) {
            kernel::time::sleep_ms(10);
            if (constexpr int max_wait_ms = 100; ++waited >= max_wait_ms) {
                Log::error("BIOS did not release xHCI ownership after %d ms", waited);
                break;
            }
        }

        if (legacy->usblegsup.bios_owned != 1) {
            //    Log::PrintLn("BIOS released ownership after %d ms", waited);
        }

        // Clear SMI bits: bits 29–31 (RW1C)
        legacy->usblegctlsts.raw = (1 << 29) | (1 << 30) | (1 << 31);
    }

    bool XhciDriver::is_usb3_port(u8 port_num) {
        for (const unsigned char m_usb3_port : usb3_ports_) {
            if (m_usb3_port == port_num) {
                return true;
            }
        }
        return false;
    }

    XhciPortRegisterManager XhciDriver::get_port_register_set(const u8 port_num) {
        const u64 base = reinterpret_cast<u64>(op_regs_) + (0x400 + (0x10 * port_num));
        return XhciPortRegisterManager(base);
    }

    u8 XhciDriver::get_port_speed(const u8 port) {
        auto port_register_set = get_port_register_set(port);
        XHCI_PORTSC_REGISTER portsc{};
        port_register_set.read_portsc_reg(portsc);

        return static_cast<u8>(portsc.port_speed);
    }

    bool XhciDriver::reset_host_controller() const {
        // Make sure we clear the Run/Stop bit
        u32 usbcmd = op_regs_->usbcmd;
        usbcmd &= ~XHCI_USBCMD_RUN_STOP;
        op_regs_->usbcmd = usbcmd;

        // Wait for the HCHalted bit to be set
        u32 timeout = 20;
        while (!(op_regs_->usbsts & XHCI_USBSTS_HCH)) {
            if (--timeout == 0) {
                Log::print_ln("Host controller did not halt within %ums", 200);
                return false;
            }

            kernel::time::sleep_ms(10);
        }

        // Set the HC Reset bit
        usbcmd = op_regs_->usbcmd;
        usbcmd |= XHCI_USBCMD_HCRESET;
        op_regs_->usbcmd = usbcmd;

        // Wait for this bit and CNR bit to clear
        timeout = 100;
        while (op_regs_->usbcmd & XHCI_USBCMD_HCRESET || op_regs_->usbsts & XHCI_USBSTS_CNR) {
            if (--timeout == 0) {
                Log::print_ln("Host controller did not reset within %ums", 1000);
                return false;
            }

            kernel::time::sleep_ms(10);
        }

        kernel::time::sleep_ms(50);

        // Check the defaults of the operational registers
        if (op_regs_->usbcmd != 0) return false;

        if (op_regs_->dnctrl != 0) return false;

        if (op_regs_->crcr != 0) return false;

        if (op_regs_->dcbaap != 0) return false;

        if (op_regs_->config != 0) return false;

        return true;
    }

    void XhciDriver::configure_operational_registers() {
        // Enable device notifications
        op_regs_->dnctrl = 0xffff;

        // Configure the usbconfig field
        op_regs_->config = static_cast<u32>(max_device_slots_);

        // Setup device context base address array and scratchpad buffers
        setup_dcbaa();

        // Set up the command ring and write CRCR
        command_ring_ = new XhciCommandRing(XHCI_COMMAND_RING_TRB_COUNT);

        kernel::time::sleep_ms(100);
        op_regs_->crcr = command_ring_->get_physical_base() | command_ring_->get_cycle_bit();
    }

    void XhciDriver::setup_dcbaa() {
        usize dcbaa_size = sizeof(uptr) * (max_device_slots_ + 1);

        dcbaa_ = static_cast<u64*>(
            alloc_xhci_memory(dcbaa_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY)
        );

        dcbaa_virtual_addresses_ = new u64[max_device_slots_ + 1];

        if (max_scratchpad_buffers_ > 0) {
            auto* scratchpad_array = static_cast<u64*>(alloc_xhci_memory(
                max_scratchpad_buffers_ * sizeof(u64), XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY
            ));

            // Create scratchpad pages
            for (u8 i = 0; i < max_scratchpad_buffers_; i++) {
                void* scratchpad =
                    alloc_xhci_memory(PAGE_SIZE, XHCI_SCRATCHPAD_BUFFERS_ALIGNMENT, XHCI_SCRATCHPAD_BUFFERS_BOUNDARY);

                u64 scratchpad_paddr = xhci_get_physical_addr(scratchpad);
                scratchpad_array[i] = scratchpad_paddr;
            }

            u64 scratchpad_array_physical_base = xhci_get_physical_addr(scratchpad_array);

            // Set the first slot in the DCBAA to point to the scratchpad array
            dcbaa_[0] = scratchpad_array_physical_base;

            dcbaa_virtual_addresses_[0] = reinterpret_cast<u64>(scratchpad_array);
        }

        // Set DCBAA pointer in the operational registers
        op_regs_->dcbaap = xhci_get_physical_addr(dcbaa_);
    }

    void XhciDriver::acknowledge_irq(u8 interrupter) const {
        // Get the interrupter registers
        volatile XHCI_INTERRUPTER_REGISTERS* interrupter_regs = &runtime_regs_->ir[interrupter];

        // Read the current value of IMAN
        u32 iman = interrupter_regs->iman;

        // Set the IP bit to '1' to clear it, preserve other bits including IE
        iman |= XHCI_IMAN_INTERRUPT_PENDING;

        // Write back to IMAN
        interrupter_regs->iman = iman;

        // Clear the EINT bit in USBSTS by writing '1' to it
        op_regs_->usbsts = XHCI_USBSTS_EINT;
    }

    xhci_command_completion_trb_t* XhciDriver::send_command(xhci_trb_t* cmd_trb, u32 timeout_ms) {
        command_ring_->enqueue(cmd_trb);

        doorbell_manager_->ring_command_doorbell();

        // Wait for the IRQ and let the host controller process the command
        u64 sleep_passed = 0;
        while (!command_irq_completed_.load()) {
            kernel::time::sleep_ms(10);
            sleep_passed += 10;

            if (sleep_passed > timeout_ms) {
                break;
            }
        }

        xhci_command_completion_trb_t* completion_trb = nullptr;
        {
            SpinlockGuardIrq guard(command_lock_);
            //  - Only one command is being sent to the controller at a time
            completion_trb = !command_completion_events_.empty() ? command_completion_events_[0] : nullptr;

            // Reset the irq flag and clear out the command completion event queue
            command_completion_events_.clear();
        }
        command_irq_completed_.clear();

        if (!completion_trb) {
            Log::error("Failed to find completion TRB for command %u", cmd_trb->trb_type);
            return nullptr;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::error(
                "Command TRB failed with error: %s", trb_completion_code_to_string(completion_trb->completion_code)
            );
            return nullptr;
        }

        return completion_trb;
    }

    void XhciDriver::configure_runtime_registers() {
        volatile XHCI_INTERRUPTER_REGISTERS* interrupter_regs = &runtime_regs_->ir[0];

        interrupter_regs->iman = 0;

        // Clear any pending interrupts
        op_regs_->usbsts = XHCI_USBSTS_EINT;  // Clear EINT bit

        event_ring_ = new XhciEventRing(XHCI_EVENT_RING_TRB_COUNT, interrupter_regs);

        u64 erdp = event_ring_->get_physical_base();
        interrupter_regs->erdp = erdp;

        interrupter_regs->imod = 0;
        // Clear IP bit before enabling
        interrupter_regs->iman = XHCI_IMAN_INTERRUPT_PENDING | XHCI_IMAN_INTERRUPT_ENABLE;
    }

    bool XhciDriver::start_host_controller() const {
        op_regs_->usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;

        op_regs_->usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;
        asm volatile("" ::: "memory");

        op_regs_->usbcmd |= XHCI_USBCMD_RUN_STOP;

        // Wait for controller to start
        int retries = 0;

        while (op_regs_->usbsts & XHCI_USBSTS_HCH) {
            if (constexpr int max_retries = 100; retries++ >= max_retries) {
                Log::error("Controller failed to start within timeout");
                return false;
            }
            kernel::time::sleep_ms(10);
        }

        // Verify CNR bit is clear
        if (op_regs_->usbsts & XHCI_USBSTS_CNR) {
            Log::error("Controller Not Ready after start");
            return false;
        }

        return true;
    }

    Irqreturn XhciDriver::xhci_irq_handler(XhciDriver* driver) {
        driver->process_events();
        driver->acknowledge_irq(0);

        return IRQ_HANDLED;
    }

    void XhciDriver::process_events() {
        Vector<xhci_trb_t*> events;

        if (event_ring_->has_unprocessed_events()) {
            event_ring_->dequeue_events(events);
        }

        u8 command_completion_status = 0;
        u8 transfer_completion_status = 0;

        for (auto event : events) {
            switch (event->trb_type) {
                case XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT: {
                    auto port_evt = reinterpret_cast<xhci_port_status_change_trb_t*>(event);
                    port_status_change_events_.push_back(port_evt);

                    XhciPortRegisterManager regman = get_port_register_set(port_evt->port_id - 1);
                    XHCI_PORTSC_REGISTER portsc{};
                    regman.read_portsc_reg(portsc);

                    if (portsc.csc) {
                        XhciPortConnectionEvent conn_evt{};
                        conn_evt.port_id = port_evt->port_id;
                        conn_evt.device_connected = (portsc.ccs == 1);
                        port_connection_events_.push_back(conn_evt);
                    }
                    break;
                }
                case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT: {
                    command_completion_status = 1;
                    {
                        SpinlockGuardIrq guard(command_lock_);
                        command_completion_events_.push_back(reinterpret_cast<xhci_command_completion_trb_t*>(event));
                    }
                    break;
                }
                case XHCI_TRB_TYPE_TRANSFER_EVENT: {
                    transfer_completion_status = 1;
                    auto transfer_event = reinterpret_cast<xhci_transfer_completion_trb_t*>(event);
                    {
                        SpinlockGuardIrq guard(transfer_lock_);
                        transfer_completion_events_.push_back(transfer_event);
                    }

                    const auto device = find_by_slot(transfer_event->slot_id);
                    if (!device) {
                        break;
                    }

                    if (const auto& primary_interface = device->interfaces[0]; primary_interface->driver) {
                        primary_interface->driver->on_event(this, device);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        command_irq_completed_.set(command_completion_status);
        transfer_irq_completed_.set(transfer_completion_status);
    }

    const char* XhciDriver::usb_speed_to_string(u8 speed) {
        static const char* speed_string[7] = {
            "Invalid",
            "Full Speed (12 MB/s - USB2.0)",
            "Low Speed (1.5 Mb/s - USB 2.0)",
            "High Speed (480 Mb/s - USB 2.0)",
            "Super Speed (5 Gb/s - USB3.0)",
            "Super Speed Plus (10 Gb/s - USB 3.1)",
            "Undefined"
        };

        return speed_string[speed];
    }

    XHCI_PORTSC_REGISTER XhciDriver::read_portsc_reg(u8 port_num) {
        u64 reg_base = reinterpret_cast<u64>(op_regs_) + (0x400 + (0x10 * port_num));

        XHCI_PORTSC_REGISTER reg{};
        reg.raw = *reinterpret_cast<volatile u32*>(reg_base);

        return reg;
    }

    void XhciDriver::write_portsc_reg(XHCI_PORTSC_REGISTER reg, u8 port_num) {
        u64 reg_base = reinterpret_cast<u64>(op_regs_) + (0x400 + (0x10 * port_num));
        *reinterpret_cast<volatile u32*>(reg_base) = reg.raw;
    }

    void XhciDriver::clear_port(u8 port_num) {
        XHCI_PORTSC_REGISTER portsc = read_portsc_reg(port_num);
        portsc.csc = 1;  // clear connect status change
        portsc.pec = 1;
        portsc.prc = 1;
        portsc.wrc = 1;
        write_portsc_reg(portsc, port_num);
    }

    bool XhciDriver::reset_port(u8 port_num) {
        XHCI_PORTSC_REGISTER portsc = read_portsc_reg(port_num);

        bool is_usb3 = is_usb3_port(port_num);

        // Power on the port if necessary
        if (portsc.pp == 0) {
            portsc.pp = 1;
            write_portsc_reg(portsc, port_num);
            kernel::time::sleep_ms(20);  // Wait for power stabilization
            portsc = read_portsc_reg(port_num);

            if (portsc.pp == 0) {
                Log::warning("Port %u: Failed to power on port", port_num);
                return false;
            }
        }

        // Clear any lingering status change bits before initiating the reset
        portsc.csc = 1;  // Clear connect status change
        portsc.pec = 1;  // Clear port enable/disable change
        portsc.prc = 1;  // Clear port reset change
        write_portsc_reg(portsc, port_num);

        // Initiate the port reset
        if (is_usb3) {
            portsc.wpr = 1;  // Warm reset for USB 3.0
        } else {
            portsc.pr = 1;  // Standard port reset for USB 2.0
        }
        write_portsc_reg(portsc, port_num);

        // Wait for the reset to complete
        int timeout = 50;
        while (timeout > 0) {
            portsc = read_portsc_reg(port_num);

            if ((is_usb3 && portsc.wrc) || (!is_usb3 && portsc.prc)) {
                break;  // Reset has completed
            }

            timeout--;
            kernel::time::sleep_ms(10);
        }

        if (timeout == 0) {
            Log::warning("Port %u: Port reset timed out", port_num);
            return false;
        }

        kernel::time::sleep_ms(10);

        // Clear the reset completion and status change bits
        portsc.prc = 1;  // Clear port reset change
        portsc.wrc = 1;  // Clear warm reset change (USB 3.0)
        portsc.csc = 1;  // Clear connect status change
        portsc.pec = 1;  // Clear port enable/disable change
        portsc.ped = 0;  // Don't clear the PED bit
        write_portsc_reg(portsc, port_num);

        kernel::time::sleep_ms(10);

        // Re-read the register to check if the port is enabled
        portsc = read_portsc_reg(port_num);

        // This case could happen when the port has been reset after
        // a device disconnect event, and no device has connected since.
        if (portsc.ped == 0) {
            return false;
        }

        return true;
    }

    u16 XhciDriver::get_max_initial_packet_size(u8 port_speed) {
        u16 initial_max_packet_size = 0;
        switch (port_speed) {
            case XHCI_USB_SPEED_LOW_SPEED:
                initial_max_packet_size = 8;
                break;

            case XHCI_USB_SPEED_FULL_SPEED:
            case XHCI_USB_SPEED_HIGH_SPEED:
                initial_max_packet_size = 64;
                break;

            case XHCI_USB_SPEED_SUPER_SPEED:
            case XHCI_USB_SPEED_SUPER_SPEED_PLUS:
            default:
                initial_max_packet_size = 512;
                break;
        }

        return initial_max_packet_size;
    }

    bool XhciDriver::create_device_context(u8 slot_id) const {
        // Allocate a memory block for the device context
        u64 device_context_size = _64byte_context_size_ ? sizeof(XHCI_DEVICE_CONTEXT64) : sizeof(XHCI_DEVICE_CONTEXT32);

        void* ctx = alloc_xhci_memory(device_context_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY);
        memset(ctx, 0, device_context_size);

        if (!ctx) {
            Log::error("Failed to allocate memory for a device context");
            return false;
        }

        dcbaa_[slot_id] = xhci_get_physical_addr(ctx);

        // Store the virtual address as well
        dcbaa_virtual_addresses_[slot_id] = reinterpret_cast<u64>(ctx);

        return true;
    }

    void XhciDriver::configure_control_ep_input_context(const XhciDevice* dev, u16 max_packet_size) {
        XHCI_INPUT_CONTROL_CONTEXT32* input_control_context = dev->get_input_control_ctx();
        XHCI_SLOT_CONTEXT32* slot_context = dev->get_input_slot_ctx();
        XHCI_ENDPOINT_CONTEXT32* control_ep_context = dev->get_input_control_ep_ctx();

        // Enable slot and control endpoint contexts
        input_control_context->add_flags = (1 << 0) | (1 << 1);
        input_control_context->drop_flags = 0;

        // Configure the slot context
        slot_context->context_entries = 1;
        slot_context->speed = dev->get_speed();
        slot_context->root_hub_port_num = dev->get_port_id();
        slot_context->route_string = 0;
        slot_context->interrupter_target = 0;

        // Configure the control endpoint context
        control_ep_context->endpoint_state = XHCI_ENDPOINT_STATE_DISABLED;
        control_ep_context->endpoint_type = XHCI_ENDPOINT_TYPE_CONTROL;
        control_ep_context->interval = 0;
        control_ep_context->error_count = 3;
        control_ep_context->max_packet_size = max_packet_size;
        control_ep_context->transfer_ring_dequeue_ptr =
            dev->get_control_transfer_ring()->get_physical_dequeue_pointer_base();
        control_ep_context->dcs = dev->get_control_transfer_ring()->get_cycle_bit();
        control_ep_context->max_esit_payload_lo = 0;
        control_ep_context->max_esit_payload_hi = 0;
        control_ep_context->average_trb_length = 8;
    }

    void XhciDriver::configure_ep_input_context(const XhciDevice* dev, XhciEndpoint* endpoint) {
        XHCI_INPUT_CONTROL_CONTEXT32* input_control_context = dev->get_input_control_ctx();
        XHCI_SLOT_CONTEXT32* slot_context = dev->get_input_slot_ctx();

        // Enable the input control context flags
        input_control_context->add_flags |= (1 << endpoint->xhc_endpoint_num);
        input_control_context->drop_flags = 0;

        if (endpoint->xhc_endpoint_num > slot_context->context_entries) {
            slot_context->context_entries = endpoint->xhc_endpoint_num;
        }

        // Configure the endpoint context
        XHCI_ENDPOINT_CONTEXT32* interrupt_ep_context = dev->get_input_ep_ctx(endpoint->xhc_endpoint_num);

        memset(interrupt_ep_context, 0, sizeof(XHCI_ENDPOINT_CONTEXT32));
        interrupt_ep_context->endpoint_state = XHCI_ENDPOINT_STATE_DISABLED;
        interrupt_ep_context->endpoint_type = endpoint->xhc_endpoint_type;
        interrupt_ep_context->max_packet_size = endpoint->max_packet_size;
        interrupt_ep_context->max_esit_payload_lo = endpoint->max_packet_size;
        interrupt_ep_context->error_count = 3;
        interrupt_ep_context->max_burst_size = 0;
        interrupt_ep_context->average_trb_length = endpoint->max_packet_size;
        interrupt_ep_context->transfer_ring_dequeue_ptr =
            endpoint->get_transfer_ring()->get_physical_dequeue_pointer_base();
        interrupt_ep_context->dcs = endpoint->get_transfer_ring()->get_cycle_bit();

        if (dev->get_speed() == XHCI_USB_SPEED_HIGH_SPEED || dev->get_speed() == XHCI_USB_SPEED_SUPER_SPEED) {
            interrupt_ep_context->interval = endpoint->interval - 1;
        } else {
            interrupt_ep_context->interval = endpoint->interval;

            if (endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_INTERRUPT_IN ||
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_INTERRUPT_OUT ||
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_ISOCHRONOUS_IN ||
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_ISOCHRONOUS_OUT) {
                if (endpoint->interval < 3) {
                    interrupt_ep_context->interval = 3;
                } else if (endpoint->interval > 18) {
                    interrupt_ep_context->interval = 18;
                }
            }
        }
    }

    bool XhciDriver::send_usb_request_packet(
        XhciDevice* device, XHCI_DEVICE_REQUEST_PACKET& req, void* output_buffer, u32 length
    ) {
        XhciTransferRing* transfer_ring = device->get_control_transfer_ring();

        auto* transfer_status_buffer = static_cast<u32*>(alloc_xhci_memory(sizeof(u32), 16, 16));
        auto* descriptor_buffer = static_cast<u8*>(alloc_xhci_memory(length, 256, 65536));

        xhci_setup_stage_trb_t setup_stage{};
        setup_stage.trb_type = XHCI_TRB_TYPE_SETUP_STAGE;
        setup_stage.request_packet = req;
        setup_stage.trb_transfer_length = 8;
        setup_stage.interrupter_target = 0;
        setup_stage.trt = 3;
        setup_stage.idt = 1;
        setup_stage.ioc = 0;

        xhci_data_stage_trb_t data_stage{};
        data_stage.trb_type = XHCI_TRB_TYPE_DATA_STAGE;
        data_stage.data_buffer = xhci_get_physical_addr(descriptor_buffer);
        data_stage.trb_transfer_length = length;
        data_stage.td_size = 0;
        data_stage.interrupter_target = 0;
        data_stage.dir = 1;
        data_stage.chain = 1;
        data_stage.ioc = 0;
        data_stage.idt = 0;

        // Clear the status buffer
        *transfer_status_buffer = 0;

        xhci_event_data_trb_t event_data_first{};
        event_data_first.trb_type = XHCI_TRB_TYPE_EVENT_DATA;
        event_data_first.data = xhci_get_physical_addr(transfer_status_buffer);
        event_data_first.interrupter_target = 0;
        event_data_first.chain = 0;
        event_data_first.ioc = 1;

        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&setup_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&data_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&event_data_first));

        // QEMU doesn't quite handle SETUP/DATA/STATUS transactions correctly.
        // It will wait for the STATUS TRB before it completes the transfer.
        // Technically, you need to check for a good transfer before you send the
        //  STATUS TRB.  However, since QEMU doesn't update the status until after
        //  the STATUS TRB, waiting here will not complete a successful transfer.
        //  Bochs and real hardware handles this correctly, however QEMU does not.
        // If you are using QEMU, do not ring the doorbell here.  Ring the doorbell
        //  *after* you place the STATUS TRB on the ring.
        // (See bug report: https://bugs.launchpad.net/qemu/+bug/1859378 )
        if (!in_qemu()) {
            if (auto completion_trb = start_control_endpoint_transfer(transfer_ring); !completion_trb) {
                free_xhci_memory(transfer_status_buffer);
                free_xhci_memory(descriptor_buffer);
                return false;
            }
        }

        xhci_status_stage_trb_t status_stage{};
        status_stage.trb_type = XHCI_TRB_TYPE_STATUS_STAGE;
        status_stage.interrupter_target = 0;
        status_stage.chain = 1;
        status_stage.ioc = 0;
        status_stage.dir = 0;

        // Clear the status buffer
        *transfer_status_buffer = 0;

        xhci_event_data_trb_t event_data_second{};
        event_data_second.trb_type = XHCI_TRB_TYPE_EVENT_DATA;
        event_data_second.ioc = 1;

        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&status_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&event_data_second));

        if (!start_control_endpoint_transfer(transfer_ring)) {
            free_xhci_memory(transfer_status_buffer);
            free_xhci_memory(descriptor_buffer);
            return false;
        }

        // Copy the descriptor into the requested user buffer location
        memcpy(output_buffer, descriptor_buffer, length);

        free_xhci_memory(transfer_status_buffer);
        free_xhci_memory(descriptor_buffer);

        return true;
    }

    bool XhciDriver::send_usb_no_data_request_packet(const XhciDevice* dev, const XHCI_DEVICE_REQUEST_PACKET& req) {
        XhciTransferRing* transfer_ring = dev->get_control_transfer_ring();
        if (!transfer_ring) {
            Log::error("No control transfer ring allocated.");
            return false;
        }

        xhci_setup_stage_trb_t setup_stage{};
        memset(&setup_stage, 0, sizeof(setup_stage));
        setup_stage.trb_type = XHCI_TRB_TYPE_SETUP_STAGE;

        setup_stage.request_packet = req;

        // TRT=0 => no data stage
        // If (bmRequestType & 0x80) and wLength>0 => TRT=3 (IN data)
        // If (!(bmRequestType & 0x80)) and wLength>0 => TRT=2 (OUT data)
        setup_stage.trt = 0;                  // No data stage
        setup_stage.idt = 1;                  // Immediate Data
        setup_stage.ioc = 0;                  // We'll complete on the Status Stage or Event Data
        setup_stage.trb_transfer_length = 8;  // Setup packet length is always 8

        xhci_status_stage_trb_t status_stage{};
        memset(&status_stage, 0, sizeof(status_stage));
        status_stage.trb_type = XHCI_TRB_TYPE_STATUS_STAGE;

        // For a host->device (or no-data) control transfer, the status stage is an IN handshake => dir=1
        status_stage.dir = 1;    // 1 = IN handshake
        status_stage.chain = 0;  // or 1 if you want to chain to an Event Data TRB
        status_stage.ioc = 1;    // Interrupt on completion

        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&setup_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&status_stage));

        if (const auto completion_trb = start_control_endpoint_transfer(transfer_ring); !completion_trb) {
            Log::error("No-Data request: Timed out or failed.");
            return false;
        }

        return true;
    }

    xhci_transfer_completion_trb_t* XhciDriver::start_control_endpoint_transfer(const XhciTransferRing* transfer_ring) {
        doorbell_manager_->ring_control_endpoint_doorbell(transfer_ring->get_doorbell_id());

        u64 sleep_passed = 0;

        while (!transfer_irq_completed_.load()) {
            kernel::time::sleep_ms(10);
            sleep_passed += 10;

            if (constexpr u64 timeout_ms = 400; sleep_passed > timeout_ms) {
                break;
            }
        }

        xhci_transfer_completion_trb_t* completion_trb = nullptr;
        {
            SpinlockGuardIrq guard(transfer_lock_);
            completion_trb = !transfer_completion_events_.empty() ? transfer_completion_events_[0] : nullptr;

            // Reset the irq flag and clear out the command completion event queue
            transfer_completion_events_.clear();
        }
        transfer_irq_completed_.clear();

        if (!completion_trb) {
            Log::warning("Failed to find transfer completion TRB");
            return nullptr;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::warning(
                "Transfer TRB failed with error: %s", trb_completion_code_to_string(completion_trb->completion_code)
            );
            return nullptr;
        }

        return completion_trb;
    }

    bool XhciDriver::get_device_descriptor(XhciDevice* device, USB_DEVICE_DESCRIPTOR* desc, u32 length) {
        XHCI_DEVICE_REQUEST_PACKET req{};
        req.b_request_type = 0x80;  // Device to Host, Standard, Device
        req.b_request = USB_GET_DESCRIPTOR_REQ;
        req.w_value = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_DEVICE, 0);
        req.w_index = 0;
        req.w_length = length;

        return send_usb_request_packet(device, req, desc, length);
    }

    bool XhciDriver::evaluate_context(const XhciDevice* dev) {
        xhci_evaluate_context_command_trb_t evaluate_context_trb{};
        evaluate_context_trb.trb_type = XHCI_TRB_TYPE_EVALUATE_CONTEXT_CMD;
        evaluate_context_trb.input_context_physical_base = dev->get_input_context_phys();
        evaluate_context_trb.slot_id = dev->get_slot_id();

        xhci_command_completion_trb_t* completion_trb =
            send_command(reinterpret_cast<xhci_trb_t*>(&evaluate_context_trb), 200);

        if (!completion_trb) {
            Log::error("Failed to send Evaluate Context command");
            return false;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::error(
                "Evaluate Context command failed with completion code: %s",
                trb_completion_code_to_string(completion_trb->completion_code)
            );
            return false;
        }

        return true;
    }

    bool XhciDriver::get_string_descriptor(
        XhciDevice* device, u8 descriptor_index, u8 langid, USB_STRING_DESCRIPTOR* desc
    ) {
        XHCI_DEVICE_REQUEST_PACKET req{};
        req.b_request_type = 0x80;  // Device to Host, Standard, Device
        req.b_request = USB_GET_DESCRIPTOR_REQ;
        req.w_value = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_STRING, descriptor_index);
        req.w_index = langid;
        req.w_length = sizeof(USB_DESCRIPTOR_HEADER);

        if (!send_usb_request_packet(device, req, desc, sizeof(USB_DESCRIPTOR_HEADER))) {
            Log::warning("Failed to read device string descriptor header");
            return false;
        }

        // Read the entire desc
        req.w_length = desc->header.b_length;

        if (!send_usb_request_packet(device, req, desc, desc->header.b_length)) {
            Log::warning("Failed to read device string descriptor");
            return false;
        }

        return true;
    }

    bool XhciDriver::get_string_language_descriptor(XhciDevice* device, USB_STRING_LANGUAGE_DESCRIPTOR* desc) {
        XHCI_DEVICE_REQUEST_PACKET req{};
        req.b_request_type = 0x80;
        req.b_request = USB_GET_DESCRIPTOR_REQ;
        req.w_value = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_STRING, 0);
        req.w_index = 0;
        req.w_length = sizeof(USB_DESCRIPTOR_HEADER);

        if (!send_usb_request_packet(device, req, desc, sizeof(USB_DESCRIPTOR_HEADER))) {
            Log::warning("Failed to read device string language descriptor header");
            return false;
        }

        req.w_length = desc->header.b_length;

        if (!send_usb_request_packet(device, req, desc, desc->header.b_length)) {
            Log::warning("Failed to read device string language descriptor");
            return false;
        }

        return true;
    }

    bool XhciDriver::get_configuration_descriptor(XhciDevice* device, UsbConfigurationDescriptor* desc) {
        XHCI_DEVICE_REQUEST_PACKET req{};
        req.b_request_type = 0x80;  // Device to Host, Standard, Device
        req.b_request = USB_GET_DESCRIPTOR_REQ;
        req.w_value = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_CONFIGURATION, 0);
        req.w_index = 0;
        req.w_length = sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER);

        if (!send_usb_request_packet(device, req, &desc->header, sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER))) {
            Log::error("Failed to read configuration descriptor header");
            return false;
        }

        if (desc->header.w_total_length < sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER)) {
            Log::error("Invalid configuration descriptor total length: %u", desc->header.w_total_length);
            return false;
        }

        desc->data_size = desc->header.w_total_length - sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER);

        if (desc->data) {
            delete[] desc->data;
            desc->data = nullptr;
        }

        if (desc->data_size > 0) {
            desc->data = new u8[desc->data_size];
            if (!desc->data) {
                Log::error("Failed to allocate memory for configuration descriptor (%zu bytes)", desc->data_size);
                return false;
            }
        }

        req.w_length = desc->header.w_total_length;

        auto* temp_buffer = new u8[desc->header.w_total_length];
        if (!temp_buffer) {
            Log::error("Failed to allocate temporary buffer for configuration descriptor");
            if (desc->data) {
                delete[] desc->data;
                desc->data = nullptr;
            }
            return false;
        }

        if (!send_usb_request_packet(device, req, temp_buffer, desc->header.w_total_length)) {
            Log::error("Failed to read complete configuration descriptor");
            delete[] temp_buffer;
            if (desc->data) {
                delete[] desc->data;
                desc->data = nullptr;
            }
            return false;
        }

        memcpy(&desc->header, temp_buffer, sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER));
        if (desc->data_size > 0) {
            memcpy(desc->data, temp_buffer + sizeof(USB_CONFIGURATION_DESCRIPTOR_HEADER), desc->data_size);
        }

        delete[] temp_buffer;

        return true;
    }

    bool XhciDriver::set_device_configuration(const XhciDevice* device, u16 configuration_value) {
        XHCI_DEVICE_REQUEST_PACKET setup_packet{};
        memset(&setup_packet, 0, sizeof(XHCI_DEVICE_REQUEST_PACKET));
        setup_packet.b_request_type = 0x00;  // Host to Device, Standard, Device
        setup_packet.b_request = USB_SET_CONFIGURATION_REQ;
        setup_packet.w_value = configuration_value;
        setup_packet.w_index = 0;
        setup_packet.w_length = 0;

        if (!send_usb_no_data_request_packet(device, setup_packet)) {
            Log::error("Failed to set device configuration");
            return false;
        }

        return true;
    }

    bool XhciDriver::get_hid_report_descriptor(
        XhciDevice* device, u8 interface_number, u8 descriptor_index, u8* report_buffer, u16 report_length
    ) {
        XHCI_DEVICE_REQUEST_PACKET req{};
        memset(&req, 0, sizeof(req));

        // bmRequestType: 0x81 = Device-to-Host, Standard, Interface
        req.b_request_type = 0x81;
        req.b_request = USB_GET_DESCRIPTOR_REQ;
        // wValue: high byte is the descriptor type (HID Report), low byte is descriptor index (0 for now)
        req.w_value = (USB_DESCRIPTOR_HID_REPORT << 8) | descriptor_index;
        req.w_index = interface_number;
        req.w_length = report_length;

        return send_usb_request_packet(device, req, report_buffer, report_length);
    }

    bool XhciDriver::configure_endpoint(const XhciDevice* device) {
        xhci_configure_endpoint_command_trb_t configure_ep_trb{};
        configure_ep_trb.trb_type = XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_CMD;
        configure_ep_trb.input_context_physical_base = device->get_input_context_phys();
        configure_ep_trb.slot_id = device->get_slot_id();

        xhci_command_completion_trb_t* completion_trb =
            send_command(reinterpret_cast<xhci_trb_t*>(&configure_ep_trb), 200);

        if (!completion_trb) {
            Log::error("Failed to send Configure Endpoint command");
            return false;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::error(
                "Configure Endpoint command failed with completion code: %s",
                trb_completion_code_to_string(completion_trb->completion_code)
            );
            return false;
        }

        return true;
    }

    bool XhciDriver::setup_device(u8 port) {
        u8 port_id = port + 1;
        u8 port_speed = get_port_speed(port);
        u16 max_packet_size = get_max_initial_packet_size(port_speed);

        u8 slot_id = assign_slot();
        if (!slot_id) {
            Log::error("Failed to enable device slot %u", slot_id);
            return false;
        }

        if (!create_device_context(slot_id)) {
            Log::error("Failed to create device context");
            return false;
        }

        auto* device = new XhciDevice(slot_id, port_id, port_speed, _64byte_context_size_);

        configure_control_ep_input_context(device, max_packet_size);

        if (!address_device_command(device, true)) {
            Log::error("Failed to setup device - failed to set device address");
            return false;
        }

        auto* device_descriptor = new USB_DEVICE_DESCRIPTOR();
        if (!get_device_descriptor(device, device_descriptor, 8)) {
            Log::error("Failed to get device descriptor");
            return false;
        }
        configure_control_ep_input_context(device, device_descriptor->b_max_packet_size0);

        if (device_descriptor->b_max_packet_size0 != max_packet_size) {
            if (!evaluate_context(device)) {
                return false;
            }
        }

        address_device_command(device, false);

        device->sync_input_ctx(reinterpret_cast<void*>(dcbaa_virtual_addresses_[device->get_slot_id()]));

        if (!get_device_descriptor(device, device_descriptor, device_descriptor->header.b_length)) {
            Log::error("Failed to get full device descriptor");
            return false;
        }

#if 0
        Log::info("USB Device Descriptor:");
        Log::info("  bcdUSB:            0x%04x", device_descriptor->bcd_usb);
        Log::info("  bDeviceClass:      0x%02x", device_descriptor->b_device_class);
        Log::info("  bDeviceSubClass:   0x%02x", device_descriptor->b_device_sub_class);
        Log::info("  bDeviceProtocol:   0x%02x", device_descriptor->b_device_protocol);
        Log::info("  bMaxPacketSize0:   0x%02x", device_descriptor->b_max_packet_size0);
        Log::info("  idVendor:          0x%04x", device_descriptor->id_vendor);
        Log::info("  idProduct:         0x%04x", device_descriptor->id_product);
        Log::info("  bcdDevice:         0x%04x", device_descriptor->bcd_device);
        Log::info("  iManufacturer:     0x%02x", device_descriptor->i_manufacturer);
        Log::info("  iProduct:          0x%02x", device_descriptor->i_product);
        Log::info("  iSerialNumber:     0x%02x", device_descriptor->i_serial_number);
        Log::info("  bNumConfigurations: 0x%02x", device_descriptor->b_num_configurations);
#endif

        USB_STRING_LANGUAGE_DESCRIPTOR string_language_descriptor{};
        if (!get_string_language_descriptor(device, &string_language_descriptor)) {
            return false;
        }
        u16 lang_id = string_language_descriptor.lang_ids[0];

        auto* product_name = new USB_STRING_DESCRIPTOR();
        if (!get_string_descriptor(device, device_descriptor->i_product, lang_id, product_name)) {
            return false;
        }

        auto* manufacturer_name = new USB_STRING_DESCRIPTOR();
        if (!get_string_descriptor(device, device_descriptor->i_manufacturer, lang_id, manufacturer_name)) {
            return false;
        }

        auto* serial_number_string = new USB_STRING_DESCRIPTOR();
        if (!get_string_descriptor(device, device_descriptor->i_serial_number, lang_id, serial_number_string)) {
            return false;
        }

        u16 product_str[126];
        u16 manufacturer_str[126];
        u16 serial_str[126];

        memcpy(product_str, product_name->unicode_string, sizeof(product_str));
        memcpy(manufacturer_str, manufacturer_name->unicode_string, sizeof(manufacturer_str));
        memcpy(serial_str, serial_number_string->unicode_string, sizeof(serial_str));

        auto* dev_info = new UsbDeviceInfo();

        utf16_to_utf8(product_str, sizeof(product_str), dev_info->model);
        utf16_to_utf8(manufacturer_str, sizeof(manufacturer_str), dev_info->vendor);
        utf16_to_utf8(serial_str, sizeof(serial_str), dev_info->serial);

        if (dev_info->model[0] == '?' && dev_info->vendor[0] == '?' &&
            dev_info->serial[0] == '?') {
            Log::log_msg("Unknown USB device, canceling setup...");
            return false;
        }

        device->set_model_name(dev_info->model);

        snprintf(dev_info->firmware, sizeof(dev_info->firmware), "%u.%02u",
                 device_descriptor->bcd_device >> 8,
                 device_descriptor->bcd_device & 0xFF);

        dev_info->usb_info.bus_number          = bus_number_;
        dev_info->usb_info.slot_id             = slot_id;
        dev_info->usb_info.port_num            = port_id;
        dev_info->usb_info.speed               = port_speed;
        dev_info->usb_info.vendor_id           = device_descriptor->id_vendor;
        dev_info->usb_info.product_id          = device_descriptor->id_product;
        dev_info->usb_info.bcd_device          = device_descriptor->bcd_device;
        dev_info->usb_info.bcd_usb             = device_descriptor->bcd_usb;
        dev_info->usb_info.b_device_class      = device_descriptor->b_device_class;
        dev_info->usb_info.b_device_subclass   = device_descriptor->b_device_sub_class;
        dev_info->usb_info.b_device_protocol   = device_descriptor->b_device_protocol;
        dev_info->usb_info.num_configurations  = device_descriptor->b_num_configurations;
        dev_info->usb_info.num_interfaces      = 0; // gets filled below after parsing the config descriptor


        auto* configuration_descriptor = new UsbConfigurationDescriptor();
        if (!get_configuration_descriptor(device, configuration_descriptor)) {
            return false;
        }

        device->sync_input_ctx(reinterpret_cast<void*>(dcbaa_virtual_addresses_[device->get_slot_id()]));

        if (!set_device_configuration(device, configuration_descriptor->header.b_configuration_value)) {
            return false;
        }

        u8* buffer = configuration_descriptor->data;
        u16 total_length =
            configuration_descriptor->header.w_total_length - configuration_descriptor->header.header.b_length;
        u16 index = 0;

        while (index < total_length) {
            auto* header = reinterpret_cast<USB_DESCRIPTOR_HEADER*>(&buffer[index]);

            switch (header->b_descriptor_type) {
                case USB_DESCRIPTOR_INTERFACE: {
                    auto* iface_desc = reinterpret_cast<USB_INTERFACE_DESCRIPTOR*>(header);
                    device->setup_add_interface(iface_desc);
                    dev_info->usb_info.num_interfaces++;
                    break;
                }
                case USB_DESCRIPTOR_HID: {
                    // Process HID Descriptor
                    auto* hid_desc = reinterpret_cast<USB_HID_DESCRIPTOR*>(header);

                    // Process subordinate descriptors
                    for (u8 i = 0; i < hid_desc->b_num_descriptors; i++) {
                        // Check if this subordinate descriptor is the HID Report Descriptor
                        if (hid_desc->desc[i].b_descriptor_type == USB_DESCRIPTOR_HID_REPORT) {
                            if (device->interfaces.empty()) {
                                Log::error("??? HID descriptor discovered before an interface!");
                                break;
                            }
                            const auto& current_interface = device->interfaces.back();

                            current_interface->additional_data_length = hid_desc->desc[i].w_descriptor_length;

                            // Allocate a buffer to hold the HID report descriptor.
                            current_interface->additional_data = new u8[current_interface->additional_data_length];

                            // Retrieve the HID report descriptor.

                            if (const i8 interface_number = device->interfaces.back()->descriptor.b_interface_number;
                                !get_hid_report_descriptor(
                                    device,
                                    interface_number,
                                    0,
                                    current_interface->additional_data,
                                    current_interface->additional_data_length
                                )) {
                                delete[] current_interface->additional_data;
                                current_interface->additional_data_length = 0;
                            }
                        }
                    }
                    break;
                }
                case USB_DESCRIPTOR_ENDPOINT: {
                    if (device->interfaces.empty()) {
                        Log::error("??? Endpoint descriptor discovered before an interface!");
                        break;
                    }
                    auto& current_interface = device->interfaces.back();

                    auto* ep_desc = reinterpret_cast<USB_ENDPOINT_DESCRIPTOR*>(header);
                    current_interface->setup_add_endpoint(ep_desc);
                    break;
                }
                default:
                    break;
            }

            index += header->b_length;
        }

        XHCI_INPUT_CONTROL_CONTEXT32* in_ctrl_ctx = device->get_input_control_ctx();
        in_ctrl_ctx->add_flags = (1 << 0);
        in_ctrl_ctx->drop_flags = 0;

        for (auto& iface : device->interfaces) {
            /*   Log::PrintLn("  ---- Interface %u ----", iface->descriptor.bInterfaceNumber);
               Log::PrintLn("  class    : %u", iface->descriptor.bInterfaceClass);
               Log::PrintLn("  subclass : %u", iface->descriptor.bInterfaceSubClass);
               Log::PrintLn("  protocol : %u", iface->descriptor.bInterfaceProtocol);*/

            for (auto& ep : iface->endpoints) {
                /*    Log::PrintLn("    -- Endpoint %u --", ep->xhc_endpoint_num);
                    Log::PrintLn("    type            : %u", ep->xhc_endpoint_type);
                    Log::PrintLn("    address         : 0x%x", ep->usb_endpoint_addr);
                    Log::PrintLn("    max_packet_size : %u", ep->max_packet_size);
                    Log::PrintLn("    interval        : %u", ep->interval);
                    Log::PrintLn("    attribs         : %u", ep->usb_endpoint_attributes);*/

                configure_ep_input_context(device, ep);
            }

            if (iface->descriptor.b_interface_class == 0x03 && iface->descriptor.b_interface_sub_class == 0x01) {
                // Mouse
                if (iface->descriptor.b_interface_protocol == 0x02) {
                    //   iface->driver = new xhciMouseDriver();
                    //   iface->driver->attach_interface(iface);
                }

                // Keyboard
                if (iface->descriptor.b_interface_protocol == 0x01) {
                    iface->driver = new XhciKeyboardDriver();
                    iface->driver->attach_interface(iface);
                }
            } else if (iface->descriptor.b_interface_class == 0x08) {
                // Mass Storage Class
                if (iface->descriptor.b_interface_protocol == 0x50) {
                    iface->driver = new XhciMassStorageDriver();
                    iface->driver->attach_interface(iface);
                }
            }

            if (iface->driver) {
                iface->driver->set_device_info(dev_info);
            }
        }

        if (!configure_endpoint(device)) {
            return false;
        }

        device->sync_input_ctx(reinterpret_cast<void*>(dcbaa_virtual_addresses_[device->get_slot_id()]));

        m_connected_devices.push_back(device);
        Log::print_ln("Device setup complete");

        SYS_EVENT_DEVICE_REGISTERED(device->get_model_name(), port_id);

        if (device->interfaces[0]->driver) {
            device->interfaces[0]->driver->on_startup(this, device);
        }

        return true;
    }

    bool XhciDriver::address_device_command(const XhciDevice* dev, bool bsr) {
        u64 input_ctx_phys = dev->get_input_context_phys();

        xhci_address_device_command_trb_t address_device_trb{};

        address_device_trb.trb_type = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
        address_device_trb.input_context_physical_base = input_ctx_phys;
        address_device_trb.bsr = bsr ? 1 : 0;
        address_device_trb.slot_id = dev->get_slot_id();
        address_device_trb.cycle_bit = command_ring_->get_cycle_bit();

        xhci_command_completion_trb_t* completion_trb =
            send_command(reinterpret_cast<xhci_trb_t*>(&address_device_trb), 200);

        if (!completion_trb) {
            Log::error("[xhci] Failed to address device with BSR=%u", static_cast<int>(bsr));
            return false;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::error("[xhci] Address Device returned code 0x%02x", completion_trb->completion_code);
            return false;
        }

        return true;
    }

    u8 XhciDriver::assign_slot() {
        xhci_trb_t trb{};
        trb.parameter = 0;
        trb.status = 0;

        trb.cycle_bit = command_ring_->get_cycle_bit();
        trb.eval_next_trb = 0;
        trb.interrupt_on_completion = 1;
        trb.chain_bit = 0;
        trb.immediate_data = 0;
        trb.block_event_interrupt = 0;
        trb.trb_type = XHCI_TRB_TYPE_ENABLE_SLOT_CMD;

        xhci_command_completion_trb_t* cce = send_command(&trb, 1000);

        if (!cce) {
            Log::error("[xhci] assign_slot enable slot failed (no completion TRB)");
            return 0;
        }
        if (cce->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::error("[xhci] enable slot returned code 0x%02x", cce->completion_code);
            return 0;
        }

        return cce->slot_id & 0xFF;
    }

    bool XhciDriver::get_vendor(char* out, usize len) {
        strncpy(out, pci::get_vendor_name(pci_hdr_->header.vendor_id), len);
        out[len - 1] = '\0';
        return true;
    }
    bool XhciDriver::get_model(char* out, usize len) {
        strncpy(out, pci::get_device_name(pci_hdr_->header.vendor_id, pci_hdr_->header.device_id), len);
        out[len - 1] = '\0';
        return true;
    }
}  // namespace usb
