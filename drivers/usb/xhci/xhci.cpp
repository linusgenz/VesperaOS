#include "xhci.h"
#include "xhci_device.h"
#include "xhci_ext_cap.h"
#include "../../../include/log.h"
#include "../../../include/vector.h"
#include "../../../kernel/time/time.h"
#include "../../pci/pci.h"
#include "../../../kernel/include/interrupts.h"
#include "../usb_descriptors.h"
#include "../../include/encoding.h"
#include "xhci_usb_interface.h"
#include "../../../arch/x86_64/interrupts/apic.h"

namespace USB {
    xhciDriver::xhciDriver() = default;

    bool xhciDriver::init_device(PCI::PCIDeviceHeader *pci_base_address) {
        flag = false;
        auto *pci_hdr = reinterpret_cast<PCI::PCIHeader0 *>(pci_base_address);
        uint64_t bar0 = pci_hdr->BAR0 & ~0xF;
        uint64_t bar1 = pci_hdr->BAR1;
        uint64_t bar = ((bar1 << 32) | bar0);

        uint32_t original_bar0 = pci_hdr->BAR0;
        uint32_t original_bar1 = pci_hdr->BAR1;

        pci_hdr->BAR0 = 0xFFFFFFFF;
        pci_hdr->BAR1 = 0xFFFFFFFF;

        uint32_t size_mask_lo = pci_hdr->BAR0;
        uint32_t size_mask_hi = pci_hdr->BAR1;

        pci_hdr->BAR0 = original_bar0;
        pci_hdr->BAR1 = original_bar1;

        uint64_t mask = ((uint64_t) size_mask_hi << 32) | (size_mask_lo & ~0xF);
        if (mask == 0) {
            return false;
        }

        uint64_t bar_size = ~(mask) + 1;

        m_xhc_base = xhci_map_mmio(bar, bar_size);

        // Read capability registers
        parse_capability_registers();
        //    log_capability_registers();
        parse_extended_capabilities();

        // Reset the host controller
        if (!reset_host_controller()) {
            return false;
        }

        // Setup operational registers
        configure_operational_registers();
        //    log_operational_registers();

        kernel::interrupts::register_irq(IRQ_XHCI_VECTOR, reinterpret_cast<irq_handler_t>(xhci_irq_handler), this);


        // Setup runtime registers
        configure_runtime_registers();

        Log::Info("XHCI initialized");
        return true;
    }

    bool xhciDriver::start_device() {
        if (!start_host_controller()) {
            Log::PrintLn("Failed to start the host controller");
            return false;
        }

        for (uint8_t i = 0; i < m_max_ports; i++) {
            xhci_port_register_manager regman = get_port_register_set(i);
            xhci_portsc_register portsc{};
            regman.read_portsc_reg(portsc);

            if (portsc.csc && portsc.ccs) {
                xhci_port_connection_event conn_evt{};
                conn_evt.port_id = i + 1;
                conn_evt.device_connected = (portsc.ccs == 1);
                if (portsc.ccs == 1) {
                    Log::Info("Device connected - %s", usb_speed_to_string(portsc.port_speed));

                }
                m_port_connection_events.push_back(conn_evt);
            }
        }

        Log::PrintLn("Controller started! Max ports: %u", m_max_ports);

        while (true) {
            kernel::time::sleep_ms(100);

            if (m_port_connection_events.empty()) {
                //   continue;
                break;
            }

            for (size_t i = 0; i < m_port_connection_events.size(); i++) {
              //  Log::Print(". %u", i);
                auto event = m_port_connection_events[i];
                uint8_t port = event.port_id;
                uint8_t port_reg_idx = port - 1;

                xhci_port_register_manager regman = get_port_register_set(port_reg_idx);
                xhci_portsc_register portsc{};
                regman.read_portsc_reg(portsc);

                if (event.device_connected) {
                    Log::Print("Device connected");
                    bool reset_successful = reset_port(port_reg_idx);
                    Log::debug("after reset");
                    kernel::time::sleep_ms(100);

                    if (reset_successful) {
                        Log::Info("Device connected on port %u - %s", port, usb_speed_to_string(portsc.port_speed));
                     //   setup_device(port_reg_idx, portsc);
                    } else {
                        Log::Warning("Failed to reset port %u after connection detection", port);
                    }
                } else {
                    Log::Info("Device disconnected from port %u", port);
                    reset_port(port_reg_idx);
                }
            }

            m_port_connection_events.clear();
        }

        return true;
    }

    bool xhciDriver::shutdown_device() {
        return true;
    }

    xhci_port_register_manager xhciDriver::get_port_register_set(uint8_t port_num) {
        uint64_t base = reinterpret_cast<uint64_t>(m_op_regs) + (0x400 + (0x10 * port_num));
        return {base};
    }

    void xhciDriver::parse_capability_registers() {
        m_cap_regs = reinterpret_cast<volatile xhci_capability_registers *>(m_xhc_base);

        m_capability_regs_length = m_cap_regs->caplength;

        m_max_device_slots = XHCI_MAX_DEVICE_SLOTS(m_cap_regs);
        m_max_interrupters = XHCI_MAX_INTERRUPTERS(m_cap_regs);
        m_max_ports = XHCI_MAX_PORTS(m_cap_regs);

        m_isochronous_scheduling_threshold = XHCI_IST(m_cap_regs);
        m_erst_max = XHCI_ERST_MAX(m_cap_regs);
        m_max_scratchpad_buffers = XHCI_MAX_SCRATCHPAD_BUFFERS(m_cap_regs);

        m_64bit_addressing_capability = XHCI_AC64(m_cap_regs);
        m_bandwidth_negotiation_capability = XHCI_BNC(m_cap_regs);
        m_64byte_context_size = XHCI_CSZ(m_cap_regs);
        m_port_power_control = XHCI_PPC(m_cap_regs);
        m_port_indicators = XHCI_PIND(m_cap_regs);
        m_light_reset_capability = XHCI_LHRC(m_cap_regs);
        m_extended_capabilities_offset = XHCI_XECP(m_cap_regs) * sizeof(uint32_t);

        // Update the base pointer to operational register set
        m_op_regs = reinterpret_cast<volatile xhci_operational_registers *>(m_xhc_base + m_capability_regs_length);

        // Update the base pointer to the runtime register set
        m_runtime_regs = reinterpret_cast<volatile xhci_runtime_registers *>(m_xhc_base + m_cap_regs->rtsoff);

        // Construct a manager class instance for the doorbell register array
        m_doorbell_manager = new xhci_doorbell_manager(m_xhc_base + m_cap_regs->dboff);
    }

