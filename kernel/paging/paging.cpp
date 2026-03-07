#include "paging.h"

#include "vespera/mm/addr.h"
#include <vespera/mm/memory.h>

void PageDirectoryEntry::set_flag(PtFlag flag, bool enabled) {
    u64 bit_selector = static_cast<u64>(1) << flag;
    value &= ~bit_selector;
    if (enabled) {
        value |= bit_selector;
    }
}

bool PageDirectoryEntry::get_flag(PtFlag flag) const {
    return (value & (1ULL << flag)) != 0;
}

phys_addr_t PageDirectoryEntry::get_address() const {
    return make_phys((value & 0x000ffffffffff000));
}

void PageDirectoryEntry::set_address(phys_addr_t phys_addr) {
    u64 raw = phys_raw(phys_addr) >> 12;
    raw &= 0x000000ffffffffff;
    value &= 0xfff0000000000fff;
    value |= (raw << 12);
}