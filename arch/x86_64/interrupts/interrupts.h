//
// Created by linus on 05.10.24.
//

#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>
#include "../../../kernel/include/basic_renderer.h"
#include "../../../drivers/input/mouse.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

struct interrupt_frame;
__attribute__((interrupt)) void page_fault_handler(interrupt_frame* frame);
__attribute__((interrupt)) void double_fault_handler(interrupt_frame* frame);
__attribute__((interrupt)) void gp_fault_handler(interrupt_frame* frame); // General protection fault
__attribute__((interrupt)) void keyboard_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void mouse_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void apic_timer_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void spurious_int_handler(interrupt_frame* frame);

void pic_init();
void remap_pic();
void pic_end_master();
void pic_end_slave();
void pic_disable();

#endif //INTERRUPTS_H