    void xhciDriver::parse_extended_capabilities() {
        volatile auto *head_cap_ptr = reinterpret_cast<volatile uint32_t *>(
            m_xhc_base + m_extended_capabilities_offset);

        extended_capabilities_head = new xhci_extended_capability(head_cap_ptr);

        auto node = extended_capabilities_head;
        while (node) {
            if (node->id() == xhci_extended_capability_code::support_protocol) {
                xhci_usb_supported_protocol_capability cap(node->base());

                uint8_t first_port = cap.compatible_port_offset - 1;
                uint8_t last_port = cap.compatible_port_offset - 1;

                if (cap.major_revision_version == 3) {
                    for (uint8_t port = first_port; port <= last_port; port++) {
                        m_usb3_ports.push_back(port);
                    }
                }
            }

            if (node->id() == xhci_extended_capability_code::usb_legacy_support) {
                xhci_legacy_support_capability legacy(node->base());
                claim_legacy_ownership(&legacy);
            }

            if (node->next() == nullptr) break;
            node = node->next();
        }
    }

    void xhciDriver::log_capability_registers() {
        Log::PrintLn("===== Xhci Capability Registers (0x%llx) =====", (uint64_t) m_cap_regs);
        Log::PrintLn("    Length                : %u", m_capability_regs_length);
        Log::PrintLn("    Max Device Slots      : %u", m_max_device_slots);
        Log::PrintLn("    Max Interrupters      : %u", m_max_interrupters);
        Log::PrintLn("    Max Ports             : %u", m_max_ports);
        Log::PrintLn("    IST                   : %u", m_isochronous_scheduling_threshold);
        Log::PrintLn("    ERST Max Size         : %u", m_erst_max);
        Log::PrintLn("    Scratchpad Buffers    : %u", m_max_scratchpad_buffers);
        Log::PrintLn("    64-bit Addressing     : %s", m_64bit_addressing_capability ? "yes" : "no");
        Log::PrintLn("    Bandwidth Negotiation : %u", m_bandwidth_negotiation_capability);
        Log::PrintLn("    64-byte Context Size  : %s", m_64byte_context_size ? "yes" : "no");
        Log::PrintLn("    Port Power Control    : %u", m_port_power_control);
        Log::PrintLn("    Port Indicators       : %u", m_port_indicators);
        Log::PrintLn("    Light Reset Available : %u", m_light_reset_capability);
        Log::PrintLn("");
    }

    void xhciDriver::log_operational_registers() {
        Log::PrintLn("===== Xhci Operational Registers (0x%llx) =====", (uint64_t) m_op_regs);
        Log::PrintLn("    usbcmd     : 0x%x", m_op_regs->usbcmd);
        Log::PrintLn("    usbsts     : 0x%x", m_op_regs->usbsts);
        Log::PrintLn("    pagesize   : 0x%x", m_op_regs->pagesize);
        Log::PrintLn("    dnctrl     : 0x%x", m_op_regs->dnctrl);
        Log::PrintLn("    crcr       : 0x%llx", m_op_regs->crcr);
        Log::PrintLn("    dcbaap     : 0x%llx", m_op_regs->dcbaap);
        Log::PrintLn("    config     : 0x%x", m_op_regs->config);
        Log::PrintLn("");
    }

    void xhciDriver::log_usbsts() const {
        uint32_t status = m_op_regs->usbsts;
        Log::PrintLn("===== USBSTS =====");
        if (status & XHCI_USBSTS_HCH) Log::PrintLn("    Host Controlled Halted");
        if (status & XHCI_USBSTS_HSE) Log::PrintLn("    Host System Error");
        if (status & XHCI_USBSTS_EINT) Log::PrintLn("    Event Interrupt");
        if (status & XHCI_USBSTS_PCD) Log::PrintLn("    Port Change Detect");
        if (status & XHCI_USBSTS_SSS) Log::PrintLn("    Save State Status");
        if (status & XHCI_USBSTS_RSS) Log::PrintLn("    Restore State Status");
        if (status & XHCI_USBSTS_SRE) Log::PrintLn("    Save/Restore Error");
        if (status & XHCI_USBSTS_CNR) Log::PrintLn("    Controller Not Ready");
        if (status & XHCI_USBSTS_HCE) Log::PrintLn("    Host Controller Error");
    }

    void xhciDriver::claim_legacy_ownership(xhci_legacy_support_capability *legacy) {
        // Set OS_OWNED bit (bit 24)
        legacy->usblegsup.os_owned = 1;
        // Wait for BIOS to clear BIOS_OWNED (bit 16)
        constexpr int max_wait_ms = 100;
        int waited = 0;

        while (legacy->usblegsup.bios_owned == 1) {
            kernel::time::sleep_ms(10);
            if (++waited >= max_wait_ms) {
                Log::Error("BIOS did not release xHCI ownership after %d ms", waited);
                break;
            }
        }

        // Clear SMI bits: bits 29–31 (RW1C)
        legacy->usblegctlsts.raw = (1 << 29) | (1 << 30) | (1 << 31);
    }

    bool xhciDriver::is_usb3_port(uint8_t port_num) {
        for (size_t i = 0; i < m_usb3_ports.size(); i++) {
            if (m_usb3_ports[i] == port_num) {
                return true;
            }
        }
        return false;
    }

    bool xhciDriver::reset_host_controller() const {
        // Make sure we clear the Run/Stop bit
        uint32_t usbcmd = m_op_regs->usbcmd;
        usbcmd &= ~XHCI_USBCMD_RUN_STOP;
        m_op_regs->usbcmd = usbcmd;

        // Wait for the HCHalted bit to be set
        uint32_t timeout = 20;
        while (!(m_op_regs->usbsts & XHCI_USBSTS_HCH)) {
            if (--timeout == 0) {
                Log::PrintLn("Host controller did not halt within %ums", 200);
                return false;
            }

            kernel::time::sleep_ms(10);
        }

        // Set the HC Reset bit
        usbcmd = m_op_regs->usbcmd;
        usbcmd |= XHCI_USBCMD_HCRESET;
        m_op_regs->usbcmd = usbcmd;

        // Wait for this bit and CNR bit to clear
        timeout = 100;
        while (
            m_op_regs->usbcmd & XHCI_USBCMD_HCRESET ||
            m_op_regs->usbsts & XHCI_USBSTS_CNR
        ) {
            if (--timeout == 0) {
                Log::PrintLn("Host controller did not reset within %ums", 1000);
                return false;
            }

            kernel::time::sleep_ms(10);
        }

        kernel::time::sleep_ms(50);

        // Check the defaults of the operational registers
        if (m_op_regs->usbcmd != 0)
            return false;

        if (m_op_regs->dnctrl != 0)
            return false;

        if (m_op_regs->crcr != 0)
            return false;

        if (m_op_regs->dcbaap != 0)
            return false;

        if (m_op_regs->config != 0)
            return false;

        return true;
    }

