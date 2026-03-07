//
// Created by linus on 04.10.24.
//

#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H
#include <vespera/boot/boot.h>

extern uint64_t kernel_start;
extern uint64_t kernel_end;

extern framebuffer_t *target_framebuffer;

void initialize_kernel(BootInfo* boot_info);

#endif //KERNEL_UTILS_H