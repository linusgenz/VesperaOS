//
// Created by linus on 04.10.24.
//

#ifndef KERNEL_UTILS_H
#define KERNEL_UTILS_H
#include <cstddef> // rm
#include <cstdint>
#include "basic_renderer.h"
#include "../../include/string.h"
#include "efi_memory.h"
#include "memory.h"
#include "bitmap.h"
#include "../cpu/io.h"
#include "../acpi/acpi.h"
#include "../../drivers/pci/pci.h"
#include "ScrollManager.h"

struct BootInfo {
	Framebuffer* framebuffer;
	FONT* font;
	EFI_MEMORY_DESCRIPTOR* mMap;
	uint64_t mMapSize;
	uint64_t mMapDescSize;
	ACPI::RSDP2* rsdp;
};

extern uint64_t _KernelStart;
extern uint64_t _KernelEnd;

extern Framebuffer *TargetFramebuffer;

void initialize_kernel(BootInfo* BootInfo);

#endif //KERNEL_UTILS_H