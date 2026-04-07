

#include <klib/string.h>
#include <vespera/mm/memory.h>

void memset(void* dest, const u32 val, const u64 num) {
    for (u64 i = 0; i < num; i++) {
        *reinterpret_cast<u8*>(reinterpret_cast<u64>(dest) + i) = val;
    }
}

void* memcpy(void* dest, const void* src, usize len) {
    auto* d = static_cast<char*>(dest);
    auto* s = static_cast<const char*>(src);
    while (len--) *d++ = *s++;
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, const usize num) {
    const auto* a = static_cast<const u8*>(ptr1);
    const auto* b = static_cast<const u8*>(ptr2);
    for (usize i = 0; i < num; i++) {
        if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
    }
    return 0;
}

void* memmove(void* dest, const void* src, usize len) {
    auto* d = static_cast<char*>(dest);
    if (auto s = static_cast<const char*>(src); d < s)
        while (len--) *d++ = *s++;
    else {
        auto lasts = const_cast<char*>(s + (len - 1));
        auto* lastd = d + (len - 1);
        while (len--) *lastd-- = *lasts--;
    }
    return dest;
}

template <typename T>
char* itohex(T value, char* buffer, const usize buffer_size) {
    // Buffer too small?
    if (constexpr usize required_size = sizeof(T) * 2 + 1; buffer_size < required_size) {
        if (buffer_size > 0) buffer[0] = '\0';
        return nullptr;
    }

    const auto* bytes = reinterpret_cast<const u8*>(&value);
    constexpr usize hex_digits = sizeof(T) * 2 - 1;

    for (usize i = 0; i < sizeof(T); i++) {
        const u8 byte = bytes[i];
        const u8 high_nibble = (byte >> 4) & 0x0F;
        const u8 low_nibble = byte & 0x0F;

        const usize pos = hex_digits - (i * 2) - 1;
        buffer[pos] = static_cast<char>(high_nibble + (high_nibble > 9 ? 'A' - 10 : '0'));
        buffer[pos + 1] = static_cast<char>(low_nibble + (low_nibble > 9 ? 'A' - 10 : '0'));
    }

    buffer[sizeof(T) * 2] = '\0';
    return buffer;
}

char* ptohex(void* ptr, char* buffer, const usize buffer_size) {
    const auto value = reinterpret_cast<uptr>(ptr);

    if (constexpr usize required_size = sizeof(uptr) * 2 + 1; buffer_size < required_size) {
        if (buffer_size > 0) buffer[0] = '\0';
        return nullptr;
    }

    constexpr usize hex_digits = sizeof(uptr) * 2;

    for (usize i = 0; i < hex_digits; i++) {
        const auto nibble = static_cast<u8>((value >> ((hex_digits - i - 1) * 4)) & 0x0F);
        buffer[i] = static_cast<char>(nibble + (nibble > 9 ? 'A' - 10 : '0'));
    }

    buffer[hex_digits] = '\0';
    return buffer;
}

char* u64_tohex(const u64 value, char* buffer, const usize buffer_size) {
    return itohex(value, buffer, buffer_size);
}

char* u32_tohex(const u32 value, char* buffer, const usize buffer_size) {
    return itohex(value, buffer, buffer_size);
}

char* u16_tohex(const u16 value, char* buffer, const usize buffer_size) {
    return itohex(value, buffer, buffer_size);
}

char* u8_tohex(const u8 value, char* buffer, const usize buffer_size) {
    return itohex(value, buffer, buffer_size);
}

usize strlen(const char* s) {
    const char* start = s;
    while (*s != '\0') {
        ++s;
    }
    return static_cast<usize>(s - start);
}

int strcmp(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return static_cast<int>(static_cast<unsigned char>(*a)) - static_cast<int>(static_cast<unsigned char>(*b));
}

int strncmp(const char* a, const char* b, usize n) {
    while (n != 0 && *a != '\0' && *b != '\0' && *a == *b) {
        ++a;
        ++b;
        --n;
    }

    if (n == 0) return 0;

    return static_cast<int>(static_cast<unsigned char>(*a)) - static_cast<int>(static_cast<unsigned char>(*b));
}

