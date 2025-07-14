#include "usb.h"
#include "../../kernel/include/basic_renderer.h"
#include "../../kernel/include/page_table_manager.h"
#include "../../kernel/include/page_frame_allocator.h"
#include "../../kernel/scheduling/pit_legacy/pit.h"
#include "../../kernel/include/memory.h"
#include "../../kernel/time/time.h"

namespace USB {
    /*xHCIDriver::xHCIDriver(PCI::PCIDeviceHeader* pci_base_address) {
        this->PCIBaseAddress = pci_base_address;
        global_renderer->print("xHCI Driver instance initialized");
        global_renderer->new_line();

        uint64_t bar0 = reinterpret_cast<PCI::PCIHeader0 *>(pci_base_address)->BAR0 & ~0xF;
        uint64_t bar1 = reinterpret_cast<PCI::PCIHeader0 *>(pci_base_address)->BAR1;
        bar = reinterpret_cast<xHCICapabilityRegisters *>((bar1 << 32) | bar0);
        global_page_table_manager.map_memory(bar, bar, false);

        op_regs = (xHCIOperationalRegisters*)((uint64_t)bar + bar->caplength);
        global_page_table_manager.map_memory(op_regs, op_regs, false);

        runtime_regs = (xHCIRuntimeRegisters*)((uint64_t)bar + bar->rts_off);
        global_page_table_manager.map_memory(runtime_regs, runtime_regs, false);

        global_renderer->print("ADDRESS: ");
        global_renderer->print(to_hstring((uint64_t)bar));
        global_renderer->new_line();

        uint16_t hci_version = bar->hci_version;
        xHCIHcsParams1 hcs_params1 = bar->hcs_params_1;
        xHCIHcsParams2 hcs_params2 = bar->hcs_params_2;
        xHCIHcsParams3 hcs_params3 = bar->hcs_params_3;

        uint8_t major_bcd = (hci_version >> 8) & 0xFF;
        uint8_t minor_bcd = hci_version & 0xFF;

        uint8_t major_version = ((major_bcd >> 4) * 10) + (major_bcd & 0x0F);
        uint8_t minor_version = ((minor_bcd >> 4) * 10) + (minor_bcd & 0x0F);


        global_renderer->print("Version: ");
        global_renderer->print(to_string(major_version));
        global_renderer->print(".");
        global_renderer->print(to_string(minor_version));
        global_renderer->new_line();
        global_renderer->print("Ports: ");
        global_renderer->print(to_string(hcs_params1.max_ports));
        global_renderer->new_line();
        global_renderer->print("Slots: ");
        global_renderer->print(to_string(hcs_params1.max_slots));
        global_renderer->new_line();
        global_renderer->print("intrps: ");
        global_renderer->print(to_string(hcs_params1.max_intrs));
        global_renderer->new_line();
    


                // Now you can access the operational registers
        global_renderer->print("Operational Registers Address: ");
        global_renderer->print(to_hstring((uint64_t)op_regs));
        global_renderer->new_line();

        global_renderer->print("USBCMD Register: ");
        global_renderer->print(to_hstring(op_regs->usbcmd.RS));
        global_renderer->new_line();

        stop();
        reset();
        wait_controller_ready();
        op_regs->config.MaxSlotsEn = hcs_params1.max_slots;
        initialize_FLADJ();
        initialize_doorbell_registers(hcs_params1.max_slots);
        initialize_dcbaa();
        initialize_command_ring();
        initialize_event_ring();
        op_regs->usbcmd.RS = 1;

        if (op_regs->usbsts.HCH) {  // Check if Host Controller is Halted
            global_renderer->print("Host Controller is halted. Check command execution.");
        }

        if (op_regs->usbsts.HCE) {  // Check for Host Controller Error
            global_renderer->print("Host Controller Error detected.");
        }

        uint64_t port_regs_base_address = bar->caplength + PORT_REGISTER_OFFSET;
        uint64_t port_regs_size = hcs_params1.max_ports * 0x10;  // each port takes 0x10 bytes
        global_page_table_manager.map_memory((void*)port_regs_base_address, (void*)(port_regs_base_address), false);

        ports = new xHCIPortRegisters[hcs_params1.max_ports];
        for (uint32_t port_num = 0; port_num < hcs_params1.max_ports; port_num++) {
            uint64_t port_base_address = port_regs_base_address + (0x10 * port_num);

            // Access the PORTSC, PORTPMSC, and PORTLI registers directly
            ports[port_num].portsc = *(PORTSC*)port_base_address;
            ports[port_num].portpmsc = *(uint32_t*)(port_base_address + 0x4);  // PORTPMSC is at +4 offset
            ports[port_num].portli = *(uint32_t*)(port_base_address + 0x8);    // PORTLI is at +8 offset

            // Debug information
            global_renderer->print("Port ");
            global_renderer->print(to_string(port_num + 1));  // Port numbering starts from 1
            global_renderer->print(" mapped at PORTSC: ");
            global_renderer->print(to_hstring(port_base_address));
            global_renderer->print(", PORTPMSC: ");
            global_renderer->print(to_hstring(port_base_address + 0x4));
            global_renderer->print(", PORTLI: ");
            global_renderer->print(to_hstring(port_base_address + 0x8));
            global_renderer->new_line();
        }

        initialize_port(0);

        global_renderer->print("TEST");
    }

    void xHCIDriver::enumerate_devices(uint32_t max_ports) {
        for (uint32_t i = 0; i < max_ports; ++i) {
            if (initialize_port(i)) {
                global_renderer->print("device on port");
                uint8_t result = enumerate_device(i);
                if (result != 0) {
                    global_renderer->print("Device on port ");
                    global_renderer->print(to_string(i + 1));
                    global_renderer->print(" enumerated successfully.");
                    global_renderer->new_line();
                } else {
                    global_renderer->print("Failed to enumerate device on port ");
                    global_renderer->print(to_string(i + 1));
                    global_renderer->new_line();
                }
            }
        }
    }

    uint8_t xHCIDriver::enumerate_device(uint32_t port_number) {
        
    }

    void xHCIDriver::initialize_event_ring() {
        // 1. Allocate Event Ring Segment
        size_t event_ring_size = 256; // Define number of TRBs per segment (e.g., 256)
        event_ring_segment = (TRB*)malloc(event_ring_size * sizeof(TRB));
        if (!event_ring_segment) {
            global_renderer->print("Failed to allocate memory for Event Ring Segment");
            return;
        }
        memset(event_ring_segment, 0, event_ring_size * sizeof(TRB));

        // 2. Allocate Event Ring Segment Table (ERST)
        ERSTEntry* erst = (ERSTEntry*)malloc(sizeof(ERSTEntry));
        if (!erst) {
            global_renderer->print("Failed to allocate memory for ERST");
            free(event_ring_segment);
            return;
        }

        // Initialize ERST entry to point to Event Ring Segment
        erst->ring_segment_size = (uint64_t)event_ring_segment >> 6;
        erst->ring_segment_size = event_ring_size; // Number of TRBs in the segment

        runtime_regs->ir[0].erstsz.segment_count = 1; // Number of entries in the ERST
        runtime_regs->ir[0].erstba.erstba_reg = ((uint64_t)erst >> 6) & 0x03FFFFFFFFFFFFFF; // Set base address (shifted by 6)
        runtime_regs->ir[0].erdp.ERDP = (uint64_t)event_ring_segment; // Set Event Ring Dequeue Pointer

        global_renderer->print("Event Ring and ERST initialized.");
        global_renderer->new_line();
    }

    void xHCIDriver::initialize_command_ring() {
        size_t command_ring_size = 1024;

        command_ring = (xHCICommandRing*)malloc(sizeof(xHCICommandRing));
        command_ring->trbs = (TRB*)malloc(command_ring_size * sizeof(TRB));
        memset(command_ring->trbs, 0, command_ring_size * sizeof(TRB));
        command_ring->cycle_state = 1;
        command_ring->enqueue_index = 0;
        command_ring->dequeue_index = 0;

        NoOpCommandTRB* no_op_trb = new NoOpCommandTRB();
        no_op_trb->cycleBit = command_ring->cycle_state;

        add_trb_to_command_ring(*command_ring, no_op_trb);

        op_regs->crcr.CRR = 1;
        op_regs->crcr.cmd_ring_p = ((uint64_t)&command_ring->trbs[command_ring->enqueue_index] >> 6) | command_ring->cycle_state;
    }

    void xHCIDriver::stop_command_ring() {
        if (op_regs->crcr.CRR == 1) {
            op_regs->crcr.CS = 1; 
            PIT::sleep(10); // Allow time for the command to complete
        }
    }

    void xHCIDriver::add_trb_to_command_ring(xHCICommandRing &command_ring, TRB* trb) {
        if (op_regs->crcr.CRR == 0) {
        // Create a TRB entry with given trb_value
        command_ring.trbs[command_ring.enqueue_index] = *trb;

        // Ensure cycle bit is set correctly
        command_ring.trbs[command_ring.enqueue_index].cycleBit = command_ring.cycle_state;

        command_ring.enqueue_index = (command_ring.enqueue_index + 1) % RING_SIZE;

        if (command_ring.enqueue_index == 0) {
            command_ring.cycle_state ^= 1;
        }

        op_regs->crcr.cmd_ring_p = (uint64_t)(command_ring.trbs) | command_ring.cycle_state;

        PIT::sleep(10); // Sleep to allow the controller to process

        // Check status or any specific registers to verify command execution
        global_renderer->print("Command Ring Cycle State: ");
        global_renderer->print(to_string(op_regs->crcr.CRR));  // This should match your expected state
        global_renderer->new_line();
        } else {
            
        }
    }

    uint8_t xHCIDriver::initialize_port(uint32_t port_number) {
        PORTSC portsc = ports[port_number].portsc;
        if (portsc.CCS == 0) {
            return 0;
        }
        if (portsc.PED==0) {
            portsc.PR == 1;
            PIT::sleep(15);
        }
        if (portsc.PED == 0) {
            return 0;
        }

        global_renderer->print("Port ");
        global_renderer->print(to_string(port_number + 1)); 
        global_renderer->print(" initialized.");
        global_renderer->new_line();

        return 1; // Success
    }

    void xHCIDriver::stop(){
        if(op_regs->usbcmd.RS == 1){
            op_regs->usbcmd.RS = 0;
            kernel::time::sleep_ms(10);
            while(op_regs->usbsts.HCH==0);
        }
    }

    void xHCIDriver::reset(){
        op_regs->usbcmd.HCR = 1;
        kernel::time::sleep_ms(10);
        while(op_regs->usbcmd.HCR==1);
    }

    void xHCIDriver::wait_controller_ready(){
        while(op_regs->usbsts.CNR) {
            kernel::time::sleep_ms(10);
        }
    }

    void xHCIDriver::initialize_FLADJ() {
        fladj_reg = (FLADJ*)((uint64_t)bar + FLADJ_OFFSET);
        if (fladj_reg->NFC == 0) {
            global_renderer->print("FLADJ supported.");
            fladj_reg->FLTV = DEFAULT_FRAME_LENGTH_TIMING_VALUE;
        } else {
            global_renderer->print("Frame Length Timing Value not supported (NFC is 1).");
            global_renderer->new_line();
        }
        global_renderer->new_line();
    }

    void xHCIDriver::initialize_dcbaa() {
        void* page_address = global_allocator.request_page();
        global_page_table_manager.map_memory(page_address, page_address, false);
        if (page_address) {
            // Align the address to a 64-byte boundary
            page_address = (void*)(((uintptr_t)(page_address) + 0x3F) & ~0x3F);

            memcpy(dcbaa, page_address, sizeof(dcbaa));

            uint64_t dcbaa_physical_address = (uint64_t)&dcbaa;

            op_regs->dcbaap = dcbaa_physical_address >> 6;

            for (int i = 0; i < 256; i++) {
                dcbaa[i] = 0;
            }

        } else {
            // Handle allocation failure
            global_renderer->print("Page allocation failed.");
        }
    }

    void xHCIDriver::initialize_doorbell_registers(uint32_t max_slots) {
        doorbell_regs = new Doorbell[max_slots];
        
        uint32_t db_offset = bar->db_off;

        uint64_t doorbell_array_base = ((uint64_t)bar + db_offset) & ~0x3; // Align to DWORD (4-byte) boundary

        global_page_table_manager.map_memory((void*)doorbell_array_base, (void*)(doorbell_array_base + (max_slots * sizeof(Doorbell))), false);

        for (uint32_t i = 0; i < max_slots; ++i) {
            *(volatile uint32_t*)(doorbell_array_base + (i * sizeof(Doorbell))) = 0; // Clear each doorbell register
        }

        global_renderer->print("Doorbell Registers initialized at: ");
        global_renderer->print(to_hstring(doorbell_array_base));
        global_renderer->new_line();
    }*/
}
