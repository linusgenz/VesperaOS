//
// Created by linus on 04.10.24.
//

#ifndef IDT_H
#define IDT_H
#include <cstddef>
#include <cstdint>
enum irqreturn_t : int {
    IRQ_HANDLED = 1,
    IRQ_NONE = 0,
    IRQ_ERROR = -1
};
using irq_handler_t = irqreturn_t (*)(void *cookie);

namespace arch::x86_64::interrupts::idt {
    constexpr uint8_t VECTOR_MIN = 0x23;
    constexpr uint8_t VECTOR_MAX = 0xEF;

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

        [[nodiscard]] uint64_t get_offset() const;
    };

    struct IDTR {
        uint16_t limit;
        uint64_t offset;
    } __attribute((packed));

    struct irq_desc {
        irq_handler_t handler = nullptr;
        void *cookie = nullptr;
        bool free = true;
    };

    constexpr int IRQ_MAX = 256;
    inline irq_desc irq_handler_table[IRQ_MAX];

    void init_irq_table();

    inline IDTR idtr;

    IDTR *get_idtr_address();

    void load_default_idt();

    using ISRHandler = void (*)();
    void set_idt_gate(ISRHandler handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector);

    bool allocate_vector(uint8_t vector, irq_handler_t handler, void *cookie);

    void free_vector(uint8_t vec);

    uint8_t get_free_vector_block(size_t size);

    uint8_t get_free_vector();

    extern "C" void irq_common_stub(uint8_t irqno);
}  // namespace arch::x86_64::interrupts::idt
#endif  // IDT_H
