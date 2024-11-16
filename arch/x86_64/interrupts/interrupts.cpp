#include "interrupts.h"
#include "../../../kernel/utils/panic.h"
#include "../../../drivers/io/io.h"
#include "../../../drivers/input/keyboard.h"
#include "../../../kernel/scheduling/pit/pit.h"

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
    handle_keyboard(scancode);
    pic_end_master();
}

__attribute__((interrupt)) void mouse_int_handler(interrupt_frame* frame) {
    uint8_t mouse_data = inb(0x60);
    handle_ps2_mouse(mouse_data);
    pic_end_slave();
}

__attribute__((interrupt)) void pit_int_handler(interrupt_frame* frame) {
    PIT::tick();
    pic_end_master();
}

__attribute__((interrupt)) void cursor_int_handler(interrupt_frame* frame) {
    global_renderer->cursor_visible = !global_renderer->cursor_visible;

    if (global_renderer->cursor_visible) {
        global_renderer->draw_cursor();
    } else {
        global_renderer->clear_cursor();
    }
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