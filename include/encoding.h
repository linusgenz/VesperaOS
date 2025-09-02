//
// Created by linus on 03.07.25.
//

#ifndef ENCODING_H
#define ENCODING_H
#include <cstdint>

typedef uint16_t utf16_t;
typedef uint8_t utf8_t;
typedef uint32_t codepoint_t;

codepoint_t decode_utf16(utf16_t *utf16, size_t len, size_t *index);

size_t encode_utf8(codepoint_t cp, utf8_t *utf8, size_t utf8_len, size_t index);

size_t utf16_to_utf8(utf16_t *utf16, size_t utf16_len,
                     utf8_t *utf8, size_t utf8_len);

size_t utf16_to_utf8(utf16_t *in, size_t in_len,
                             char *out, size_t out_len);

template <size_t N>
size_t utf16_to_utf8(utf16_t* in, size_t in_len, char (&out)[N]) {
    return utf16_to_utf8(in, in_len, out, N);
}

template <size_t InSize, size_t OutSize>
size_t utf16_to_utf8(utf16_t (&utf16)[InSize], char (&out)[OutSize]) {
    return utf16_to_utf8(utf16, InSize, out, OutSize);
}

#endif //ENCODING_H