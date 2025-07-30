#include "xhci.h"

#include "xhci_ext_cap.h"
#include "../../../include/log.h"
#include "../../../include/vector.h"
#include "../../../kernel/time/time.h"
#include "../../pci/pci.h"
#include "../../../kernel/include/interrupts.h"

namespace USB {
    xhciDriver::xhciDriver() {
    }

    bool xhciDriver::init_device(PCI::PCIDeviceHeader *pci_base_address) {
        auto *pci_hdr = reinterpret_cast<PCI::PCIHeader0 *>(pci_base_address);
        uint64_t bar0 = pci_hdr->BAR0 & ~0xF;
        uint64_t bar1 = pci_hdr->BAR1;
        uint64_t bar = ((bar1 << 32) | bar0);


        // 1. Originale Werte sichern
        uint32_t original_bar0 = pci_hdr->BAR0;
        uint32_t original_bar1 = pci_hdr->BAR1;

        // 2. Temporär 0xFFFFFFFF schreiben
        pci_hdr->BAR0 = 0xFFFFFFFF;
        pci_hdr->BAR1 = 0xFFFFFFFF;

        // 3. Gelesene Werte interpretieren
        uint32_t size_mask_lo = pci_hdr->BAR0;
        uint32_t size_mask_hi = pci_hdr->BAR1;

        // 4. Originale Werte wiederherstellen
        pci_hdr->BAR0 = original_bar0;
        pci_hdr->BAR1 = original_bar1;

        // 5. Größe berechnen
        uint64_t mask = ((uint64_t) size_mask_hi << 32) | (size_mask_lo & ~0xF);
        if (mask == 0) {
            return false;
        }

        uint64_t bar_size = ~(mask) + 1;

        m_xhc_base = xhci_map_mmio(bar, bar_size);

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

        Log::PrintLn("Controller started!");

        for (uint8_t port = 0; port < m_max_ports; port++) {
            Log::PrintLn("Port %u is USB%u", port, is_usb3_port(port) ? 3 : 2);
        }

        return true;
    }