    void xhciDriver::configure_operational_registers() {
        // Enable device notifications
        m_op_regs->dnctrl = 0xffff;

        // Configure the usbconfig field
        m_op_regs->config = static_cast<uint32_t>(m_max_device_slots);

        // Setup device context base address array and scratchpad buffers
        setup_dcbaa();

        // Set up the command ring and write CRCR
        m_command_ring = new xhciCommandRing(XHCI_COMMAND_RING_TRB_COUNT);

        Log::Info("CRCR: 0x%llx", m_op_regs->crcr);
        Log::Info("Command Ring Running (CRR): %s", (m_op_regs->crcr & (1 << 1)) ? "yes" : "no");
        kernel::time::sleep_ms(100);
        m_op_regs->crcr = m_command_ring->get_physical_base() | m_command_ring->get_cycle_bit(); //   0xC039181;
        Log::PrintLn("xhciDriver::configure_operational_registers after: %p", m_op_regs->crcr);
    }

    void xhciDriver::setup_dcbaa() {
        size_t dcbaa_size = sizeof(uintptr_t) * (m_max_device_slots + 1);

        m_dcbaa = reinterpret_cast<uint64_t *>(
            alloc_xhci_memory(dcbaa_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY)
        );

        m_dcbaa_virtual_addresses = new uint64_t[m_max_device_slots + 1];

        if (m_max_scratchpad_buffers > 0) {
            auto *scratchpad_array = reinterpret_cast<uint64_t *>(
                alloc_xhci_memory(
                    m_max_scratchpad_buffers * sizeof(uint64_t),
                    XHCI_DEVICE_CONTEXT_ALIGNMENT,
                    XHCI_DEVICE_CONTEXT_BOUNDARY
                )
            );

            // Create scratchpad pages
            for (uint8_t i = 0; i < m_max_scratchpad_buffers; i++) {
                void *scratchpad = alloc_xhci_memory(
                    PAGE_SIZE,
                    XHCI_SCRATCHPAD_BUFFERS_ALIGNMENT,
                    XHCI_SCRATCHPAD_BUFFERS_BOUNDARY
                );

                uint64_t scratchpad_paddr = xhci_get_physical_addr(scratchpad);
                scratchpad_array[i] = scratchpad_paddr;
            }

            uint64_t scratchpad_array_physical_base = xhci_get_physical_addr(scratchpad_array);

            // Set the first slot in the DCBAA to point to the scratchpad array
            m_dcbaa[0] = scratchpad_array_physical_base;

            m_dcbaa_virtual_addresses[0] = reinterpret_cast<uint64_t>(scratchpad_array);
        }

        // Set DCBAA pointer in the operational registers
        m_op_regs->dcbaap = xhci_get_physical_addr(m_dcbaa);
    }

    void xhciDriver::acknowledge_irq(uint8_t interrupter) const {
        // Get the interrupter registers
        volatile xhci_interrupter_registers *interrupter_regs = &m_runtime_regs->ir[interrupter];

        // Read the current value of IMAN
        uint32_t iman = interrupter_regs->iman;

        // Set the IP bit to '1' to clear it, preserve other bits including IE
        iman |= XHCI_IMAN_INTERRUPT_PENDING;

        // Write back to IMAN
        interrupter_regs->iman = iman;

        // Clear the EINT bit in USBSTS by writing '1' to it
        m_op_regs->usbsts = XHCI_USBSTS_EINT;
    }

    xhci_command_completion_trb_t *xhciDriver::_send_command(xhci_trb_t *trb, uint32_t timeout_ms) {
        if (!m_command_ring->enqueue(trb)) {
            Log::Warning("Failed to enqueue command. Command ring is full.");
            return nullptr;
        };


        // Ring the command doorbell
        m_doorbell_manager->ring_command_doorbell();

        // Let the host controller process the command
        uint64_t sleep_passed = 0;
        while (!m_command_irq_completed) {
            kernel::time::sleep_ms(10);
            sleep_passed += 10;

            if (sleep_passed > timeout_ms) {
                break;
            }
        }

        xhci_command_completion_trb_t *completion_trb =
                !m_command_completion_events.empty() ? m_command_completion_events[0] : nullptr;

        // Reset the irq flag and clear out the command completion event queue
        m_command_completion_events.clear();
        m_command_irq_completed = 0;

        if (!completion_trb) {
            Log::Warning("Failed to find completion TRB for command %u", trb->trb_type);
            return nullptr;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::Warning("Command TRB failed with error: %s",
                         trb_completion_code_to_string(completion_trb->completion_code));
            return nullptr;
        }

        // Update the command ring dequeue pointer
        m_command_ring->process_event(completion_trb);

        return completion_trb;
    }

    void xhciDriver::configure_runtime_registers() {
        volatile xhci_interrupter_registers *interrupter_regs = &m_runtime_regs->ir[0];

        // WICHTIG: Erst alle Interrupts disablen
        interrupter_regs->iman = 0;

        // Clear any pending interrupts
        m_op_regs->usbsts = XHCI_USBSTS_EINT; // Clear EINT bit

        m_event_ring = new xhciEventRing(XHCI_EVENT_RING_TRB_COUNT, interrupter_regs);

        uint64_t erdp = m_event_ring->get_physical_base();
        interrupter_regs->erdp = erdp;

        interrupter_regs->imod = 0;
        // Clear IP bit before enabling
        interrupter_regs->iman = XHCI_IMAN_INTERRUPT_PENDING | XHCI_IMAN_INTERRUPT_ENABLE;
    }

