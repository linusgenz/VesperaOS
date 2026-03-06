//
// Created by linus on 03.10.24.
//
#ifndef PAGE_MAP_INDEXER_H
#define PAGE_MAP_INDEXER_H
#include "kernel/addr.h"
#include <stdint.h>
class PageMapIndexer {
   private:
   public:
    explicit PageMapIndexer(virt_addr_t virt_addr);
    uint64_t pdp_i;
    uint64_t pd_i;
    uint64_t pt_i;
    uint64_t p_i;
};

#endif  // PAGE_MAP_INDEXER_H