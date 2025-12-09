#include "../include/string.h"
#include <kernel/memory.h>
#include <cstddef>


template <typename T>
char* itohex(T value, char* buffer, const size_t buffer_size)
{
    constexpr size_t required_size = sizeof(T) * 2 + 1;

    // Buffer zu klein?
    if (buffer_size < required_size)
    {
        if (buffer_size > 0) buffer[0] = '\0';
        return nullptr;
    }

    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    constexpr size_t hex_digits = sizeof(T) * 2 - 1;

    for (size_t i = 0; i < sizeof(T); i++)
    {
        uint8_t byte = bytes[i];
        uint8_t high_nibble = (byte >> 4) & 0x0F;
        uint8_t low_nibble  = byte & 0x0F;

        size_t pos = hex_digits - (i * 2) - 1;
        buffer[pos]     = static_cast<char>(high_nibble + (high_nibble > 9 ? 'A' - 10 : '0'));
        buffer[pos + 1] = static_cast<char>(low_nibble  + (low_nibble  > 9 ? 'A' - 10 : '0'));
    }

    buffer[sizeof(T) * 2] = '\0';
    return buffer;
}

char* ptohex(void* ptr, char* buffer, const size_t buffer_size)
{
    const auto value = reinterpret_cast<uintptr_t>(ptr);
    constexpr size_t required_size = sizeof(uintptr_t) * 2 + 1;

    if (buffer_size < required_size)
    {
        if (buffer_size > 0)
            buffer[0] = '\0';
        return nullptr;
    }

    constexpr size_t hex_digits = sizeof(uintptr_t) * 2;

    for (size_t i = 0; i < hex_digits; i++)
    {
        const auto nibble = static_cast<uint8_t>((value >> ((hex_digits - i - 1) * 4)) & 0x0F);
        buffer[i] = static_cast<char>(nibble + (nibble > 9 ? 'A' - 10 : '0'));
    }

    buffer[hex_digits] = '\0';
    return buffer;
}


char* u64tohex(const uint64_t value, char* buffer, const size_t buffer_size)
{
    return itohex(value, buffer, buffer_size);
}

char* u32tohex(const uint32_t value, char* buffer, const size_t buffer_size)
{
    return itohex(value, buffer, buffer_size);
}

char* u16tohex(const uint16_t value, char* buffer, const size_t buffer_size)
{
    return itohex(value, buffer, buffer_size);
}

char* u8tohex(const uint8_t value, char* buffer, const size_t buffer_size)
{
    return itohex(value, buffer, buffer_size);
}


size_t strlen(const char* s)
{
    const char* start = s;
    while (*s != '\0')
    {
        ++s;
    }
    return static_cast<size_t>(s - start);
}

int strcmp(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b)
    {
        ++a;
        ++b;
    }
    return static_cast<int>(static_cast<unsigned char>(*a)) -
        static_cast<int>(static_cast<unsigned char>(*b));
}

int strncmp(const char* a, const char* b, size_t n)
{
    while (n != 0 && *a != '\0' && *b != '\0' && *a == *b)
    {
        ++a;
        ++b;
        --n;
    }

    if (n == 0)
        return 0;

    return static_cast<int>(static_cast<unsigned char>(*a)) -
        static_cast<int>(static_cast<unsigned char>(*b));
}

char* strncpy(char* dest, const char* src, const size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }
    return dest;
}

char* strcpy(char* dest, const char* src)
{
    char* temp = dest;
    while ((*dest++ = *src++) != '\0')
    {
    }
    return temp;
}

void replace_char(char* s, char old_char, char new_char) {
    while (*s) {
        if (*s == old_char) {
            *s = new_char;
        }
        s++;
    }
}

char* strdup(const char* src)
{
    const size_t len = strlen(src) + 1;
    const auto dst = static_cast<char*>(kernel::memory::malloc(len));

    if (dst == nullptr)
        return nullptr;

    strncpy(dst, src, len);
    return dst;
}

char* strrchr(const char* s, const int c)
{
    const auto target = static_cast<unsigned char>(c);
    char* rtnval = nullptr;

    do
    {
        if (static_cast<unsigned char>(*s) == target)
            rtnval = const_cast<char*>(s);
    }
    while (*s++ != '\0');

    return rtnval;
}

char* strchr(const char* s, const unsigned char c)
{
    const auto target = static_cast<unsigned char>(c);

    while (*s != '\0')
    {
        if (static_cast<unsigned char>(*s) == target)
            return const_cast<char*>(s);
        ++s;
    }

    if (target == 0)
        return const_cast<char*>(s);

    return nullptr;
}