    bool xhciDriver::shutdown_device() {
        return true;
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
        volatile uint32_t *head_cap_ptr = reinterpret_cast<volatile uint32_t *>(
            m_xhc_base + m_extended_capabilities_offset);

        extended_capabilities_head = new xhci_extended_capability(head_cap_ptr);

        auto node = extended_capabilities_head;
        while (node) {
            Log::PrintLn("node>id: %u", node->id());

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

    void xhciDriver::log_usbsts() {
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
        Log::PrintLn("");
    }

    void xhciDriver::claim_legacy_ownership(xhci_legacy_support_capability* legacy) {
        Log::PrintLn("USBLEGSUP: 0x%08x", legacy->usblegsup.raw);
        Log::PrintLn("USBLEGCTLSTS: 0x%08x", legacy->usblegctlsts.raw);
        Log::PrintLn("os_owned: %u", legacy->usblegsup.os_owned);
        Log::PrintLn("bios_owned: %u", legacy->usblegsup.bios_owned);
        // Set OS_OWNED bit (bit 24)
        legacy->usblegsup.os_owned = 1;
        Log::PrintLn("Set OS_OWNED bit in USBLEGSUP");

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

        if (!(legacy->usblegsup.bios_owned == 1)) {
            Log::PrintLn("BIOS released ownership after %d ms", waited);
        }

        // Clear SMI bits: bits 29–31 (RW1C)
        legacy->usblegctlsts.raw = (1 << 29) | (1 << 30) | (1 << 31);
        Log::PrintLn("Cleared SMI bits in USBLEGCTLSTS (bits 29-31)");
    }

    bool xhciDriver::is_usb3_port(uint8_t port_num) {
        for (size_t i = 0; i < m_usb3_ports.size(); i++) {
            if (m_usb3_ports[i] == port_num) {
                return true;
            }
        }
        return false;
    }

    bool xhciDriver::reset_host_controller() {
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

        // Setup the command ring and write CRCR
        m_command_ring = new xhciCommandRing(XHCI_COMMAND_RING_TRB_COUNT);

        Log::PrintLn("xhciDriver::configure_operational_registers: %p %p %p", m_command_ring->get_physical_base(), m_command_ring->get_cycle_bit(), m_command_ring->get_physical_base() | m_command_ring->get_cycle_bit());
        Log::Info("CRCR: 0x%llx", m_op_regs->crcr);
        Log::Info("Command Ring Running (CRR): %s", (m_op_regs->crcr & (1 << 1)) ? "yes" : "no");
        kernel::time::sleep_ms(100);
        m_op_regs->crcr = 0xC039181; // m_command_ring->get_physical_base() | m_command_ring->get_cycle_bit();
        Log::PrintLn("xhciDriver::configure_operational_registers after: %p", m_op_regs->crcr);

    }

    void xhciDriver::setup_dcbaa() {
        size_t dcbaa_size = sizeof(uintptr_t) * (m_max_device_slots + 1);

        m_dcbaa = reinterpret_cast<uint64_t *>(
            alloc_xhci_memory(dcbaa_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY)
        );

        m_dcbaa_virtual_addresses = new uint64_t[m_max_device_slots + 1];

        if (m_max_scratchpad_buffers > 0) {
            uint64_t *scratchpad_array = reinterpret_cast<uint64_t *>(
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

    void xhciDriver::acknowledge_irq(uint8_t interrupter) {
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

    xhci_command_completion_trb_t *xhciDriver::_send_command_trb(xhci_trb_t *cmd_trb, uint32_t timeout_ms) {
        m_command_ring->enqueue(cmd_trb);

        m_doorbell_manager->ring_command_doorbell();

        // Wait for the IRQ and let the host controller process the command
        uint64_t sleep_passed = 0;
        while (!m_command_irq_completed) {
            kernel::time::sleep_ms(10);
            sleep_passed += 10;

            if (sleep_passed > timeout_ms * 1000) {
                break;
            }
        }

        //  - Only one command is being sent to the controller at a time
        xhci_command_completion_trb_t *completion_trb =
                m_command_completion_events.size() ? m_command_completion_events[0] : nullptr;

        // Reset the irq flag and clear out the command completion event queue
        m_command_completion_events.clear();
        m_command_irq_completed = 0;

        if (!completion_trb) {
            Log::Error("Failed to find completion TRB for command %i\n", cmd_trb->trb_type);
            return nullptr;
        }

        if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
            Log::Error("Command TRB failed with error: %s\n",
                       trb_completion_code_to_string(completion_trb->completion_code));
            return nullptr;
        }

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

    bool xhciDriver::start_host_controller() {
        uint32_t usbcmd = m_op_regs->usbcmd;

        log_operational_registers();

        Log::Info("usbcmd1 %p", m_op_regs->usbcmd);
        m_op_regs->usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
        Log::Info("usbcmd2 %p", m_op_regs->usbcmd);

        m_op_regs->usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;
        Log::Info("usbcmd3 %p", m_op_regs->usbcmd);
        asm volatile ("" ::: "memory");

        m_op_regs->usbcmd |= XHCI_USBCMD_RUN_STOP;

        Log::Info("usbcmd4 %p", m_op_regs->usbcmd);

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

    // 4. Enhanced interrupt handler with debugging
    irqreturn_t xhciDriver::xhci_irq_handler(xhciDriver *driver) {
        driver->process_events();
        driver->acknowledge_irq(0);


        return IRQ_HANDLED;
    }

    void xhciDriver::process_events() {
        // Poll the event ring for the command completion event
        Vector<xhci_trb_t *> events;
        if (m_event_ring->has_unprocessed_events()) {
            m_event_ring->dequeue_events(events);
        }

        uint8_t command_completion_status = 0;

        for (size_t i = 0; i < events.size(); i++) {
            xhci_trb_t *event = events[i];
            switch (event->trb_type) {
                case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT: {
                    command_completion_status = 1;
                    m_command_completion_events.push_back((xhci_command_completion_trb_t *) event);
                    break;
                }
                default: break;
            }
        }

        m_command_irq_completed = command_completion_status;
    }
} // namespace USB
