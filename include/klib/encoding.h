//
// Created by linus on 03.07.25.
//

#ifndef ENCODING_H
#define ENCODING_H
#include <vespera/types.h>

typedef u16 utf16_t;
typedef u8 utf8_t;
typedef u32 codepoint_t;

codepoint_t decode_utf16(utf16_t *utf16, usize len, usize *index);

usize encode_utf8(codepoint_t cp, utf8_t *utf8, usize utf8_len, usize index);

usize utf16_to_utf8(const utf16_t *utf16, usize utf16_len,
                     utf8_t *utf8, usize utf8_len);

usize utf16_to_utf8(const utf16_t *in, usize in_len,
                             char *out, usize out_len);

template <usize N>
usize utf16_to_utf8(const utf16_t* in, usize in_len, char (&out)[N]) {
    return utf16_to_utf8(in, in_len, out, N);
}

template <usize InSize, usize OutSize>
usize utf16_to_utf8(const utf16_t (&utf16)[InSize], char (&out)[OutSize]) {
    return utf16_to_utf8(utf16, InSize, out, OutSize);
}

#endif //ENCODING_H