char* strncat(char* dest, const char* src, const size_t max)
{
    if (!dest || !src || max == 0)
        return dest;

    size_t dlen = 0;
    while (dlen < max && dest[dlen] != '\0')
    {
        ++dlen;
    }

    if (dlen == max)
        return dest;

    size_t i = 0;
    while (i + dlen < max - 1 && src[i] != '\0')
    {
        dest[dlen + i] = src[i];
        ++i;
    }

    dest[dlen + i] = '\0';
    return dest;
}

char* strcat(char* dst, const char* src)
{
    char* p = dst;

    while (*p != '\0')
        ++p;

    while ((*p++ = *src++) != '\0')
    {
    }
    return dst;
}

char* strtok(char* s, const char delim)
{
    static char* next = nullptr;

    if (s != nullptr)
        next = s;

    if (next == nullptr)
        return nullptr;

    char* start = next;

    while (*next != '\0' && *next != delim)
        ++next;

    if (*next == delim)
    {
        *next = '\0';
        ++next;
    }
    else
    {
        next = nullptr;
    }

    return start;
}

int strcasecmp(const char* s1, const char* s2)
{
    while (*s1 != '\0' && *s2 != '\0')
    {
        auto c1 = static_cast<unsigned char>(*s1);
        auto c2 = static_cast<unsigned char>(*s2);

        if (c1 >= 'A' && c1 <= 'Z')
            c1 = static_cast<unsigned char>(c1 + ('a' - 'A'));

        if (c2 >= 'A' && c2 <= 'Z')
            c2 = static_cast<unsigned char>(c2 + ('a' - 'A'));

        if (c1 != c2)
            return static_cast<int>(c1) - static_cast<int>(c2);

        ++s1;
        ++s2;
    }

    return static_cast<int>(static_cast<unsigned char>(*s1)) -
        static_cast<int>(static_cast<unsigned char>(*s2));
}

char to_upper(const char c)
{
    if (c >= 'a' && c <= 'z')
        return static_cast<char>(c - 32);

    return c;
}

static void reverse_string(char* str, const int length)
{
    int start = 0;
    int end = length - 1;

    while (start < end)
    {
        const char tmp = str[start];
        str[start] = str[end];
        str[end] = tmp;
        ++start;
        --end;
    }
}

// Helper function to convert integer to string
static int int_to_string(int num, char* str, const int base)
{
    int i = 0;
    int is_negative = 0;

    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }

    if (num < 0 && base == 10)
    {
        is_negative = 1;
        num = -num;
    }

    while (num != 0)
    {
        int rem = num % base;
        str[i++] = static_cast<char>((rem > 9) ? (rem - 10) + 'a' : rem + '0');
        num = num / base;
    }

    if (is_negative)
    {
        str[i++] = '-';
    }

    str[i] = '\0';
    reverse_string(str, i);

    return i;
}

static int uint64_to_string(unsigned long long value, char* buffer, const int base)
{
    char temp[32];
    int pos = 0;

    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    while (value > 0)
    {
        unsigned digit = value % base;
        temp[pos++] = static_cast<char>((digit < 10) ? ('0' + digit) : ('a' + (digit - 10)));
        value /= base;
    }

    // reverse
    for (int i = 0; i < pos; i++)
    {
        buffer[i] = temp[pos - i - 1];
    }
    buffer[pos] = '\0';
    return pos;
}

static int int64_to_string(const long long value, char* buffer, int base)
{
    char temp[32];
    int pos = 0;
    int is_negative = 0;

    // Basis prüfen
    if (base < 2 || base > 16)
    {
        buffer[0] = '\0';
        return 0;
    }

    if (value == 0)
    {
        buffer[0] = '0';
        buffer[1] = '\0';
        return 1;
    }

    if (value < 0 && base == 10)
    {
        is_negative = 1;
        uint64_t abs_value = static_cast<uint64_t>(-(value + 1)) + 1;
        while (abs_value > 0)
        {
            unsigned digit = abs_value % base;
            temp[pos++] = static_cast<char>((digit < 10) ? ('0' + digit) : ('a' + (digit - 10)));
            abs_value /= base;
        }
    }
    else
    {
        unsigned long long abs_value = (value < 0) ? -value : value;
        while (abs_value > 0)
        {
            unsigned digit = abs_value % base;
            temp[pos++] = static_cast<char>((digit < 10) ? ('0' + digit) : ('a' + (digit - 10)));
            abs_value /= base;
        }
    }

    if (is_negative)
    {
        temp[pos++] = '-';
    }

    for (int i = 0; i < pos; i++)
    {
        buffer[i] = temp[pos - i - 1];
    }

    buffer[pos] = '\0';
    return pos;
}

