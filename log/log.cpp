//
// Created by Linus on 10.07.25.
//

#include "../include/log.h"
#include "../include/string.h"

BasicRenderer *Log::renderer = nullptr;

void Log::SetRenderer(BasicRenderer *r) {
    renderer = r;
}

spinlock_t Log::log_lock = {};


void UIntToStr(uint64_t value, char *buffer, uint8_t base = 10, bool prefix = false) {
    char *digits = "0123456789ABCDEF";
    char temp[32];
    int i = 0;

    if (value == 0) {
        temp[i++] = '0';
    } else {
        while (value > 0) {
            temp[i++] = digits[value % base];
            value /= base;
        }
    }

    int j = 0;
    if (prefix && base == 16) {
        buffer[j++] = '0';
        buffer[j++] = 'x';
    }

    while (i--) {
        buffer[j++] = temp[i];
    }

    buffer[j] = '\0';
}

void Log::Info(const char *fmt, ...) {
    spinlock_guard g(log_lock);

    Colour old = renderer->get_colour();
    renderer->print("[  ");
    renderer->set_colour(Colour::BLUE);
    renderer->print("INFO");
    renderer->set_colour(old);
    renderer->print("   ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Ok(const char *fmt, ...) {
    spinlock_guard g(log_lock);

    Colour old = renderer->get_colour();
    renderer->print("[   ");
    renderer->set_colour(Colour::GREEN);
    renderer->print("OK");
    renderer->set_colour(old);
    renderer->print("    ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Warning(const char *fmt, ...) {
    spinlock_guard g(log_lock);

    Colour old = renderer->get_colour();
    renderer->print("[ ");
    renderer->set_colour(Colour::YELLOW);
    renderer->print("WARNING");
    renderer->set_colour(old);
    renderer->print(" ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Error(const char *fmt, ...) {
    spinlock_guard g(log_lock);

    Colour old = renderer->get_colour();
    renderer->print("[  ");
    renderer->set_colour(Colour::RED);
    renderer->print("ERROR");
    renderer->set_colour(old);
    renderer->print("  ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::LogMsg(const char *fmt, ...) {
    spinlock_guard g(log_lock);

    Colour old = renderer->get_colour();
    renderer->print("[   ");
    renderer->set_colour(Colour::GRAY);
    renderer->print("LOG");
    renderer->set_colour(old);
    renderer->print("   ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::PrintLn(const char *fmt, ...) {
    spinlock_guard g(log_lock);
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Print(const char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);
}

void Log::PrintFormatted(const char *fmt, __builtin_va_list args) {
    char chr;
    while ((chr = *fmt++) != 0) {
        if (chr == '%') {
            // Flags & Width
            bool long_long = false;
            bool long_flag = false;
            char pad_char = ' ';
            int min_width = 0;

            // Padding: z. B. %02x → '0' erkannt
            if (*fmt == '0') {
                pad_char = '0';
                fmt++;
            }

            // Breite (z. B. 2, 4, 8, etc.)
            while (*fmt >= '0' && *fmt <= '9') {
                min_width = min_width * 10 + (*fmt - '0');
                fmt++;
            }

            // Länge: l / ll
            if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    long_long = true;
                    fmt++;
                } else {
                    long_flag = true;
                }
            }

            char specifier = *fmt++;
            char buffer[64];

            switch (specifier) {
                case 's': {
                    const char *str = __builtin_va_arg(args, const char*);
                    renderer->print(str ? str : "<null>");
                    break;
                }
                case 'u':
                case 'x': {
                    uint64_t val = (long_long || long_flag)
                                       ? __builtin_va_arg(args, uint64_t)
                                       : __builtin_va_arg(args, uint32_t);
                    int base = (specifier == 'x') ? 16 : 10;
                    UIntToStr(val, buffer, base);

                    // Padding manuell
                    int len = strlen(buffer);
                    for (int i = len; i < min_width; i++)
                        renderer->put_char(pad_char);

                    renderer->print(buffer);
                    break;
                }
                case 'd': {
                    int64_t val = (long_long || long_flag)
                                      ? __builtin_va_arg(args, int64_t)
                                      : __builtin_va_arg(args, int32_t);
                    if (val < 0) {
                        renderer->put_char('-');
                        val = -val;
                    }
                    UIntToStr((uint64_t) val, buffer, 10);
                    int len = strlen(buffer);
                    for (int i = len; i < min_width; i++)
                        renderer->put_char(pad_char);
                    renderer->print(buffer);
                    break;
                }
                case 'p': {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    renderer->print("0x");
                    UIntToStr(val, buffer, 16);
                    int len = strlen(buffer);
                    for (int i = len; i < min_width; i++)
                        renderer->put_char('0');
                    renderer->print(buffer);
                    break;
                }
                case '%':
                    renderer->put_char('%');
                    break;
                default:
                    renderer->put_char('%');
                    renderer->put_char(specifier);
                    break;
            }
        } else {
            renderer->put_char(chr);
        }
    }
}
