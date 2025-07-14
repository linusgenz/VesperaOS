//
// Created by linus on 06.07.25.
//

#ifndef TSS_H
#define TSS_H
#include "stdint.h"

#define TSS_SELECTOR 0x30 // (6 << 3)

struct TSS {
    uint32_t rsv0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t rsv1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t rsv2;
    uint16_t rsv3;
    uint16_t io_map_base;
}__attribute__((packed));

#endif //TSS_H
