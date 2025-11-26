//
// Created by linus on 21.09.24.
//

#ifndef STRING_H
#define STRING_H
#include <cstdint>
#include <cstddef>

const char* to_string(uint64_t value);
const char* to_string(uint8_t value);
const char* to_string(int64_t value);
const char* to_string(uint16_t value);
const char* to_string(uint32_t value);
const char* to_hstring(void* ptr);
const char* to_hstring(uint64_t value);
const char* to_hstring(uint32_t value);
const char* to_hstring(uint16_t value);
const char* to_hstring(uint8_t value);
const char* to_string(double value, uint8_t decimalPlaces);
const char* to_string(double value);

size_t strlen(const char *str);
int strcmp(const char* a, const char* b);
int strncmp(const char *a, const char *b, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
char *strcpy(char *dest, const char *src);
char* strdup(const char* src);
char *strrchr (const char *s, int c);
char* strncat(char* dest, const char* src, size_t max);
char* strtok(char* str, char delim);
int strcasecmp(const char* s1, const char* s2);
char to_upper(char c);
char *strchr(const char *s, unsigned char c);
char *strcat (char *dst, const char *src);
int snprintf(char *buffer, size_t size, const char *format, ...);


static inline int isdigit(int c) {
    return (c >= '0' && c <= '9');
}

static inline int isxdigit(int c) {
    return ( (c >= '0' && c <= '9') ||
             (c >= 'a' && c <= 'f') ||
             (c >= 'A' && c <= 'F') );
}

static inline int tolower(int c) {
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}


#endif //STRING_H