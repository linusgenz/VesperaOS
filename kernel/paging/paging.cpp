#include "paging.h"

void PageDirectoryEntry::set_flag(PT_Flag flag, bool enabled) {
    uint64_t bit_selector = (uint64_t)1 << flag;
    Value &= ~bit_selector;
    if (enabled) {
        Value |= bit_selector;
    }
}

bool PageDirectoryEntry::get_flag(PT_Flag flag) {
    uint64_t bit_selector = (uint64_t)1 << flag;
    return Value & bit_selector > 0 ? true : false;
}

uint64_t PageDirectoryEntry::get_address() {
    return (Value & 0x000ffffffffff000) >> 12;
}

void PageDirectoryEntry::set_address(uint64_t address) {
    address &= 0x000000ffffffffff;
    Value &= 0xfff0000000000fff;
    Value |= (address << 12);
}