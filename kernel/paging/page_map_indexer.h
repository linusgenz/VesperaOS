//
// Created by linus on 03.10.24.
//
#ifndef PAGE_MAP_INDEXER_H
#define PAGE_MAP_INDEXER_H
#include <vespera/types.h>

#include <vespera/mm/addr.h>
class PageMapIndexer {
   private:
   public:
    explicit PageMapIndexer(virt_addr_t virt_addr);
    u64 pdp_i;
    u64 pd_i;
    u64 pt_i;
    u64 p_i;
};

#endif  // PAGE_MAP_INDEXER_H