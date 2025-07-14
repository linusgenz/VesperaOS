#include "interrupts.h"
#include "../../../kernel/utils/panic.h"
#include "../../../drivers/io/io.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "apic.h"
#include "../../../include/log.h"

__attribute__((interrupt)) void page_fault_handler(interrupt_frame* frame) {
    // Hole CR2 Register (Page Fault Address)
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    // Hole Error Code
    uint64_t error_code = frame->error_code;
    
    Log::Error("PAGE FAULT: addr=0x%llx, error=0x%llx, rip=0x%llx", 
               fault_addr, error_code, frame->rip);
    
    // Dekodiere Error Code
    Log::Error("  Present: %s, Write: %s, User: %s, Reserved: %s",
               (error_code & 1) ? "Yes" : "No",
               (error_code & 2) ? "Yes" : "No", 
               (error_code & 4) ? "Yes" : "No",
               (error_code & 8) ? "Yes" : "No");
    
    panic("Page fault detected");
    while (true);
}

__attribute__((interrupt)) void double_fault_handler(interrupt_frame* frame) {
    Log::Error("DOUBLE FAULT: rip=0x%llx, error=0x%llx", 
               frame->rip, frame->error_code);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    panic("Double fault detected");
    while (true);
}

__attribute__((interrupt)) void gp_fault_handler(interrupt_frame* frame) {
    Log::Error("GENERAL PROTECTION FAULT: rip=0x%llx, error=0x%llx", 
               frame->rip, frame->error_code);
    
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    if (frame->error_code & 0x1) {
        Log::Error("  External event caused fault");
    }
    if (frame->error_code & 0x2) {
        Log::Error("  IDT referenced");
    } else if (frame->error_code & 0x4) {
        Log::Error("  LDT referenced");
    } else {
        Log::Error("  GDT referenced");
    }
    
    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Selector: 0x%x", selector);
    
    panic("General protection fault detected");
    while (true);
}

// Invalid Opcode Fault (Vector 6)
__attribute__((interrupt)) void invalid_opcode_handler(interrupt_frame* frame) {
    Log::Error("INVALID OPCODE: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    // Zeige die Bytes an der fehlerhaften Adresse
    uint8_t* opcode_ptr = (uint8_t*)frame->rip;
    Log::Error("  Opcode bytes: %02x %02x %02x %02x", 
               opcode_ptr[0], opcode_ptr[1], opcode_ptr[2], opcode_ptr[3]);
    
    panic("Invalid opcode detected");
    while (true);
}

// Stack Segment Fault (Vector 12)
__attribute__((interrupt)) void stack_fault_handler(interrupt_frame* frame) {
    Log::Error("STACK FAULT: rip=0x%llx, error=0x%llx", 
               frame->rip, frame->error_code);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Stack selector: 0x%x", selector);
    
    panic("Stack fault detected");
    while (true);
}

// Segment Not Present (Vector 11)
__attribute__((interrupt)) void segment_not_present_handler(interrupt_frame* frame) {
    Log::Error("SEGMENT NOT PRESENT: rip=0x%llx, error=0x%llx", 
               frame->rip, frame->error_code);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Missing segment selector: 0x%x", selector);
    
    if (frame->error_code & 0x2) {
        Log::Error("  IDT referenced");
    } else if (frame->error_code & 0x4) {
        Log::Error("  LDT referenced");
    } else {
        Log::Error("  GDT referenced");
    }
    
    panic("Segment not present");
    while (true);
}

// Divide by Zero (Vector 0)
__attribute__((interrupt)) void divide_error_handler(interrupt_frame* frame) {
    Log::Error("DIVIDE BY ZERO: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    panic("Divide by zero");
    while (true);
}

// Machine Check Exception (Vector 18)
__attribute__((interrupt)) void machine_check_handler(interrupt_frame* frame) {
    Log::Error("MACHINE CHECK EXCEPTION: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    panic("Machine check exception");
    while (true);
}

// Generic unhandled interrupt handler
__attribute__((interrupt)) void unhandled_interrupt_handler(interrupt_frame* frame) {
    Log::Error("UNHANDLED INTERRUPT: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    
    panic("Unhandled interrupt");
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
    lapic_eoi();
}

__attribute__((interrupt))
void ap_entry_int_handler(interrupt_frame* frame) {
    lapic_eoi();

 //   lapic_init_ap();

  /*  uint32_t apic_id = LocalApicGetId();

    for (uint32_t i = 0; i < CPUManager::total_cpus; i++) {
        if (CPUManager::cpu_infos[i].apic_id == apic_id) {
            CPUManager::cpu_infos[i].state = CPUManager::CPU_STATE_ONLINE;
            break;
        }
    }
*/
    while (true) {
        asm volatile("hlt");
    }
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