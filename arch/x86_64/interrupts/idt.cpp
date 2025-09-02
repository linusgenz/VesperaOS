#include "idt.h"

#include <log.h>

#include "../../../kernel/include/memory.h"
#include "interrupts_internal.h"
#include "../../../kernel/include/interrupts.h"
#include "apic.h"

namespace arch::x86_64::interrupts::idt {
    void IDTDescEntry::set_offset(uint64_t offset) {
        offset0 = (uint16_t) (offset & 0x000000000000ffff);
        offset1 = (uint16_t) ((offset & 0x00000000ffff0000) >> 16);
        offset2 = (uint32_t) ((offset & 0xffffffff00000000) >> 32);
    }

    uint64_t IDTDescEntry::get_offset() {
        uint64_t offset = 0;
        offset |= (uint64_t) offset0;
        offset |= (uint64_t) offset1 << 16;
        offset |= (uint64_t) offset2 << 32;
        return offset;
    }

    void set_idt_gate(void* handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector) {
        IDTDescEntry* interrupt = (IDTDescEntry*)(idtr.offset + entry_offset * sizeof(IDTDescEntry));
        interrupt->set_offset((uint64_t)handler);
        interrupt->selector = selector;
        interrupt->ist = 0;
        interrupt->type_attr = type_attr;
        interrupt->ignore = 0;
    }

    bool register_irq_handler(uint8_t irqno, irq_handler_t handler, void* cookie) {
        if (irqno >= IRQ_MAX) return false;
        if (irq_handler_table[irqno].handler != nullptr) return false; // Schon vergeben
        irq_handler_table[irqno].handler = handler;
        irq_handler_table[irqno].cookie = cookie;
        return true;
    }

    extern "C" void irq_common_stub_handler(uint8_t irqno) {
        if (irqno >= IRQ_MAX) return;

        irq_desc& desc = irq_handler_table[irqno];

        if (desc.handler) {
            desc.handler(desc.cookie);
        }

        apic::send_eoi();
    }

    extern "C" void irq_stub_0x30();
    void load_default_idt() {
        void* idt_page = kernel::memory::request_page();
        memset(idt_page, 0, 0x1000);

        idtr.limit = 0x0FFF;
        idtr.offset = reinterpret_cast<uint64_t>(idt_page);

        // Standard Exception Handlers
        set_idt_gate((void*)divide_error_handler, 0x00, IDT_TA_InterruptGate, 0x08);          // Divide by Zero
        set_idt_gate((void*)invalid_opcode_handler, 0x06, IDT_TA_InterruptGate, 0x08);       // Invalid Opcode
        set_idt_gate((void*)double_fault_handler, 0x08, IDT_TA_InterruptGate, 0x08);         // Double Fault
        set_idt_gate((void*)segment_not_present_handler, 0x0B, IDT_TA_InterruptGate, 0x08);  // Segment Not Present
        set_idt_gate((void*)stack_fault_handler, 0x0C, IDT_TA_InterruptGate, 0x08);          // Stack Fault
        set_idt_gate((void*)gp_fault_handler, 0x0D, IDT_TA_InterruptGate, 0x08);             // General Protection
        set_idt_gate((void*)page_fault_handler, 0x0E, IDT_TA_InterruptGate, 0x08);           // Page Fault
        set_idt_gate((void*)machine_check_handler, 0x12, IDT_TA_InterruptGate, 0x08);        // Machine Check
        set_idt_gate((void*)irq_stub_0x30, IRQ_XHCI_VECTOR, IDT_TA_InterruptGate, 0x08);

        set_idt_gate((void*)keyboard_int_handler, 0x21, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)mouse_int_handler, 0x2C, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)apic_timer_int_handler, IRQ_TIMER, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)spurious_int_handler, IRQ_SPURIOUS, IDT_TA_InterruptGate, 0x08);

        asm ("lidt %0" : : "m" (idtr));
        asm ("cli");
    }

    IDTR* get_idtr_address() {
        return &idtr;
    }

}