int snprintf(char* buffer, const size_t size, const char* format, ...)
{
    if (!buffer || !format || size == 0)
    {
        return -1;
    }

    __builtin_va_list args;
    __builtin_va_start(args, format);

    size_t buf_pos = 0;
    int written = 0;

    for (int i = 0; format[i] != '\0'; i++)
    {
        if (format[i] == '%' && format[i + 1] != '\0')
        {
            i++; // Skip '%'
            int precision = -1;
            bool plus_sign = false;

            // Check for prefix
            if (format[i] == '+')
            {
                plus_sign = true;
                i++;
            }

            if (format[i] == '.')
            {
                i++; // skip '.'
                if (format[i] == '*')
                {
                    precision = __builtin_va_arg(args, int);
                    i++; // skip '*'
                }
                else
                {
                    precision = 0;
                    while (format[i] >= '0' && format[i] <= '9')
                    {
                        precision = precision * 10 + (format[i] - '0');
                        i++;
                    }
                }
            }

            switch (format[i])
            {
            case 'u':
                {
                    const unsigned int val = __builtin_va_arg(args, unsigned int);
                    char temp[32];
                    int len = uint64_to_string(val, temp, 10);

                    if (plus_sign)
                    {
                        if (buf_pos < size - 1)
                            buffer[buf_pos++] = '+';
                        written++;
                    }

                    for (int j = 0; j < len && buf_pos < size - 1; j++)
                    {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }
            case 'd':
                {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    int len = int_to_string(val, temp, 10);

                    for (int j = 0; j < len && buf_pos < size - 1; j++)
                    {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

            case 'x':
                {
                    int val = __builtin_va_arg(args, int);
                    char temp[32];
                    int len = int_to_string(val, temp, 16);

                    for (int j = 0; j < len && buf_pos < size - 1; j++)
                    {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

            case 's':
                {
                    const char* str = __builtin_va_arg(args, const char *);
                    if (!str)
                        str = "<null>";

                    int count = 0;
                    while (str[count] && (precision < 0 || count < precision) &&
                        buf_pos < size - 1)
                    {
                        buffer[buf_pos++] = str[count++];
                    }
                    written += count;
                    break;
                }

            case 'p':
                {
                    void* ptr_val = __builtin_va_arg(args, void *);
                    const auto val = reinterpret_cast<uint64_t>(ptr_val);
                    char temp[32];
                    const int len = uint64_to_string(val, temp, 16);

                    if (plus_sign)
                    {
                        if (buf_pos < size - 1)
                            buffer[buf_pos++] = '+';
                        written++;
                    }

                    for (int j = 0; j < len && buf_pos < size - 1; j++)
                    {
                        buffer[buf_pos++] = temp[j];
                    }
                    written += len;
                    break;
                }

            case 'c':
                {
                    const char ch = static_cast<char>(__builtin_va_arg(args, int));
                    if (buf_pos < size - 1)
                    {
                        buffer[buf_pos++] = ch;
                    }
                    written++;
                    break;
                }

            case 'l':
                {
                    if (format[i + 1] == 'l')
                    {
                        i++;
                        if (format[i + 1] == 'd')
                        {
                            i++;
                            long long val = __builtin_va_arg(args, long long);
                            char temp[32];
                            int len = int64_to_string(val, temp, 10);
                            if (plus_sign && val >= 0)
                            {
                                if (buf_pos < size - 1)
                                    buffer[buf_pos++] = '+';
                                written++;
                            }
                            for (int j = 0; j < len && buf_pos < size - 1; j++)
                                buffer[buf_pos++] = temp[j];
                            written += len;
                        }
                        else if (format[i + 1] == 'u')
                        {
                            i++;
                            unsigned long long val = __builtin_va_arg(args, unsigned long long);
                            char temp[32];
                            int len = uint64_to_string(val, temp, 10);
                            for (int j = 0; j < len && buf_pos < size - 1; j++)
                                buffer[buf_pos++] = temp[j];
                            written += len;
                        }
                        else if (format[i + 1] == 'x')
                        {
                            // HEX 64-bit
                            i++;
                            unsigned long long val = __builtin_va_arg(args, unsigned long long);
                            char temp[32];
                            int len = uint64_to_string(val, temp, 16);
                            for (int j = 0; j < len && buf_pos < size - 1; j++)
                                buffer[buf_pos++] = temp[j];
                            written += len;
                        }
                    }
                    break;
                }

            case '%':
                {
                    if (buf_pos < size - 1)
                    {
                        buffer[buf_pos++] = '%';
                    }
                    written++;
                    break;
                }

            default:
                // Unknown format specifier, just copy it
                if (buf_pos < size - 1)
                {
                    buffer[buf_pos++] = '%';
                }
                if (buf_pos < size - 1)
                {
                    buffer[buf_pos++] = format[i];
                }
                written += 2;
                break;
            }
        }
        else
        {
            // Regular character
            if (buf_pos < size - 1)
            {
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
