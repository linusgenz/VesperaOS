#include "../include/string.h"
#include <cstddef>
#include "../kernel/include/basic_renderer.h"

char uintTo_StringOutput[128];
const char* to_string(uint64_t value){
    uint8_t size;
    uint64_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        uintTo_StringOutput[size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    uintTo_StringOutput[size - index] = remainder + '0';
    uintTo_StringOutput[size + 1] = 0; 
    return uintTo_StringOutput;
}

char uintTo_StringOutput16[128];
const char* to_string(uint16_t value){
    uint8_t size;
    uint16_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        uintTo_StringOutput16[size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    uintTo_StringOutput16[size - index] = remainder + '0';
    uintTo_StringOutput16[size + 1] = 0; 
    return uintTo_StringOutput16;
}

char uintTo_StringOutput8[128];
const char* to_string(uint8_t value){
    uint8_t size;
    uint16_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        uintTo_StringOutput8[size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    uintTo_StringOutput8[size - index] = remainder + '0';
    uintTo_StringOutput8[size + 1] = 0; 
    return uintTo_StringOutput8;
}

char uintTo_StringOutput32[128];
const char* to_string(uint32_t value){
    uint8_t size;
    uint16_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        uintTo_StringOutput32[size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    uintTo_StringOutput32[size - index] = remainder + '0';
    uintTo_StringOutput32[size + 1] = 0; 
    return uintTo_StringOutput32;
}

char hexTo_StringOutputVoid[128];
const char* to_hstring(void* ptr) {
    uintptr_t value = reinterpret_cast<uintptr_t>(ptr);
    uint8_t size = sizeof(value) * 2;

    for (uint8_t i = 0; i < size; i++) {
        uint8_t nibble = (value >> ((size - i - 1) * 4)) & 0xF;
        hexTo_StringOutputVoid[i] = nibble + (nibble > 9 ? 'A' - 10 : '0');
    }

    hexTo_StringOutputVoid[size] = '\0';
    return hexTo_StringOutputVoid;
}

char hexTo_StringOutput[128];
const char* to_hstring(uint64_t value){
    uint64_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 8 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hexTo_StringOutput[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hexTo_StringOutput[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hexTo_StringOutput[size + 1] = 0;
    return hexTo_StringOutput;
}

char hexTo_StringOutput32[128];
const char* to_hstring(uint32_t value){
    uint32_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 4 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hexTo_StringOutput32[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hexTo_StringOutput32[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hexTo_StringOutput32[size + 1] = 0;
    return hexTo_StringOutput32;
}

char hexTo_StringOutput16[128];
const char* to_hstring(uint16_t value){
    uint16_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 2 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hexTo_StringOutput16[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hexTo_StringOutput16[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hexTo_StringOutput16[size + 1] = 0;
    return hexTo_StringOutput16;
}

char hexTo_StringOutput8[128];
const char* to_hstring(uint8_t value){
    uint8_t* valPtr = &value;
    uint8_t* ptr;
    uint8_t tmp;
    uint8_t size = 1 * 2 - 1;
    for (uint8_t i = 0; i < size; i++){
        ptr = ((uint8_t*)valPtr + i);
        tmp = ((*ptr & 0xF0) >> 4);
        hexTo_StringOutput8[size - (i * 2 + 1)] = tmp + (tmp > 9 ? 55 : '0');
        tmp = ((*ptr & 0x0F));
        hexTo_StringOutput8[size - (i * 2)] = tmp + (tmp > 9 ? 55 : '0');
    }
    hexTo_StringOutput8[size + 1] = 0;
    return hexTo_StringOutput8;
}

char intTo_StringOutput[128];
const char* to_string(int64_t value){
    uint8_t isNegative = 0;

    if (value < 0){
        isNegative = 1;
        value *= -1;
        intTo_StringOutput[0] = '-';
    }

    uint8_t size;
    uint64_t sizeTest = value;
    while (sizeTest / 10 > 0){
        sizeTest /= 10;
        size++;
    }

    uint8_t index = 0;
    while(value / 10 > 0){
        uint8_t remainder = value % 10;
        value /= 10;
        intTo_StringOutput[isNegative + size - index] = remainder + '0';
        index++;
    }
    uint8_t remainder = value % 10;
    intTo_StringOutput[isNegative + size - index] = remainder + '0';
    intTo_StringOutput[isNegative + size + 1] = 0; 
    return intTo_StringOutput;
}

char doubleTo_StringOutput[128];
const char* to_string(double value, uint8_t decimalPlaces){
    if (decimalPlaces > 20) decimalPlaces = 20;

    char* intPtr = (char*)to_string((int64_t)value);
    char* doublePtr = doubleTo_StringOutput;

    if (value < 0){
        value *= -1;
    }

    while(*intPtr != 0){
        *doublePtr = *intPtr;
        intPtr++;
        doublePtr++;
    }

    *doublePtr = '.';
    doublePtr++;

    double newValue = value - (int)value;

    for (uint8_t i = 0; i < decimalPlaces; i++){
        newValue *= 10;
        *doublePtr = (int)newValue + '0';
        newValue -= (int)newValue;
        doublePtr++;
    }

    *doublePtr = 0;
    return doubleTo_StringOutput;
}

const char* to_string(double value){
    return to_string(value, 2);
}

size_t strlen(const char *s) {
    const char *start = s;
    while (*s) {
        ++s;
    }
    return s - start;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n != 0 && *a != '\0' && *b != '\0'  && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : (*a - *b);
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strdup(const char* src) {
    size_t len = strlen(src) + 1;
    char* dst = (char*) kernel::memory::malloc(len);
    if (!dst) return nullptr;
    strncpy(dst, src, len);
    return dst;
}

char *strrchr (const char *s, int c) {
    char *rtnval = 0;

    do {
        if (*s == c)
            rtnval = (char*) s;
    } while (*s++);
    return (rtnval);
}

char *strchr(const char *s, unsigned char c) {
    while (*s) {
        if ((unsigned char)*s == c) {
            return (char *)s;
        }
        ++s;
    }

    if (c == '\0') return (char *)s;
    return nullptr;
}

char* strncat(char* dest, const char* src, size_t max) {
    if (!dest || !src || max == 0) return dest;

    size_t dlen = 0;
    while (dlen < max && dest[dlen] != '\0') {
        dlen++;
    }

    if (dlen == max) return dest;

    size_t i = 0;
    while (i + dlen < max - 1 && src[i] != '\0') {
        dest[dlen + i] = src[i];
        i++;
    }

    dest[dlen + i] = '\0';
    return dest;
}

char *strcat (char *dst, const char *src)
{
    char *p = dst;
    while (*p)
        p++;
    while ((*p++ = *src++))
        ;
    return dst;
}

char* strtok(char* str, char delim) {
    static char* next = NULL;
    if (str) next = str;
    if (!next) return NULL;

    char* start = next;
    while (*next && *next != delim) next++;

    if (*next) {
        *next = '\0';
        next++;
    } else {
        next = NULL;
    }
    return start;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        char c1 = *s1;
        char c2 = *s2;

        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

        if (c1 != c2) return (int)(unsigned char)c1 - (int)(unsigned char)c2;

        s1++;
        s2++;
    }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}


char to_upper(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

// Helper function to reverse a string in place
static void reverse_string(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// Helper function to convert integer to string
static int int_to_string(int num, char *str, int base) {
    int i = 0;
    int is_negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }

    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }

    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (is_negative) {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse_string(str, i);

    return i;
}

static int uint64_to_string(unsigned long long value, char* buffer, int base) {
    char temp[32];
    int pos = 0;

    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    while (value > 0) {
        unsigned digit = value % base;
        temp[pos++] = (digit < 10) ? '0' + digit : 'a' + (digit - 10);
        value /= base;
    }

    // reverse
    for (int i = 0; i < pos; i++) {
        buffer[i] = temp[pos - i - 1];
    }
    buffer[pos] = '\0';
    return pos;
}

static int int64_to_string(long long value, char* buffer, int base) {
    char temp[32];
    int pos = 0;
    int is_negative = 0;

    // Basis prüfen
    if (base < 2 || base > 16) {
        buffer[0] = '\0';
        return 0;
    }

    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    if (value < 0 && base == 10) {
        is_negative = 1;
        unsigned long long abs_value = (unsigned long long)(-(value + 1)) + 1;
        while (abs_value > 0) {
            unsigned digit = abs_value % base;
            temp[pos++] = (digit < 10) ? '0' + digit : 'a' + (digit - 10);
            abs_value /= base;
        }
    } else {
        unsigned long long abs_value = (value < 0) ? -value : value;
        while (abs_value > 0) {
            unsigned digit = abs_value % base;
            temp[pos++] = (digit < 10) ? '0' + digit : 'a' + (digit - 10);
            abs_value /= base;
        }
    }

    if (is_negative) {
        temp[pos++] = '-';
    }

    for (int i = 0; i < pos; i++) {
        buffer[i] = temp[pos - i - 1];
    }

    buffer[pos] = '\0';
    return pos;
}


int snprintf(char *buffer, size_t size, const char *format, ...) {
    if (!buffer || !format || size == 0) {
        return -1;
    }

    __builtin_va_list args;
    __builtin_va_start(args, format);

    size_t buf_pos = 0;
    int written = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++; // Skip '%'

            switch (format[i]) {
                case 'u': {
                    unsigned int val = __builtin_va_arg(args, unsigned int);
                    char temp[32];
                    int len = uint64_to_string((unsigned long long)val, temp, 10);

                    for (int j = 0; j < len && buf_pos < size - 1; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }
                case 'd': {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    int len = int_to_string(val, temp, 10);

                    for (int j = 0; j < len && buf_pos < size - 1; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

                case 'x': {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    int len = int_to_string(val, temp, 16);

                    for (int j = 0; j < len && buf_pos < size - 1; j++) {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

                case 's': {
                    char *str = __builtin_va_arg(args, char*);
                    if (str) {
                        int len = strlen(str);
                        for (int j = 0; j < len && buf_pos < size - 1; j++) {
                            buffer[buf_pos++] = str[j];
                        }
                        written += len;
                    }
                    break;
                }

                case 'c': {
                    char ch = (char)__builtin_va_arg(args, int);
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = ch;
                    }
                    written++;
                    break;
                }

                case 'l': {
                    if (format[i + 1] == 'l') {
                        i++;
                        if (format[i + 1] == 'u') {
                            i++;
                            unsigned long long val = __builtin_va_arg(args, unsigned long long);
                            char temp[32];
                            int len = uint64_to_string(val, temp, 10);
                            for (int j = 0; j < len && buf_pos < size - 1; j++) buffer[buf_pos++] = temp[j];
                            written += len;
                        } else if (format[i + 1] == 'd') {
                            i++;
                            long long val = __builtin_va_arg(args, long long);
                            char temp[32];
                            int len = int64_to_string(val, temp, 10);
                            for (int j = 0; j < len && buf_pos < size - 1; j++) buffer[buf_pos++] = temp[j];
                            written += len;
                        }
                    } else if (format[i + 1] == 'u') {
                        i++;
                        unsigned long val = __builtin_va_arg(args, unsigned long);
                        char temp[32];
                        int len = uint64_to_string(val, temp, 10);
                        for (int j = 0; j < len && buf_pos < size - 1; j++) buffer[buf_pos++] = temp[j];
                        written += len;
                    }
                    break;
                }


                case '%': {
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = '%';
                    }
                    written++;
                    break;
                }

                default:
                    // Unknown format specifier, just copy it
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = '%';
                    }
                    if (buf_pos < size - 1) {
                        buffer[buf_pos++] = format[i];
                    }
                    written += 2;
                    break;
            }
        } else {
            // Regular character
            if (buf_pos < size - 1) {
                buffer[buf_pos++] = format[i];
            }
            written++;
        }
    }

    // Null terminate
    buffer[buf_pos] = '\0';

    __builtin_va_end(args);
    return written;
}