    bool xhciDriver::start_host_controller() const {
        m_op_regs->usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;

        m_op_regs->usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;
        asm volatile ("" ::: "memory");

        m_op_regs->usbcmd |= XHCI_USBCMD_RUN_STOP;

        // Wait for controller to start
        constexpr int max_retries = 100;
        int retries = 0;
        while (m_op_regs->usbsts & XHCI_USBSTS_HCH) {
            if (retries++ >= max_retries) {
                Log::Error("Controller failed to start within timeout");
                return false;
            }
            kernel::time::sleep_ms(10);
        }

        // Verify CNR bit is clear
        if (m_op_regs->usbsts & XHCI_USBSTS_CNR) {
            Log::Error("Controller Not Ready after start");
            return false;
        }

        return true;
    }

    irqreturn_t xhciDriver::xhci_irq_handler(xhciDriver *driver) {
        driver->process_events();
        driver->acknowledge_irq(0);
        return IRQ_HANDLED;
    }

    void xhciDriver::process_events() {
        Vector<xhci_trb_t *> events;
        if (m_event_ring->has_unprocessed_events()) {
            m_event_ring->dequeue_events(events);
        }

        uint8_t port_change_event_status = 0;
        uint8_t command_completion_status = 0;
        uint8_t transfer_completion_status = 0;

        for (size_t i = 0; i < events.size(); i++) {
            xhci_trb_t* event = events[i];
            switch (event->trb_type) {
                case XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT: {
                    port_change_event_status = 1;
                    auto port_evt = reinterpret_cast<xhci_port_status_change_trb_t*>(event);
                    m_port_status_change_events.push_back(port_evt);

                    xhci_port_register_manager regman = get_port_register_set(port_evt->port_id - 1);
                    xhci_portsc_register portsc;
                    regman.read_portsc_reg(portsc);

                    if (portsc.csc) {
                        xhci_port_connection_event conn_evt;
                        conn_evt.port_id = port_evt->port_id;
                        conn_evt.device_connected = (portsc.ccs == 1);
                        m_port_connection_events.push_back(conn_evt);
                    }
                    break;
                }
                case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT: {
                    command_completion_status = 1;
                    m_command_completion_events.push_back((xhci_command_completion_trb_t*)event);
                    break;
                }
                case XHCI_TRB_TYPE_TRANSFER_EVENT: {
                    transfer_completion_status = 1;
                    auto transfer_event = (xhci_transfer_completion_trb_t*)event;
                    m_transfer_completion_events.push_back(transfer_event);

                    auto device = m_connected_devices[transfer_event->slot_id];
                    if (!device) {
                        break;
                    }

                    if (device->interfaces.empty() || !device->interfaces[0]) {
                        Log::Warning("  -> Device has no primary interface or interface[0] is null!");
                        break;
                    }

                    auto& primary_interface = device->interfaces[0];
                    if (primary_interface->driver) {
                        primary_interface->driver->on_event(this, device);
                    }
                    break;
                }
                default: break;
            }
        }


        m_command_irq_completed = command_completion_status;
        m_transfer_irq_completed = transfer_completion_status;
    }


    bool xhciDriver::reset_port(uint8_t port_num) {
    xhci_port_register_manager regset = get_port_register_set(port_num);
    xhci_portsc_register portsc{};
    regset.read_portsc_reg(portsc);

    bool is_usb3 = is_usb3_port(port_num);

    // Power on the port if necessary
    if (portsc.pp == 0) {
        portsc.pp = 1;
        regset.write_portsc_reg(portsc);
        kernel::time::sleep_ms(20); // Wait for power stabilization
        regset.read_portsc_reg(portsc);

        if (portsc.pp == 0) {
            Log::Warning("Port %i: Failed to power on port\n", port_num);
            return false;
        }
    }

    portsc.csc = 1; // Clear connect status change
    portsc.pec = 1; // Clear port enable/disable change
    portsc.prc = 1; // Clear port reset change
    regset.write_portsc_reg(portsc);

    // Initiate the port reset
    if (is_usb3) {
        portsc.wpr = 1; // Warm reset for USB 3.0
    } else {
        portsc.pr = 1; // Standard port reset for USB 2.0
    }
    regset.write_portsc_reg(portsc);

    // Wait for the reset to complete
    int timeout = 10;
    while (timeout > 0) {
        regset.read_portsc_reg(portsc);

        if ((is_usb3 && portsc.wrc) || (!is_usb3 && portsc.prc)) {
            break;
        }

        timeout--;
        kernel::time::sleep_ms(10);
        global_renderer->print("bottom while ");
    }

    if (timeout == 0) {
        Log::Warning("Port %i: Port reset timed out", port_num);
        return false;
    }

        kernel::time::sleep_ms(10);

    // Clear the reset completion and status change bits
    portsc.prc = 1; // Clear port reset change
    portsc.wrc = 1; // Clear warm reset change (USB 3.0)
    portsc.csc = 1; // Clear connect status change
    portsc.pec = 1; // Clear port enable/disable change
    portsc.ped = 0; // Don't clear the PED bit
    regset.write_portsc_reg(portsc);

    kernel::time::sleep_ms(10);

    // Re-read the register to check if the port is enabled
    regset.read_portsc_reg(portsc);

    // This case could happen when the port has been reset after
    // a device disconnect event, and no device has connected since.
    if (portsc.ped == 0) {
        return false;
    }

    return true;
}


