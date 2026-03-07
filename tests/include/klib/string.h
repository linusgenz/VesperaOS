#pragma once
#define STRING_H


extern "C" {
#include "/usr/include/string.h"
#include "/usr/include/stdio.h"
}

#include <stdint.h>
#include <stddef.h>

int strcmp(const char* a, const char* b);

// Kernel-eigene Extras
constexpr size_t hex_buffer_size(const size_t bytes) { return bytes * 2 + 1; }
constexpr size_t HEX_BUFFER_PTR = hex_buffer_size(sizeof(void*));
constexpr size_t HEX_BUFFER_U64 = hex_buffer_size(sizeof(uint64_t));
constexpr size_t HEX_BUFFER_U32 = hex_buffer_size(sizeof(uint32_t));
constexpr size_t HEX_BUFFER_U16 = hex_buffer_size(sizeof(uint16_t));
constexpr size_t HEX_BUFFER_U8  = hex_buffer_size(sizeof(uint8_t));

inline char* u64_tohex(uint64_t, char* b, size_t) { return b; }
inline char* u32_tohex(uint32_t, char* b, size_t) { return b; }
inline char* u16_tohex(uint16_t, char* b, size_t) { return b; }
inline char* u8_tohex(uint8_t,   char* b, size_t) { return b; }
inline void  replace_char(char* s, char o, char n) {
    for (; *s; s++) if (*s == o) *s = n;
}
inline char to_upper(char c) { return (c>='a'&&c<='z') ? c-32 : c; }