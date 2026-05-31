#include "paging.h"

#include <vespera/mm/memory.h>

#include "vespera/mm/addr.h"

void PageDirectoryEntry::set_flag(const PtFlag flag, const bool enabled) {
    const u64 bit_selector = static_cast<u64>(1) << flag;
    value &= ~bit_selector;
    if (enabled) {
        value |= bit_selector;
    }
}

bool PageDirectoryEntry::get_flag(const PtFlag flag) const {
    return (value & (1ULL << flag)) != 0;
}

phys_addr_t PageDirectoryEntry::get_address() const {
    return make_phys((value & 0x000ffffffffff000));
}

void PageDirectoryEntry::set_address(const phys_addr_t phys_addr) {
    u64 raw = (phys_raw(phys_addr) >> 12) & 0x000000ffffffffffULL;
    u64 old, desired;
    do {
        old = __atomic_load_n(&value, __ATOMIC_RELAXED);
        desired = (old & 0xfff0000000000fffULL) | (raw << 12);
    } while (!__atomic_compare_exchange_n(&value, &old, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
}