    const char *xhciDriver::usb_speed_to_string(uint8_t speed) {
        static const char *speed_string[7] = {
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
/*
    bool xhciDriver::send_usb_request_packet(xhciDevice *device, xhci_device_request_packet &req, void *output_buffer,
                                             uint32_t length) {
        xhciTransferRing *transfer_ring = device->get_control_transfer_ring();

        auto *transfer_status_buffer = reinterpret_cast<uint32_t *>(alloc_xhci_memory(sizeof(uint32_t), 16, 16));
        auto *descriptor_buffer = reinterpret_cast<uint8_t *>(alloc_xhci_memory(256, 256, 256));

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

        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&setup_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&data_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&event_data_first));

        // QEMU doesn't quite handle SETUP/DATA/STATUS transactions correctly.
        // It will wait for the STATUS TRB before it completes the transfer.
        // Technically, you need to check for a good transfer before you send the
        //  STATUS TRB.  However, since QEMU doesn't update the status until after
        //  the STATUS TRB, waiting here will not complete a successful transfer.
        //  Bochs and real hardware handles this correctly, however QEMU does not.
        // If you are using QEMU, do not ring the doorbell here.  Ring the doorbell
        //  *after* you place the STATUS TRB on the ring.
        // (See bug report: https://bugs.launchpad.net/qemu/+bug/1859378 )
        bool in_qemu = true;
        if (!in_qemu) {
            auto completion_trb = start_control_endpoint_transfer(transfer_ring);
            if (!completion_trb) {
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

        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&status_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&event_data_second));

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

    bool xhciDriver::send_usb_no_data_request_packet(xhciDevice *dev, xhci_device_request_packet &req) {
        xhciTransferRing *transfer_ring = dev->get_control_transfer_ring();
        if (!transfer_ring) {
            Log::Error("No control transfer ring allocated.");
            return false;
        }

        xhci_setup_stage_trb_t setup_stage{};
        memset(&setup_stage, 0, sizeof(setup_stage));
        setup_stage.trb_type = XHCI_TRB_TYPE_SETUP_STAGE;

        setup_stage.request_packet = req;

        // TRT=0 => no data stage
        // If (bmRequestType & 0x80) and wLength>0 => TRT=3 (IN data)
        // If (!(bmRequestType & 0x80)) and wLength>0 => TRT=2 (OUT data)
        setup_stage.trt = 0; // No data stage
        setup_stage.idt = 1; // Immediate Data
        setup_stage.ioc = 0; // We'll complete on the Status Stage or Event Data
        setup_stage.trb_transfer_length = 8; // Setup packet length is always 8

        xhci_status_stage_trb_t status_stage{};
        memset(&status_stage, 0, sizeof(status_stage));
        status_stage.trb_type = XHCI_TRB_TYPE_STATUS_STAGE;

        // For a host->device (or no-data) control transfer, the status stage is an IN handshake => dir=1
        status_stage.dir = 1; // 1 = IN handshake
        status_stage.chain = 0; // or 1 if you want to chain to an Event Data TRB
        status_stage.ioc = 1; // Interrupt on completion

        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&setup_stage));
        transfer_ring->enqueue(reinterpret_cast<xhci_trb_t *>(&status_stage));

        auto completion_trb = start_control_endpoint_transfer(transfer_ring);
        if (!completion_trb) {
            Log::Error("No-Data request: Timed out or failed.");
            return false;
        }

        return true;
    }

    xhci_transfer_completion_trb_t *xhciDriver::start_control_endpoint_transfer(const xhciTransferRing *transfer_ring) {
        m_doorbell_manager->ring_control_endpoint_doorbell(transfer_ring->get_doorbell_id());

        constexpr uint64_t timeout_ms = 400;
        uint64_t sleep_passed = 0;

        while (!m_transfer_irq_completed) {
            kernel::time::sleep_ms(10);
            sleep_passed += 10;

            if (sleep_passed > timeout_ms) {
                break;
            }
        }

        xhci_transfer_completion_trb_t *completion_trb =
                !m_transfer_completion_events.empty() ? m_transfer_completion_events[0] : nullptr;

        // Reset the irq flag and clear out the command completion event queue
        m_transfer_completion_events.clear();
        m_transfer_irq_completed = 0;

        if (!completion_trb) {
            Log::Warning("Failed to find transfer completion TRB");
            return nullptr;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::Warning("Transfer TRB failed with error: %s",
                         trb_completion_code_to_string(completion_trb->completion_code));
            return nullptr;
        }

        return completion_trb;
    }

    bool xhciDriver::get_device_descriptor(xhciDevice *device, usb_device_descriptor *desc, uint32_t length) {
        xhci_device_request_packet req{};
        req.bRequestType = 0x80; // Device to Host, Standard, Device
        req.bRequest = USB_GET_DESCRIPTOR_REQ;
        req.wValue = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_DEVICE, 0);
        req.wIndex = 0;
        req.wLength = length;

        return send_usb_request_packet(device, req, desc, length);
    }

    bool xhciDriver::get_string_language_descriptor(xhciDevice *device, usb_string_language_descriptor *desc) {
        xhci_device_request_packet req{};
        req.bRequestType = 0x80;
        req.bRequest = USB_GET_DESCRIPTOR_REQ;
        req.wValue = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_STRING, 0);
        req.wIndex = 0;
        req.wLength = sizeof(usb_descriptor_header);

        if (!send_usb_request_packet(device, req, desc, sizeof(usb_descriptor_header))) {
            Log::Warning("Failed to read device string language descriptor header");
            return false;
        }

        req.wLength = desc->header.bLength;

        if (!send_usb_request_packet(device, req, desc, desc->header.bLength)) {
            Log::Warning("Failed to read device string language descriptor");
            return false;
        }

        return true;
    }

    bool xhciDriver::get_string_descriptor(xhciDevice *device, uint8_t descriptor_index, uint8_t langid,
                                           usb_string_descriptor *desc) {
        xhci_device_request_packet req{};
        req.bRequestType = 0x80; // Device to Host, Standard, Device
        req.bRequest = USB_GET_DESCRIPTOR_REQ;
        req.wValue = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_STRING, descriptor_index);
        req.wIndex = langid;
        req.wLength = sizeof(usb_descriptor_header);

        if (!send_usb_request_packet(device, req, desc, sizeof(usb_descriptor_header))) {
            Log::Warning("Failed to read device string descriptor header");
            return false;
        }

        // Read the entire desc
        req.wLength = desc->header.bLength;

        if (!send_usb_request_packet(device, req, desc, desc->header.bLength)) {
            Log::Warning("Failed to read device string descriptor");
            return false;
        }

        return true;
    }

    bool xhciDriver::get_configuration_descriptor(xhciDevice *device, usb_configuration_descriptor *desc) {
        xhci_device_request_packet req{};
        req.bRequestType = 0x80; // Device to Host, Standard, Device
        req.bRequest = USB_GET_DESCRIPTOR_REQ;
        req.wValue = USB_DESCRIPTOR_REQUEST(USB_DESCRIPTOR_CONFIGURATION, 0);
        req.wIndex = 0;
        req.wLength = sizeof(usb_descriptor_header);

        if (!send_usb_request_packet(device, req, desc, sizeof(usb_descriptor_header))) {
            Log::Error("Failed to read device configuration descriptor header");
            return false;
        }

        req.wLength = desc->header.bLength;

        if (!send_usb_request_packet(device, req, desc, desc->header.bLength)) {
            Log::Error("Failed to read device configuration descriptor");
            return false;
        }

        // Check if the descriptor is larger than the currently supported size (254)
        if (desc->wTotalLength > sizeof(usb_configuration_descriptor) - 1) {
            Log::Error("Configuration descriptor is larger than the currently supported size: %u > %u",
                       desc->wTotalLength, sizeof(usb_configuration_descriptor));
            return false;
        }

        req.wLength = desc->wTotalLength;

        if (!send_usb_request_packet(device, req, desc, desc->wTotalLength)) {
            Log::Error("Failed to read device configuration descriptor with interface descriptors");
            return false;
        }

        return true;
    }

    bool xhciDriver::get_hid_report_descriptor(
        xhciDevice *device,
        uint8_t interface_number,
        uint8_t descriptor_index,
        uint8_t *report_buffer,
        uint16_t report_length
    ) {
        xhci_device_request_packet req{};
        memset(&req, 0, sizeof(req));

        // bmRequestType: 0x81 = Device-to-Host, Standard, Interface
        req.bRequestType = 0x81;
        req.bRequest = USB_GET_DESCRIPTOR_REQ;
        // wValue: high byte is the descriptor type (HID Report), low byte is descriptor index (0 for now)
        req.wValue = (USB_DESCRIPTOR_HID_REPORT << 8) | descriptor_index;
        req.wIndex = interface_number;
        req.wLength = report_length;

        return send_usb_request_packet(device, req, report_buffer, report_length);
    }

    bool xhciDriver::set_device_configuration(xhciDevice *device, uint16_t configuration_value) {
        xhci_device_request_packet setup_packet{};
        memset(&setup_packet, 0, sizeof(xhci_device_request_packet));
        setup_packet.bRequestType = 0x00; // Host to Device, Standard, Device
        setup_packet.bRequest = USB_SET_CONFIGURATION_REQ;
        setup_packet.wValue = configuration_value;
        setup_packet.wIndex = 0;
        setup_packet.wLength = 0;

        if (!send_usb_no_data_request_packet(device, setup_packet)) {
            Log::Error("Failed to set device configuration");
            return false;
        }

        return true;
    }

    bool xhciDriver::setup_device(uint8_t port, xhci_portsc_register portsc) {
        uint8_t port_id = port + 1;
        uint16_t max_packet_size = get_max_initial_packet_size(portsc.port_speed);

        uint8_t slot_id = assign_slot();
        if (!slot_id) {
            Log::Error("Failed to enable device slot %u", slot_id);
            return false;
        }

        if (!create_device_context(slot_id)) {
            Log::Error("Failed to create device context");
            return false;
        }

        auto *device = new xhciDevice(slot_id, port_id, portsc.port_speed, m_64byte_context_size);

        configure_control_ep_input_context(device, max_packet_size);

        if (!address_device_command(device, true)) {
            Log::Error("Failed to setup device - failed to set device address");
            return false;
        }

        auto *device_descriptor = new usb_device_descriptor();
        if (!get_device_descriptor(device, device_descriptor, 8)) {
            Log::Error("Failed to get device descriptor");
            return false;
        }
        configure_control_ep_input_context(device, device_descriptor->bMaxPacketSize0);

        if (device_descriptor->bMaxPacketSize0 != max_packet_size) {
            max_packet_size = device_descriptor->bMaxPacketSize0;

            if (!evaluate_context(device)) {
                return false;
            }
        }

        address_device_command(device, false);

        device->sync_input_ctx(
            reinterpret_cast<void *>(m_dcbaa_virtual_addresses[device->get_slot_id()])
        );

        if (!get_device_descriptor(device, device_descriptor, device_descriptor->header.bLength)) {
            Log::Error("Failed to get full device descriptor");
            return false;
        }

#if 0
        Log::Info("USB Device Descriptor:");
        Log::Info("  bcdUSB:            0x%04x", device_descriptor->bcdUsb);
        Log::Info("  bDeviceClass:      0x%02x", device_descriptor->bDeviceClass);
        Log::Info("  bDeviceSubClass:   0x%02x", device_descriptor->bDeviceSubClass);
        Log::Info("  bDeviceProtocol:   0x%02x", device_descriptor->bDeviceProtocol);
        Log::Info("  bMaxPacketSize0:   0x%02x", device_descriptor->bMaxPacketSize0);
        Log::Info("  idVendor:          0x%04x", device_descriptor->idVendor);
        Log::Info("  idProduct:         0x%04x", device_descriptor->idProduct);
        Log::Info("  bcdDevice:         0x%04x", device_descriptor->bcdDevice);
        Log::Info("  iManufacturer:     0x%02x", device_descriptor->iManufacturer);
        Log::Info("  iProduct:          0x%02x", device_descriptor->iProduct);
        Log::Info("  iSerialNumber:     0x%02x", device_descriptor->iSerialNumber);
        Log::Info("  bNumConfigurations: 0x%02x", device_descriptor->bNumConfigurations);
#endif

        usb_string_language_descriptor string_language_descriptor{};
        if (!get_string_language_descriptor(device, &string_language_descriptor)) {
            return false;
        }
        uint16_t lang_id = string_language_descriptor.lang_ids[0];

        usb_string_descriptor *product_name = new usb_string_descriptor();
        if (!get_string_descriptor(device, device_descriptor->iProduct, lang_id, product_name)) {
            return false;
        }

        usb_string_descriptor *manufacturer_name = new usb_string_descriptor();
        if (!get_string_descriptor(device, device_descriptor->iManufacturer, lang_id, manufacturer_name)) {
            return false;
        }

        usb_string_descriptor *serial_number_string = new usb_string_descriptor();
        if (!get_string_descriptor(device, device_descriptor->iSerialNumber, lang_id, serial_number_string)) {
            return false;
        }

        char product[255] = {0};
        char manufacturer[255] = {0};
        char serial_number[255] = {0};

        utf16_to_utf8(product_name->unicode_string, sizeof(product_name->unicode_string), product);
        utf16_to_utf8(manufacturer_name->unicode_string, sizeof(manufacturer_name->unicode_string), manufacturer);
        utf16_to_utf8(serial_number_string->unicode_string, sizeof(manufacturer_name->unicode_string), serial_number);

        Log::PrintLn("---- USB Device Info ----");
        Log::PrintLn("  Product Name    : %s", product);
        Log::PrintLn("  Manufacturer    : %s", manufacturer);
        Log::PrintLn("  Serial Number   : %s", serial_number);

        auto *configuration_descriptor = new usb_configuration_descriptor();
        if (!get_configuration_descriptor(device, configuration_descriptor)) {
            return false;
        }

        if (product[0] == '?' && manufacturer[0] == '?' && serial_number[0] == '?') {
            Log::LogMsg("Unknown USB device, canceling setup...");
            return false;
        }

        Log::PrintLn("  Configuration   :");
        Log::PrintLn("      wTotalLength        - %u", configuration_descriptor->wTotalLength);
        Log::PrintLn("      bNumInterfaces      - %u", configuration_descriptor->bNumInterfaces);
        Log::PrintLn("      bConfigurationValue - %u", configuration_descriptor->bConfigurationValue);
        Log::PrintLn("      iConfiguration      - %u", configuration_descriptor->iConfiguration);
        Log::PrintLn("      bmAttributes        - %u", configuration_descriptor->bmAttributes);
        Log::PrintLn("      bMaxPower           - %u milliamps", configuration_descriptor->bMaxPower * 2);

        device->sync_input_ctx(
            reinterpret_cast<void *>(m_dcbaa_virtual_addresses[device->get_slot_id()])
        );

        if (!set_device_configuration(device, configuration_descriptor->bConfigurationValue)) {
            return false;
        }

        uint8_t *buffer = configuration_descriptor->data;
        uint16_t total_length = configuration_descriptor->wTotalLength - configuration_descriptor->header.bLength;
        uint16_t index = 0;

        while (index < total_length) {
            usb_descriptor_header *header = reinterpret_cast<usb_descriptor_header *>(&buffer[index]);

            switch (header->bDescriptorType) {
                case USB_DESCRIPTOR_INTERFACE: {
                    usb_interface_descriptor *iface_desc = reinterpret_cast<usb_interface_descriptor *>(header);
                    device->setup_add_interface(iface_desc);
                    break;
                }
                case USB_DESCRIPTOR_HID: {
                    Log::LogMsg("USB_DESCRIPTOR_HID");
                    // Process HID Descriptor
                    auto *hid_desc = reinterpret_cast<usb_hid_descriptor *>(header);

                    // Process subordinate descriptors
                    for (uint8_t i = 0; i < hid_desc->bNumDescriptors; i++) {
                        // Check if this subordinate descriptor is the HID Report Descriptor
                        if (hid_desc->desc[i].bDescriptorType == USB_DESCRIPTOR_HID_REPORT) {
                            if (device->interfaces.empty()) {
                                Log::Error("??? HID descriptor discovered before an interface!");
                                break;
                            }
                            const auto &current_interface = device->interfaces.back();

                            current_interface->additional_data_length = hid_desc->desc[i].wDescriptorLength;

                            // Allocate a buffer to hold the HID report descriptor.
                            current_interface->additional_data = new uint8_t[current_interface->additional_data_length];

                            int8_t interface_number = device->interfaces.back()->descriptor.bInterfaceNumber;

                            // Retrieve the HID report descriptor.
                            if (!get_hid_report_descriptor(device, interface_number, 0,
                                                           current_interface->additional_data,
                                                           current_interface->additional_data_length)) {
                                Log::debug("get_hid_report_descriptor failed");
                                delete[] current_interface->additional_data;
                                current_interface->additional_data_length = 0;
                            }
                            Log::PrintLn("after get_hid_report_descriptor");
                        }
                    }
                    break;
                }
                case USB_DESCRIPTOR_ENDPOINT: {
                    if (device->interfaces.empty()) {
                        Log::Error("??? Endpoint descriptor discovered before an interface!");
                        break;
                    }
                    auto &current_interface = device->interfaces.back();

                    auto *ep_desc = reinterpret_cast<usb_endpoint_descriptor *>(header);
                    current_interface->setup_add_endpoint(ep_desc);
                    break;
                }
                default: break;
            }

            index += header->bLength;
        }

        Log::debug("after while");

        xhci_input_control_context32 *in_ctrl_ctx = device->get_input_control_ctx();
        in_ctrl_ctx->add_flags = (1 << 0);
        in_ctrl_ctx->drop_flags = 0;

        for (auto &iface: device->interfaces) {
            Log::PrintLn("  ---- Interface %u ----", iface->descriptor.bInterfaceNumber);
            Log::PrintLn("  class    : %u", iface->descriptor.bInterfaceClass);
            Log::PrintLn("  subclass : %u", iface->descriptor.bInterfaceSubClass);
            Log::PrintLn("  protocol : %u", iface->descriptor.bInterfaceProtocol);

            for (auto &ep: iface->endpoints) {
                Log::PrintLn("    -- Endpoint %u --", ep->xhc_endpoint_num);
                Log::PrintLn("    type            : %u", ep->xhc_endpoint_type);
                Log::PrintLn("    address         : 0x%x", ep->usb_endpoint_addr);
                Log::PrintLn("    max_packet_size : %u", ep->max_packet_size);
                Log::PrintLn("    interval        : %u", ep->interval);
                Log::PrintLn("    attribs         : %u", ep->usb_endpoint_attributes);

                configure_ep_input_context(device, ep);
            }

            if (iface->descriptor.bInterfaceClass == 3 && iface->descriptor.bInterfaceSubClass == 1) {
                // Mouse
                if (iface->descriptor.bInterfaceProtocol == 2) {
                    Log::Warning("MOUSE");
                    //      iface->driver = new xhci_usb_hid_mouse_driver();
                    //     iface->driver->attach_interface(iface);
                }

                // Keyboard
                if (iface->descriptor.bInterfaceProtocol == 1) {
                    Log::Warning("KEYBOARD");
                    //    iface->driver = new xhci_usb_hid_kbd_driver();
                    //     iface->driver->attach_interface(iface);
                }
            }
        }


        return true;
    }

    bool xhciDriver::evaluate_context(const xhciDevice *dev) {
        xhci_evaluate_context_command_trb_t evaluate_context_trb{};
        evaluate_context_trb.trb_type = XHCI_TRB_TYPE_EVALUATE_CONTEXT_CMD;
        evaluate_context_trb.input_context_physical_base = dev->get_input_context_phys();
        evaluate_context_trb.slot_id = dev->get_slot_id();

        xhci_command_completion_trb_t *completion_trb =
                _send_command(reinterpret_cast<xhci_trb_t *>(&evaluate_context_trb), 200);

        if (!completion_trb) {
            Log::Error("Failed to send Evaluate Context command");
            return false;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::Error("Evaluate Context command failed with completion code: %s",
                       trb_completion_code_to_string(completion_trb->completion_code));
            return false;
        }

        return true;
    }

    uint8_t xhciDriver::assign_slot() {
        xhci_trb_t trb{};
        trb.parameter = 0;
        trb.status = 0;

        trb.cycle_bit = m_command_ring->get_cycle_bit();
        trb.eval_next_trb = 0;
        trb.interrupt_on_completion = 1;
        trb.chain_bit = 0;
        trb.immediate_data = 0;
        trb.block_event_interrupt = 0;
        trb.trb_type = XHCI_TRB_TYPE_ENABLE_SLOT_CMD;

        xhci_command_completion_trb_t *cce = _send_command(&trb);

        if (!cce) {
            Log::Error("[xhci] assign_slot enable slot failed (no completion TRB)");
            return 0;
        }
        if (cce->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::Error("[xhci] enable slot returned code 0x%02x", cce->completion_code);
            return 0;
        }


        return cce->slot_id & 0xFF;
    }

    bool xhciDriver::create_device_context(uint8_t slot_id) const {
        // Allocate a memory block for the device context
        uint64_t device_context_size = m_64byte_context_size
                                           ? sizeof(xhci_device_context64)
                                           : sizeof(xhci_device_context32);

        void *ctx = alloc_xhci_memory(
            device_context_size,
            XHCI_DEVICE_CONTEXT_ALIGNMENT,
            XHCI_DEVICE_CONTEXT_BOUNDARY
        );
        memset(ctx, 0, device_context_size);

        if (!ctx) {
            Log::Error("Failed to allocate memory for a device context");
            return false;
        }

        m_dcbaa[slot_id] = xhci_get_physical_addr(ctx);

        // Store the virtual address as well
        m_dcbaa_virtual_addresses[slot_id] = reinterpret_cast<uint64_t>(ctx);

        return true;
    }


    void xhciDriver::configure_control_ep_input_context(xhciDevice *dev, uint16_t max_packet_size) {
        xhci_input_control_context32 *input_control_context = dev->get_input_control_ctx();
        xhci_slot_context32 *slot_context = dev->get_input_slot_ctx();
        xhci_endpoint_context32 *control_ep_context = dev->get_input_control_ep_ctx();

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
        control_ep_context->transfer_ring_dequeue_ptr = dev->get_control_transfer_ring()->
                get_physical_dequeue_pointer_base();
        control_ep_context->dcs = dev->get_control_transfer_ring()->get_cycle_bit();
        control_ep_context->max_esit_payload_lo = 0;
        control_ep_context->max_esit_payload_hi = 0;
        control_ep_context->average_trb_length = 8;
    }

    void xhciDriver::configure_ep_input_context(xhciDevice *dev, xhciEndpoint *endpoint) {
        xhci_input_control_context32 *input_control_context = dev->get_input_control_ctx();
        xhci_slot_context32 *slot_context = dev->get_input_slot_ctx();

        // Enable the input control context flags
        input_control_context->add_flags |= (1 << endpoint->xhc_endpoint_num);
        input_control_context->drop_flags = 0;

        if (endpoint->xhc_endpoint_num > slot_context->context_entries) {
            slot_context->context_entries = endpoint->xhc_endpoint_num;
        }

        // Configure the endpoint context
        xhci_endpoint_context32 *interrupt_ep_context =
                dev->get_input_ep_ctx(endpoint->xhc_endpoint_num);

        memset(interrupt_ep_context, 0, sizeof(xhci_endpoint_context32));
        interrupt_ep_context->endpoint_state = XHCI_ENDPOINT_STATE_DISABLED;
        interrupt_ep_context->endpoint_type = endpoint->xhc_endpoint_type;
        interrupt_ep_context->max_packet_size = endpoint->max_packet_size;
        interrupt_ep_context->max_esit_payload_lo = endpoint->max_packet_size;
        interrupt_ep_context->error_count = 3;
        interrupt_ep_context->max_burst_size = 0;
        interrupt_ep_context->average_trb_length = endpoint->max_packet_size;
        interrupt_ep_context->transfer_ring_dequeue_ptr = endpoint->get_transfer_ring()->
                get_physical_dequeue_pointer_base();
        interrupt_ep_context->dcs = endpoint->get_transfer_ring()->get_cycle_bit();

        if (dev->get_speed() == XHCI_USB_SPEED_HIGH_SPEED || dev->get_speed() == XHCI_USB_SPEED_SUPER_SPEED) {
            interrupt_ep_context->interval = endpoint->interval - 1;
        } else {
            interrupt_ep_context->interval = endpoint->interval;

            if (
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_INTERRUPT_IN ||
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_INTERRUPT_OUT ||
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_ISOCHRONOUS_IN ||
                endpoint->xhc_endpoint_type == XHCI_ENDPOINT_TYPE_ISOCHRONOUS_OUT
            ) {
                if (endpoint->interval < 3) {
                    interrupt_ep_context->interval = 3;
                } else if (endpoint->interval > 18) {
                    interrupt_ep_context->interval = 18;
                }
            }
        }
    }

    uint16_t xhciDriver::get_max_initial_packet_size(uint8_t port_speed) {
        uint16_t initial_max_packet_size = 0;
        switch (port_speed) {
            case XHCI_USB_SPEED_LOW_SPEED: initial_max_packet_size = 8;
                break;

            case XHCI_USB_SPEED_FULL_SPEED:
            case XHCI_USB_SPEED_HIGH_SPEED: initial_max_packet_size = 64;
                break;

            case XHCI_USB_SPEED_SUPER_SPEED:
            case XHCI_USB_SPEED_SUPER_SPEED_PLUS:
            default: initial_max_packet_size = 512;
                break;
        }

        return initial_max_packet_size;
    }

    bool xhciDriver::address_device_command(xhciDevice *dev, bool bsr) {
        uint64_t input_ctx_phys = dev->get_input_context_phys();

        xhci_address_device_command_trb_t address_device_trb;
        memset(&address_device_trb, 0, sizeof(xhci_address_device_command_trb_t));

        address_device_trb.trb_type = XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD;
        address_device_trb.input_context_physical_base = input_ctx_phys;
        address_device_trb.bsr = bsr ? 1 : 0;
        address_device_trb.slot_id = dev->get_slot_id();
        address_device_trb.cycle_bit = m_command_ring->get_cycle_bit();

        xhci_command_completion_trb_t *completion_trb =
                _send_command(reinterpret_cast<xhci_trb_t *>(&address_device_trb), 200);

        if (!completion_trb) {
            Log::Error("[xhci] Failed to address device with BSR=%u", (int) bsr);
            return false;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::Error("[xhci] Address Device returned code 0x%02x", completion_trb->completion_code);
            return false;
        }

        return true;
    }*/
} // namespace USB
