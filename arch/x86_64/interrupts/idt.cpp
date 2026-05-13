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

    void set_idt_gate(isr_handler_t handler, const u8 entry_offset, const u8 type_attr, const u8 selector, const u8 ist) {
        auto* interrupt = reinterpret_cast<IDT_DESC_ENTRY*>(idtr.offset + entry_offset * sizeof(IDT_DESC_ENTRY));
        interrupt->set_offset(reinterpret_cast<u64>(handler));
        interrupt->selector = selector;
        interrupt->ist = ist;
        interrupt->type_attr = type_attr;
        interrupt->ignore = 0;
    }

    void init_irq_table() {
        memset(idt::irq_handler_table, 0, sizeof(idt::irq_handler_table));

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

    bool allocate_vector(const u8 vector, const irq_handler_t handler, void* cookie) {
        irq_handler_table[vector].handler = handler;
        irq_handler_table[vector].cookie = cookie;

        return true;
    }

    void load_default_idt() {
        virt_addr_t page = kernel::memory::request_page();
        memset(page, 0, 0x1000);

        idtr.limit  = 0x0FFF;
        idtr.offset = virt_raw(page);

        for (u16 v = 0; v < 256; ++v) {
            u8 type  = IDT_TA_INTERRUPT_GATE;
            u8 ist   = 0;

            if (v == 0x08) ist = 1;
            if (v == 0x02) ist = 2;
            if (v == 0x12) ist = 3;

            set_idt_gate(isr_stub_table[v], static_cast<u8>(v), type, 0x08, ist);
        }

        asm volatile("lidt %0" : : "m"(idtr));
        asm volatile("cli");
    }

    IDTR* get_idtr_address() {
        return &idtr;
    }
}  // namespace arch::x86_64::interrupts::idt
