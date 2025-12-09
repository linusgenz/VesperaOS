//
// Created by linus on 21.09.24.
//

#ifndef STRING_H
#define STRING_H
#include <cstdint>
#include <cstddef>

constexpr size_t hex_buffer_size(const size_t bytes) {
    return bytes * 2 + 1;
}

constexpr size_t HEX_BUFFER_PTR  = hex_buffer_size(sizeof(void*));    // 17 (if 64 bit)
constexpr size_t HEX_BUFFER_U64  = hex_buffer_size(sizeof(uint64_t)); // 17
constexpr size_t HEX_BUFFER_U32  = hex_buffer_size(sizeof(uint32_t)); // 9
constexpr size_t HEX_BUFFER_U16  = hex_buffer_size(sizeof(uint16_t)); // 5
constexpr size_t HEX_BUFFER_U8   = hex_buffer_size(sizeof(uint8_t));  // 3

char* u64tohex(uint64_t value, char* buffer, size_t buffer_size);
char* u32tohex(uint32_t value, char* buffer, size_t buffer_size);
char* u16tohex(uint16_t value, char* buffer, size_t buffer_size);
char* u8tohex(uint8_t value, char* buffer, size_t buffer_size);

size_t strlen(const char *s);
int strcmp(const char* a, const char* b);
int strncmp(const char *a, const char *b, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
char *strcpy(char *dest, const char *src);
void replace_char(char* s, char old_char, char new_char);
char* strdup(const char* src);
char *strrchr (const char *s, int c);
char* strncat(char* dest, const char* src, size_t max);
char* strtok(char* s, char delim);
int strcasecmp(const char* s1, const char* s2);
char to_upper(char c);
char* strchr(const char* s, unsigned char c);
char *strcat (char *dst, const char *src);
int snprintf(char *buffer, size_t size, const char *format, ...);


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