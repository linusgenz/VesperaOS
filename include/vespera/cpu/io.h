//
// Created by linus on 05.10.24.
//

#ifndef IO_H
#define IO_H
#include <vespera/types.h>

inline void outw(u16 port, u16 value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

inline u16 inw(u16 port) {
    u16 ret = 0;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void outb(u16 port, u8 value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline u8 inb(u16 port) {
    u8 ret = 0;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

inline void io_wait() {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}

inline u32 inl(u16 port) {
    u32 ret = 0;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

inline void outl(u16 port, u32 value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

#endif  // IO_H