char* strncpy(char* dest, const char* src, const usize n) {
    usize i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcpy(char* dest, const char* src) {
    char* temp = dest;
    while ((*dest++ = *src++) != '\0') {
    }
    return temp;
}

void replace_char(char* s, const char old_char, const char new_char) {
    while (*s) {
        if (*s == old_char) {
            *s = new_char;
        }
        s++;
    }
}

char* strdup(const char* src) {
    const usize len = strlen(src) + 1;
    const auto dst = static_cast<char*>(kernel::memory::malloc(len));

    if (dst == nullptr) return nullptr;

    strncpy(dst, src, len);
    return dst;
}

char* strrchr(const char* s, const int c) {
    const auto target = static_cast<unsigned char>(c);
    char* rtnval = nullptr;

    do {
        if (static_cast<unsigned char>(*s) == target) rtnval = const_cast<char*>(s);
    } while (*s++ != '\0');

    return rtnval;
}

char* strchr(const char* s, const unsigned char c) {
    const auto target = static_cast<unsigned char>(c);

    while (*s != '\0') {
        if (static_cast<unsigned char>(*s) == target) return const_cast<char*>(s);
        ++s;
    }

    if (target == 0) return const_cast<char*>(s);

    return nullptr;
}

char* strncat(char* dest, const char* src, const usize max) {
    if (!dest || !src || max == 0) return dest;

    usize dlen = 0;
    while (dlen < max && dest[dlen] != '\0') {
        ++dlen;
    }

    if (dlen == max) return dest;

    usize i = 0;
    while (i + dlen < max - 1 && src[i] != '\0') {
        dest[dlen + i] = src[i];
        ++i;
    }

    dest[dlen + i] = '\0';
    return dest;
}

char* strcat(char* dst, const char* src) {
    char* p = dst;

    while (*p != '\0') ++p;

    while ((*p++ = *src++) != '\0') {
    }
    return dst;
}

char* strtok(char* s, const char delim) {
    static char* next = nullptr;

    if (s != nullptr) next = s;

    if (next == nullptr) return nullptr;

    char* start = next;

    while (*next != '\0' && *next != delim) ++next;

    if (*next == delim) {
        *next = '\0';
        ++next;
    } else {
        next = nullptr;
    }

    return start;
}

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 != '\0' && *s2 != '\0') {
        auto c1 = static_cast<unsigned char>(*s1);
        auto c2 = static_cast<unsigned char>(*s2);

        if (c1 >= 'A' && c1 <= 'Z') c1 = static_cast<unsigned char>(c1 + ('a' - 'A'));

        if (c2 >= 'A' && c2 <= 'Z') c2 = static_cast<unsigned char>(c2 + ('a' - 'A'));

        if (c1 != c2) return static_cast<int>(c1) - static_cast<int>(c2);

        ++s1;
        ++s2;
    }

    return static_cast<int>(static_cast<unsigned char>(*s1)) - static_cast<int>(static_cast<unsigned char>(*s2));
}

char to_upper(const char c) {
    if (c >= 'a' && c <= 'z') return static_cast<char>(c - 32);

    return c;
}

static void write_str(fmt_write_fn write, void* ctx, const char* s, int len) {
    for (int i = 0; i < len; i++) write(ctx, s[i]);
}

static void write_pad(fmt_write_fn write, void* ctx, char pad_char, int count) {
    for (int i = 0; i < count; i++) write(ctx, pad_char);
}

static int fmt_uint(u64 val, char* buf, int base, bool upper) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int len = 0;
    char tmp[32];
    while (val > 0) { tmp[len++] = digits[val % base]; val /= base; }
    for (int i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
    return len;
}

static int fmt_int(i64 val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    int len = 0;
    bool neg = val < 0;
    if (neg) { buf[len++] = '-'; val = -val; }
    char tmp[32];
    int digits = 0;
    while (val > 0) { tmp[digits++] = '0' + (val % 10); val /= 10; }
    for (int i = 0; i < digits; i++) buf[len++] = tmp[digits - 1 - i];
    buf[len] = '\0';
    return len;
}

