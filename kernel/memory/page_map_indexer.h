//
// Created by linus on 03.10.24.
//
#ifndef PAGE_MAP_INDEXER_H
#define PAGE_MAP_INDEXER_H
#include <stdint.h>
class PageMapIndexer
{
private:

public:
    PageMapIndexer(uint64_t virtualAddress);
    uint64_t PDP_i;
    uint64_t PD_i;
    uint64_t PT_i;
    uint64_t P_i;
};

#endif // PAGE_MAP_INDEXER_H