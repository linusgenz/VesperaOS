//
// Created by linus on 04.10.24.
//

#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H
#include <vespera/boot/boot.h>

extern u64 kernel_start;
extern u64 kernel_end;

extern Framebuffer *target_framebuffer;

void initialize_kernel(BootInfo* boot_info);

#endif //KERNEL_UTILS_H