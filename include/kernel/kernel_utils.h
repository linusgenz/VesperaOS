//
// Created by linus on 04.10.24.
//

#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H
#include <boot.h>

extern uint64_t _KernelStart;
extern uint64_t _KernelEnd;

extern Framebuffer *TargetFramebuffer;

void initialize_kernel(BootInfo* boot_info);

#endif //KERNEL_UTILS_H