int vformat(fmt_write_fn write, void* ctx, const char* fmt, __builtin_va_list args) {
    int written = 0;
    char buf[32];

    while (*fmt) {
        if (*fmt != '%') {
            write(ctx, *fmt++);
            written++;
            continue;
        }
        fmt++; // skip '%'

        // ── Flags ──
        bool left_align = false;
        bool zero_pad   = false;
        bool plus_sign  = false;
        bool space_sign = false;
        bool alt_form   = false;

        bool parsing_flags = true;
        while (parsing_flags) {
            switch (*fmt) {
                case '-': left_align = true; fmt++; break;
                case '0': zero_pad   = true; fmt++; break;
                case '+': plus_sign  = true; fmt++; break;
                case ' ': space_sign = true; fmt++; break;
                case '#': alt_form   = true; fmt++; break;
                default:  parsing_flags = false;    break;
            }
        }

        // ── Breite ──
        int width = 0;
        if (*fmt == '*') {
            width = __builtin_va_arg(args, int);
            if (width < 0) { left_align = true; width = -width; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt++ - '0');
        }

        // ── Precision ──
        int precision = -1;
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = __builtin_va_arg(args, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9')
                    precision = precision * 10 + (*fmt++ - '0');
            }
        }

        // ── Längen-Modifier ──
        bool is_long      = false;
        bool is_long_long = false;
        bool is_size_t    = false;

        if (*fmt == 'l') {
            fmt++;
            if (*fmt == 'l') { is_long_long = true; fmt++; }
            else               is_long      = true;
        } else if (*fmt == 'z') {
            is_size_t = true; fmt++;
        }

        // ── Specifier ──
        const char spec = *fmt++;
        char pad_char = (zero_pad && !left_align) ? '0' : ' ';

        switch (spec) {
            // ── Strings ──
            case 's': {
                const char* s = __builtin_va_arg(args, const char*);
                if (!s) s = "(null)";

                int slen = 0;
                while (s[slen]) slen++;
                if (precision >= 0 && slen > precision) slen = precision;

                int pad = (width > slen) ? width - slen : 0;

                if (!left_align) write_pad(write, ctx, ' ', pad);
                write_str(write, ctx, s, slen);
                if  (left_align) write_pad(write, ctx, ' ', pad);

                written += slen + pad;
                break;
            }

            // ── Zeichen ──
            case 'c': {
                char c = static_cast<char>(__builtin_va_arg(args, int));
                int pad = (width > 1) ? width - 1 : 0;
                if (!left_align) write_pad(write, ctx, ' ', pad);
                write(ctx, c);
                if  (left_align) write_pad(write, ctx, ' ', pad);
                written += 1 + pad;
                break;
            }

            // ── Vorzeichenbehaftete Integer ──
            case 'd':
            case 'i': {
                i64 val = is_long_long ? __builtin_va_arg(args, i64)
                        : is_long      ? __builtin_va_arg(args, long)
                        : is_size_t    ? __builtin_va_arg(args, isize)
                                       : __builtin_va_arg(args, i32);
                int len = fmt_int(val, buf);

                // Vorzeichen-Präfix
                const char* prefix = "";
                if (val >= 0 && plus_sign)  prefix = "+";
                if (val >= 0 && space_sign) prefix = " ";
                int plen = (prefix[0] != '\0') ? 1 : 0;

                int pad = (width > len + plen) ? width - len - plen : 0;

                if (!left_align && pad_char == ' ') write_pad(write, ctx, ' ', pad);
                write_str(write, ctx, prefix, plen);
                if (!left_align && pad_char == '0') write_pad(write, ctx, '0', pad);
                write_str(write, ctx, buf + (val < 0 ? 0 : 0), len);
                if  (left_align)                    write_pad(write, ctx, ' ', pad);

                written += len + plen + pad;
                break;
            }

            // ── Vorzeichenlose Integer ──
            case 'u':
            case 'x':
            case 'X':
            case 'o':
            case 'p': {
                u64 val;
                if (spec == 'p') {
                    val = reinterpret_cast<u64>(__builtin_va_arg(args, void*));
                } else {
                    val = is_long_long ? __builtin_va_arg(args, u64)
                        : is_long      ? __builtin_va_arg(args, unsigned long)
                        : is_size_t    ? __builtin_va_arg(args, usize)
                                       : __builtin_va_arg(args, u32);
                }

                int base  = (spec == 'o') ? 8
                          : (spec == 'u') ? 10
                          :                 16;
                bool upper = (spec == 'X');

                int len = fmt_uint(val, buf, base, upper);

                // Präfix für %# oder %p
                const char* prefix = "";
                if (spec == 'p')                      prefix = "0x";
                else if (alt_form && spec == 'x')     prefix = "0x";
                else if (alt_form && spec == 'X')     prefix = "0X";
                else if (alt_form && spec == 'o')     prefix = "0";
                int plen = 0;
                while (prefix[plen]) plen++;

                int pad = (width > len + plen) ? width - len - plen : 0;

                if (!left_align && pad_char == ' ') write_pad(write, ctx, ' ', pad);
                write_str(write, ctx, prefix, plen);
                if (!left_align && pad_char == '0') write_pad(write, ctx, '0', pad);
                write_str(write, ctx, buf, len);
                if  (left_align)                    write_pad(write, ctx, ' ', pad);

                written += len + plen + pad;
                break;
            }

            case '%':
                write(ctx, '%');
                written++;
                break;

            default:
                write(ctx, '%');
                write(ctx, spec);
                written += 2;
                break;
        }
    }

    return written;
}

struct buf_ctx {
    char*  buf;
    usize  pos;
    usize  max;
};

static void buf_write(void* ctx, char c) {
    auto* b = static_cast<buf_ctx*>(ctx);
    if (b->pos < b->max) b->buf[b->pos++] = c;
}

int snprintf(char* buffer, usize size, const char* fmt, ...) {
    if (!buffer || !fmt || size == 0) return -1;
    buf_ctx ctx{ buffer, 0, size - 1 };
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int n = vformat(buf_write, &ctx, fmt, args);
    __builtin_va_end(args);
    buffer[ctx.pos] = '\0';
    return n;
}