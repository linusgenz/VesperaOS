//
// Created by linus on 06.07.25.
//
#include "tss.h"
#include "../../kernel/include/memory.h"
#include "gdt/gdt.h"

static TSS tss __attribute__((aligned(16)));

void tss_init(uint64_t rsp0) {
    memset(&tss, 0, sizeof(tss));
    tss.rsp0 = rsp0;

    tss.io_map_base = sizeof(tss);
  //  gdt_set_tss(&tss);

    asm volatile ("ltr %%ax" : : "a"(TSS_SELECTOR)); // TSS_SELECTOR = GDT offset
}
