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

    void set_idt_gate(void *handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector) {
        IDTDescEntry *interrupt = (IDTDescEntry *) (idtr.offset + entry_offset * sizeof(IDTDescEntry));
        interrupt->set_offset((uint64_t) handler);
        interrupt->selector = selector;
        interrupt->ist = 0;
        interrupt->type_attr = type_attr;
        interrupt->ignore = 0;
    }

    void init_irq_table() {
        for (auto &[handler, cookie, free] : irq_handler_table) {
            handler = nullptr;
            cookie  = nullptr;
            free    = true;
        }
    }

    uint8_t get_free_vector() {
        for (uint16_t vec = VECTOR_MIN; vec <= VECTOR_MAX; ++vec) {
            if (irq_handler_table[vec].free) {
                irq_handler_table[vec].free = false;
                return static_cast<uint8_t>(vec);
            }
        }
        return 0xFF; // no vector available
    }

    void free_vector(uint8_t vec) {
        if (vec < IRQ_MAX) {
            irq_handler_table[vec].handler = nullptr;
            irq_handler_table[vec].cookie = nullptr;
            irq_handler_table[vec].free = true;
        }
    }

    extern "C" void* irq_stub_table[]; // irq_stub.asm
    bool allocate_vector(uint8_t vector, const irq_handler_t handler, void *cookie) {
        irq_handler_table[vector].handler = handler;
        irq_handler_table[vector].cookie  = cookie;

        void* stub = irq_stub_table[vector];

        set_idt_gate(stub, vector, IDT_TA_InterruptGate, 0x08);

        return true;
    }

    extern "C" void irq_common_stub_handler(uint8_t irqno) {
        if (irqno >= IRQ_MAX) return;

        irq_desc &desc = irq_handler_table[irqno];

        if (desc.handler) {
            desc.handler(desc.cookie);
        }

        apic::send_eoi();
    }

    extern "C" void irq_stub_0x30();
    void load_default_idt() {
        void *idt_page = kernel::memory::request_page();
        memset(idt_page, 0, 0x1000);

        idtr.limit = 0x0FFF;
        idtr.offset = reinterpret_cast<uint64_t>(idt_page);

        // Standard Exception Handlers
        set_idt_gate((void*)isr_divide_error, 0x00, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_invalid_opcode, 0x06, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_double_fault, 0x08, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_segment_not_present, 0x0B, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_stack_fault, 0x0C, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_gp_fault, 0x0D, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_page_fault, 0x0E, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_machine_check, 0x12, IDT_TA_InterruptGate, 0x08);

        set_idt_gate((void*)isr_keyboard_int, 0x21, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_mouse_int, 0x22, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_apic_timer_int, IRQ_TIMER, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_spurious_int, IRQ_SPURIOUS, IDT_TA_InterruptGate, 0x08);
        set_idt_gate((void*)isr_panic_ipi, IRQ_PANIC, IDT_TA_InterruptGate, 0x08);

        asm ("lidt %0" : : "m" (idtr));
        asm ("cli");
    }

    IDTR *get_idtr_address() {
        return &idtr;
    }
}
