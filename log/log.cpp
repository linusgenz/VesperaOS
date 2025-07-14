//
// Created by Linus on 10.07.25.
//

#include "../include/log.h"
#include "../include/string.h"
BasicRenderer *Log::renderer = nullptr;

void Log::SetRenderer(BasicRenderer *r) {
    renderer = r;
}


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

void Log::LogMsg(const char* fmt, ...) {
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
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    PrintFormatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}
void Log::PrintFormatted(const char *fmt, __builtin_va_list args) {
    char chr;
    while ((chr = *fmt++) != 0) {
        if (chr == '%') {
            // Format-Modifikatoren erkennen
            bool long_long = false;
            bool long_flag = false;

            // Mehrere Zeichen prüfen
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
                    const char* str = __builtin_va_arg(args, const char*);
                    renderer->print(str ? str : "<null>");
                    break;
                }
                case 'u': {
                    uint64_t val;
                    if (long_long || long_flag)
                        val = __builtin_va_arg(args, uint64_t);
                    else
                        val = __builtin_va_arg(args, uint32_t);
                    UIntToStr(val, buffer, 10);
                    renderer->print(buffer);
                    break;
                }
                case 'x': {
                    uint64_t val;
                    if (long_long || long_flag)
                        val = __builtin_va_arg(args, uint64_t);
                    else
                        val = __builtin_va_arg(args, uint32_t);
                    UIntToStr(val, buffer, 16);
                    renderer->print(buffer);
                    break;
                }
                case 'd': {
                    int64_t val;
                    if (long_long || long_flag)
                        val = __builtin_va_arg(args, int64_t);
                    else
                        val = __builtin_va_arg(args, int32_t);
                    if (val < 0) {
                        renderer->print("-");
                        val = -val;
                    }
                    UIntToStr(static_cast<uint64_t>(val), buffer, 10);
                    renderer->print(buffer);
                    break;
                }
                case 'p': {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    UIntToStr(val, buffer, 16, true);
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

