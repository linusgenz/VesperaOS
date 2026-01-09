//
// Created by linus on 04.10.24.
//

#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H
#include <boot.h>


class ScreenRendererTTYOutput;

extern uint64_t kernel_phys_start;
extern uint64_t kernel_phys_end;

extern Framebuffer *TargetFramebuffer;

inline ScreenRendererTTYOutput* renderer_tty_out = nullptr;

void initialize_kernel(BootInfo* boot_info);

#endif //KERNEL_UTILS_H