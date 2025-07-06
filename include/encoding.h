//
// Created by linus on 03.07.25.
//

#ifndef ENCODING_H
#define ENCODING_H
#include <stdint.h>

typedef uint16_t utf16_t;
typedef uint8_t  utf8_t;
typedef uint32_t codepoint_t;

codepoint_t decode_utf16(const utf16_t* utf16, size_t len, size_t* index);
size_t encode_utf8(codepoint_t cp, utf8_t* utf8, size_t utf8_len, size_t index);
size_t convert_utf16_to_utf8(const utf16_t* utf16, size_t utf16_len,
                             utf8_t* utf8, size_t utf8_len);

#endif //ENCODING_H
