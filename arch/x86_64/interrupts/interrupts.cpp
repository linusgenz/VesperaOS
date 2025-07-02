#include "interrupts.h"
#include "../../../kernel/utils/panic.h"
#include "../../../drivers/io/io.h"
#include "../../../drivers/input/keyboard.h"
#include "../../../kernel/scheduling/pit/pit.h"
#include "apic.h"

__attribute__((interrupt)) void page_fault_handler(interrupt_frame* frame) {
    panic("Page fault detected");
    while (true);
}

__attribute__((interrupt)) void double_fault_handler(interrupt_frame* frame) {
    panic("Double fault detected");
    while (true);
}

__attribute__((interrupt)) void gp_fault_handler(interrupt_frame* frame) {
    panic("General protection fault detected");
    while (true);
}

__attribute__((interrupt)) void keyboard_int_handler(interrupt_frame* frame) {
    uint8_t scancode = inb(0x60);
   // handle_keyboard(scancode);
    pic_end_master();
}

__attribute__((interrupt)) void mouse_int_handler(interrupt_frame* frame) {
    uint8_t mouse_data = inb(0x60);
  //  handle_ps2_mouse(mouse_data);
    pic_end_slave();
}

__attribute__((interrupt))
void apic_timer_int_handler(interrupt_frame* frame) {
    apic_timer_tick();
    lapic_eoi();
}

__attribute__((interrupt))
void spurious_int_handler(interrupt_frame* frame) {
    global_renderer->print("Spurious interrupt received");
    lapic_eoi();
}


__attribute__((interrupt)) void cursor_int_handler(interrupt_frame* frame) {
    /*global_renderer->cursor_visible = !global_renderer->cursor_visible;

    if (global_renderer->cursor_visible) {
        global_renderer->draw_cursor();
    } else {
        global_renderer->clear_cursor();
    }*/
}

void pic_init()
{
    // ICW1: start initialization, ICW4 needed
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    // ICW2: interrupt vector address
    outb(PIC1_DATA, IRQ_BASE);
    outb(PIC2_DATA, IRQ_BASE + 8);

    // ICW3: master/slave wiring
    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    // ICW4: 8086 mode, not special fully nested, not buffered, normal EOI
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    // OCW1: Disable all IRQs
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

void pic_end_master() {
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_end_slave() {
    outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void remap_pic() {
    uint8_t a1, a2;

    a1 = inb(PIC1_DATA);
    io_wait();
    a2 = inb(PIC2_DATA);
    io_wait();

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, a1);
    io_wait();
    outb(PIC2_DATA, a2);
}