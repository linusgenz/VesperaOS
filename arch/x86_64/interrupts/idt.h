//
// Created by linus on 04.10.24.
//

#ifndef IDT_H
#define IDT_H
#include <stdint.h>

typedef int irqreturn_t;
constexpr irqreturn_t IRQ_HANDLED = 1;
using irq_handler_t = irqreturn_t (*)(void *cookie);

namespace arch::x86_64::interrupts::idt {

#define IDT_TA_InterruptGate 0b10001110
#define IDT_TA_CallGate 0b10001100
#define IDT_TA_TrapGate 0b10001111

    struct IDTDescEntry {
        uint16_t offset0;
        uint16_t selector;
        uint8_t ist;
        uint8_t type_attr;
        uint16_t offset1;
        uint32_t offset2;
        uint32_t ignore;

        void set_offset(uint64_t offset);

        uint64_t get_offset();
    };

    struct IDTR {
        uint16_t limit;
        uint64_t offset;
    }__attribute((packed));

    struct irq_desc {
        irq_handler_t handler = nullptr;
        void *cookie = nullptr;
    };

    constexpr int IRQ_MAX = 256;
    inline irq_desc irq_handler_table[IRQ_MAX];

    inline IDTR idtr;
    IDTR* get_idtr_address();

    void load_default_idt();
    void set_idt_gate(void *handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector);

    bool register_irq_handler(uint8_t irqno, irq_handler_t handler, void *cookie);

    extern "C" void irq_common_stub(uint8_t irqno);
}
#endif //IDT_H
