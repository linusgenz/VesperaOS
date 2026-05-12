//
// Created by linus on 04.10.24.
//

#ifndef IDT_H
#define IDT_H

#include <vespera/interrupts.h>
#include <vespera/types.h>

namespace arch::x86_64::interrupts::idt {
    constexpr u8 VECTOR_MIN = 0x24;
    constexpr u8 VECTOR_MAX = 0xEF;

#define IDT_TA_INTERRUPT_GATE 0b10001110
#define IDT_TA_CALL_GATE 0b10001100
#define IDT_TA_TRAP_GATE 0b10001111

    struct IDT_DESC_ENTRY {
        u16 offset0;
        u16 selector;
        u8 ist;
        u8 type_attr;
        u16 offset1;
        u32 offset2;
        u32 ignore;

        void set_offset(u64 offset);

        [[nodiscard]] u64 get_offset() const;
    };

    struct IDTR {
        u16 limit;
        u64 offset;
    } __attribute((packed));

    struct IrqDesc {
        irq_handler_t handler = nullptr;
        void *cookie = nullptr;
        bool free = true;
    };

    constexpr int IRQ_MAX = 256;
    inline IrqDesc irq_handler_table[IRQ_MAX];

    void init_irq_table();

    inline IDTR idtr;

    IDTR *get_idtr_address();

    void load_default_idt();

    using isr_handler_t = void (*)();
    void set_idt_gate(isr_handler_t handler, u8 entry_offset, u8 type_attr, u8 selector, u8 ist = 0);

    bool allocate_vector(u8 vector, irq_handler_t handler, void *cookie);

    void free_vector(u8 vec);

    u8 get_free_vector_block(usize size);

    u8 get_free_vector();

}  // namespace arch::x86_64::interrupts::idt
#endif  // IDT_H
