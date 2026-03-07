//
// Created by linus on 03.07.25.
//
#include <stddef.h>

#include <klib/encoding.h>

// === Konstanten & Masken ===

#define BMP_END                  0xFFFF
#define UNICODE_MAX              0x10FFFF
#define INVALID_CODEPOINT        0xFFFD

#define GENERIC_SURROGATE_VALUE  0xD800
#define GENERIC_SURROGATE_MASK   0xF800

#define HIGH_SURROGATE_VALUE     0xD800
#define LOW_SURROGATE_VALUE      0xDC00
#define SURROGATE_MASK           0xFC00

#define SURROGATE_CODEPOINT_OFFSET 0x10000
#define SURROGATE_CODEPOINT_MASK   0x03FF
#define SURROGATE_CODEPOINT_BITS   10

#define UTF8_1_MAX               0x7F
#define UTF8_2_MAX               0x7FF
#define UTF8_3_MAX               0xFFFF
#define UTF8_4_MAX               0x10FFFF

#define UTF8_CONTINUATION_VALUE     0x80
#define UTF8_CONTINUATION_MASK      0xC0
#define UTF8_CONTINUATION_CODEPOINT_BITS 6

typedef struct {
    utf8_t mask;
    utf8_t value;
} utf8_pattern_t;

static constexpr utf8_pattern_t UTF8_LEADING_BYTES[] = {
    { 0x80, 0x00 }, // 0xxxxxxx
    { 0xE0, 0xC0 }, // 110xxxxx
    { 0xF0, 0xE0 }, // 1110xxxx
    { 0xF8, 0xF0 }  // 11110xxx
};



static size_t calculate_utf8_len(codepoint_t cp) {
    if (cp <= UTF8_1_MAX)  return 1;
    if (cp <= UTF8_2_MAX)  return 2;
    if (cp <= UTF8_3_MAX)  return 3;
    if (cp <= UTF8_4_MAX)  return 4;
    return 0;
}

// utf16 -> codepoint

codepoint_t decode_utf16(const utf16_t* utf16, size_t len, size_t* index) {
    utf16_t high = utf16[*index];

    if ((high & GENERIC_SURROGATE_MASK) != GENERIC_SURROGATE_VALUE)
        return high;

    if ((high & SURROGATE_MASK) != HIGH_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    if (*index == len - 1)
        return INVALID_CODEPOINT;

    utf16_t low = utf16[*index + 1];

    if ((low & SURROGATE_MASK) != LOW_SURROGATE_VALUE)
        return INVALID_CODEPOINT;

    (*index)++;

    codepoint_t result = high & SURROGATE_CODEPOINT_MASK;
    result <<= SURROGATE_CODEPOINT_BITS;
    result |= low & SURROGATE_CODEPOINT_MASK;
    result += SURROGATE_CODEPOINT_OFFSET;

    return result;
}

// codepoint -> utf8

size_t encode_utf8(codepoint_t cp, utf8_t* utf8, size_t utf8_len, size_t index) {
    int size = calculate_utf8_len(cp);
    if (size == 0 || index + size > utf8_len)
        return 0;

    for (int i = size - 1; i > 0; i--) {
        utf8_t cont = (cp & ((1 << UTF8_CONTINUATION_CODEPOINT_BITS) - 1)) | UTF8_CONTINUATION_VALUE;
        utf8[index + i] = cont;
        cp >>= UTF8_CONTINUATION_CODEPOINT_BITS;
    }

    utf8_pattern_t pattern = UTF8_LEADING_BYTES[size - 1];
    utf8_t lead = (cp & ~pattern.mask) | pattern.value;
    utf8[index] = lead;

    return size;
}

// utf16 buf -> utf8 buf

size_t utf16_to_utf8(const utf16_t *utf16, size_t utf16_len,
                     utf8_t *utf8, size_t utf8_len) {
    size_t utf16_index = 0;
    size_t utf8_index = 0;

    while (utf16_index < utf16_len) {
        size_t old_index = utf16_index;
        codepoint_t cp = decode_utf16(utf16, utf16_len, &utf16_index);

        if (cp > 0x7F) { // ascii range
            cp = '?';
        }

        size_t needed = calculate_utf8_len(cp);
        if (utf8_index + needed > utf8_len)
            break;

        encode_utf8(cp, utf8, utf8_len, utf8_index);
        utf8_index += needed;

        utf16_index++;
        if (utf16_index == old_index)
            utf16_index++;
    }

    return utf8_index;
}

size_t utf16_to_utf8(const utf16_t* in, size_t in_len,
                             char* out, size_t out_len) {
    if (out_len == 0) return 0;
    const size_t written = utf16_to_utf8(in, in_len, reinterpret_cast<utf8_t*>(out), out_len - 1);
    out[written] = '\0';
    return written;
}