#include "idt.h"

#include <kernel/memory.h>
#include <log.h>

#include "apic.h"
#include "interrupts_internal.h"

namespace arch::x86_64::interrupts::idt {
    void IDTDescEntry::set_offset(uint64_t offset) {
        offset0 = static_cast<uint16_t>(offset & 0x000000000000ffff);
        offset1 = static_cast<uint16_t>((offset & 0x00000000ffff0000) >> 16);
        offset2 = static_cast<uint32_t>((offset & 0xffffffff00000000) >> 32);
    }

    uint64_t IDTDescEntry::get_offset() const {
        uint64_t offset = 0;
        offset |= static_cast<uint64_t>(offset0);
        offset |= static_cast<uint64_t>(offset1) << 16;
        offset |= static_cast<uint64_t>(offset2) << 32;
        return offset;
    }

    void set_idt_gate(ISRHandler handler, uint8_t entry_offset, uint8_t type_attr, uint8_t selector) {
        auto* interrupt = reinterpret_cast<IDTDescEntry*>(idtr.offset + entry_offset * sizeof(IDTDescEntry));
        interrupt->set_offset(reinterpret_cast<uint64_t>(handler));
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

    uint8_t get_free_vector_block(size_t size) {
        if (size == 0 || size > (VECTOR_MAX - VECTOR_MIN + 1)) return 0xFF;

        for (uint16_t vec = VECTOR_MIN; vec + size - 1 <= VECTOR_MAX; ++vec) {
            bool block_free = true;

            for (size_t i = 0; i < size; ++i) {
                if (!irq_handler_table[vec + i].free) {
                    block_free = false;
                    vec += i;
                    break;
                }
            }

            if (block_free) {
                for (size_t i = 0; i < size; ++i) {
                    irq_handler_table[vec + i].free = false;
                }
                return static_cast<uint8_t>(vec);
            }
        }

        return 0xFF;
    }

    uint8_t get_free_vector() {
        for (uint16_t vec = VECTOR_MIN; vec <= VECTOR_MAX; ++vec) {
            if (irq_handler_table[vec].free) {
                irq_handler_table[vec].free = false;
                return static_cast<uint8_t>(vec);
            }
        }
        return 0xFF;  // no vector available
    }

    void free_vector(const uint8_t vec) {
        irq_handler_table[vec].handler = nullptr;
        irq_handler_table[vec].cookie = nullptr;
        irq_handler_table[vec].free = true;
    }

    extern "C" ISRHandler irq_stub_table[];  // irq_stub.asm
    bool allocate_vector(uint8_t vector, const irq_handler_t handler, void* cookie) {
        irq_handler_table[vector].handler = handler;
        irq_handler_table[vector].cookie = cookie;

        ISRHandler stub = irq_stub_table[vector];

        set_idt_gate(stub, vector, IDT_TA_InterruptGate, 0x08);

        return true;
    }

    extern "C" void irq_common_stub_handler(uint8_t irqno) {
        if (const irq_desc& desc = irq_handler_table[irqno]; desc.handler) {
            desc.handler(desc.cookie);
        }

        apic::send_eoi();
    }

    extern "C" void irq_stub_0x30();

    void load_default_idt() {
        void* idt_virt = kernel::memory::request_page();

        memset(idt_virt, 0, 0x1000);

        idtr.limit = 0x0FFF;
        idtr.offset = reinterpret_cast<uint64_t>(idt_virt);

        // Standard Exception Handlers
        set_idt_gate(isr_divide_error, 0x00, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_invalid_opcode, 0x06, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_double_fault, 0x08, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_segment_not_present, 0x0B, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_stack_fault, 0x0C, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_gp_fault, 0x0D, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_page_fault, 0x0E, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_machine_check, 0x12, IDT_TA_InterruptGate, 0x08);

        set_idt_gate(isr_keyboard_int, 0x21, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_mouse_int, 0x22, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_apic_timer_int, IRQ_TIMER, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_spurious_int, IRQ_SPURIOUS, IDT_TA_InterruptGate, 0x08);
        set_idt_gate(isr_panic_ipi, IRQ_PANIC, IDT_TA_InterruptGate, 0x08);

        Log::debug("Loading IDT: %x page-offset: %p", idtr, idt_virt);
        asm("lidt %0" : : "m"(idtr));
        asm("cli");
    }

    IDTR* get_idtr_address() {
        return &idtr;
    }
}  // namespace arch::x86_64::interrupts::idt
