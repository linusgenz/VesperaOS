//
// Created by linus on 21.09.24.
//

#ifndef STRING_H
#define STRING_H
#include <vespera/types.h>


constexpr usize hex_buffer_size(const usize bytes) {
    return bytes * 2 + 1;
}

constexpr usize HEX_BUFFER_PTR  = hex_buffer_size(sizeof(void*));    // 17 (if 64 bit)
constexpr usize HEX_BUFFER_U64  = hex_buffer_size(sizeof(u64)); // 17
constexpr usize HEX_BUFFER_U32  = hex_buffer_size(sizeof(u32)); // 9
constexpr usize HEX_BUFFER_U16  = hex_buffer_size(sizeof(u16)); // 5
constexpr usize HEX_BUFFER_U8   = hex_buffer_size(sizeof(u8));  // 3

char* u64_tohex(u64 value, char* buffer, usize buffer_size);
char* u32_tohex(u32 value, char* buffer, usize buffer_size);
char* u16_tohex(u16 value, char* buffer, usize buffer_size);
char* u8_tohex(u8 value, char* buffer, usize buffer_size);

usize strlen(const char *s);
int strcmp(const char* a, const char* b);
int strncmp(const char *a, const char *b, usize n);
char *strncpy(char *dest, const char *src, usize n);
char *strcpy(char *dest, const char *src);
void replace_char(char* s, char old_char, char new_char);
char* strdup(const char* src);
char *strrchr (const char *s, int c);
char* strncat(char* dest, const char* src, usize max);
char* strtok(char* s, char delim);
int strcasecmp(const char* s1, const char* s2);
char to_upper(char c);
char* strchr(const char* s, unsigned char c);
char *strcat (char *dst, const char *src);
int snprintf(char *buffer, usize size, const char *format, ...);


inline int isdigit(const int c) {
    return (c >= '0' && c <= '9');
}

inline int isxdigit(const int c) {
    return ( (c >= '0' && c <= '9') ||
             (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F') );
}

inline int tolower(const int c) {
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}


#endif //STRING_H