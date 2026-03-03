#include "page_map_indexer.h"

PageMapIndexer::PageMapIndexer(virt_addr_t virt_addr) {
    uint64_t addr = virt_raw(virt_addr);
    addr >>= 12;
    P_i   = addr & 0x1ff;
    addr >>= 9;
    PT_i  = addr & 0x1ff;
    addr >>= 9;
    PD_i  = addr & 0x1ff;
    addr >>= 9;
    PDP_i = addr & 0x1ff;
}