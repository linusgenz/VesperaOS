#include "idt.h"

#include "apic.h"
#include "interrupts_internal.h"
#include <vespera/mm/memory.h>
#include <klib/string.h>

namespace arch::x86_64::interrupts::idt {
    void IDT_DESC_ENTRY::set_offset(const u64 offset) {
        offset0 = static_cast<u16>(offset & 0x000000000000ffff);
        offset1 = static_cast<u16>((offset & 0x00000000ffff0000) >> 16);
        offset2 = static_cast<u32>((offset & 0xffffffff00000000) >> 32);
    }

    u64 IDT_DESC_ENTRY::get_offset() const {
        u64 offset = 0;
        offset |= static_cast<u64>(offset0);
        offset |= static_cast<u64>(offset1) << 16;
        offset |= static_cast<u64>(offset2) << 32;
        return offset;
    }

    void set_idt_gate(isr_handler_t handler, const u8 entry_offset, const u8 type_attr, const u8 selector) {
        auto* interrupt = reinterpret_cast<IDT_DESC_ENTRY*>(idtr.offset + entry_offset * sizeof(IDT_DESC_ENTRY));
        interrupt->set_offset(reinterpret_cast<u64>(handler));
        interrupt->selector = selector;
        interrupt->ist = 0;
        interrupt->type_attr = type_attr;
        interrupt->ignore = 0;
    }

    void init_irq_table() {
        for (auto& [handler, cookie, free] : irq_handler_table) {
            handler = nullptr;
            cookie = nullptr;
            free = true;
        }
    }

    u8 get_free_vector_block(const usize size) {
        if (size == 0 || size > (VECTOR_MAX - VECTOR_MIN + 1)) return 0xFF;

        for (u16 vec = VECTOR_MIN; vec + size - 1 <= VECTOR_MAX; ++vec) {
            bool block_free = true;

            for (usize i = 0; i < size; ++i) {
                if (!irq_handler_table[vec + i].free) {
                    block_free = false;
                    vec += i;
                    break;
                }
            }

            if (block_free) {
                for (usize i = 0; i < size; ++i) {
                    irq_handler_table[vec + i].free = false;
                }
                return static_cast<u8>(vec);
            }
        }

        return 0xFF;
    }

    u8 get_free_vector() {
        for (u16 vec = VECTOR_MIN; vec <= VECTOR_MAX; ++vec) {
            if (irq_handler_table[vec].free) {
                irq_handler_table[vec].free = false;
                return static_cast<u8>(vec);
            }
        }
        return 0xFF;  // no vector available
    }

    void free_vector(const u8 vec) {
        irq_handler_table[vec].handler = nullptr;
        irq_handler_table[vec].cookie = nullptr;
        irq_handler_table[vec].free = true;
    }

    extern "C" isr_handler_t irq_stub_table[];  // irq_stub.asm
    bool allocate_vector(const u8 vector, const irq_handler_t handler, void* cookie) {
        irq_handler_table[vector].handler = handler;
        irq_handler_table[vector].cookie = cookie;

        const isr_handler_t stub = irq_stub_table[vector];

        set_idt_gate(stub, vector, IDT_TA_INTERRUPT_GATE, 0x08);

        return true;
    }

    extern "C" void irq_common_stub_handler(const u8 irqno) {
        if (const IrqDesc& desc = irq_handler_table[irqno]; desc.handler) {
            desc.handler(desc.cookie);
        }

        apic::send_eoi();
    }

    void load_default_idt() {
        virt_addr_t idt_virt = kernel::memory::request_page();

        memset(idt_virt, 0, 0x1000);

        idtr.limit = 0x0FFF;
        idtr.offset = virt_raw(idt_virt);

        // Standard Exception Handlers
        set_idt_gate(isr_divide_error, 0x00, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_invalid_opcode, 0x06, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_double_fault, 0x08, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_segment_not_present, 0x0B, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_stack_fault, 0x0C, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_gp_fault, 0x0D, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_page_fault, 0x0E, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_machine_check, 0x12, IDT_TA_INTERRUPT_GATE, 0x08);

        set_idt_gate(isr_keyboard_int, 0x21, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_mouse_int, 0x22, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_apic_timer_int, IRQ_TIMER, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_spurious_int, IRQ_SPURIOUS, IDT_TA_INTERRUPT_GATE, 0x08);
        set_idt_gate(isr_panic_ipi, IRQ_PANIC, IDT_TA_INTERRUPT_GATE, 0x08);

        asm("lidt %0" : : "m"(idtr));
        asm("cli");
    }

    IDTR* get_idtr_address() {
        return &idtr;
    }
}  // namespace arch::x86_64::interrupts::idt
