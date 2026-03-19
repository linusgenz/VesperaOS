#include "page_map_indexer.h"

PageMapIndexer::PageMapIndexer(const virt_addr_t virt_addr) {
    u64 addr = virt_raw(virt_addr);
    addr >>= 12;
    p_i   = addr & 0x1ff;
    addr >>= 9;
    pt_i  = addr & 0x1ff;
    addr >>= 9;
    pd_i  = addr & 0x1ff;
    addr >>= 9;
    pdp_i = addr & 0x1ff;
}