#include "paging.h"

#include <kernel/memory.h>

#include "kernel/addr.h"

void PageDirectoryEntry::set_flag(PT_Flag flag, bool enabled) {
    uint64_t bit_selector = static_cast<uint64_t>(1) << flag;
    Value &= ~bit_selector;
    if (enabled) {
        Value |= bit_selector;
    }
}

bool PageDirectoryEntry::get_flag(PT_Flag flag) const {
    return (Value & (1ULL << flag)) != 0;
}

phys_addr_t PageDirectoryEntry::get_address() const {
    return make_phys((Value & 0x000ffffffffff000));
}

void PageDirectoryEntry::set_address(phys_addr_t phys_addr) {
    uint64_t raw = phys_raw(phys_addr) >> 12;
    raw &= 0x000000ffffffffff;
    Value &= 0xfff0000000000fff;
    Value |= (raw << 12);
}