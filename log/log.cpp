//
// Created by Linus on 10.07.25.
//

#include <log.h>
#include <string.h>
#include "../kernel/graphics/IScreenRenderer.h"

IScreenRenderer* Log::renderer = nullptr;
spinlock_t Log::log_spin = {};
kernel::mutex_t Log::log_mutex = {};
bool Log::initialized = false;
bool Log::is_debug = false;

void Log::init()
{
    //  initialized = true;
    //  kernel::mutex_init(&log_mutex);
    // log_spin.init();
}

void Log::SetRenderer(IScreenRenderer* r)
{
    renderer = r;
}

void Log::enableDebug()
{
    is_debug = true;
}


void UIntToStr(uint64_t value, char* buffer, uint8_t base = 10, bool prefix = false)
{
    char temp[32];
    int i = 0;

    if (value == 0)
    {
        temp[i++] = '0';
    }
    else
    {
        while (value > 0)
        {
            const auto digits = "0123456789ABCDEF";
            temp[i++] = digits[value % base];
            value /= base;
        }
    }

    int j = 0;
    if (prefix && base == 16)
    {
        buffer[j++] = '0';
        buffer[j++] = 'x';
    }

    while (i--)
    {
        buffer[j++] = temp[i];
    }

    buffer[j] = '\0';
}

void Log::Info(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    renderer->print("[  ");
    renderer->print("INFO", BLUE, BLACK);
    renderer->print("   ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}


void Log::Ok(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    renderer->print("[   ");
    renderer->print("OK", GREEN, BLACK);
    renderer->print("    ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Warning(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    renderer->print("[ ");
    renderer->print("WARNING", YELLOW, BLACK);
    renderer->print(" ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Error(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    renderer->print("[  ");
    renderer->print("ERROR", RED, BLACK);
    renderer->print("  ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::LogMsg(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    renderer->print("[   ");
    renderer->print("LOG", GRAY, BLACK);
    renderer->print("   ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::PrintLn(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

void Log::Print(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);
}

void Log::debug(const char* fmt, ...)
{
    if (!is_debug) return;
    spinlock_guard g(log_spin);

    renderer->print("[  ");
    renderer->print("DEBUG", ORANGE, BLACK);
    renderer->print("  ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    renderer->print("\n");
}

static void float_to_str(float val, char* buf, int precision)
{
    if (val < 0)
    {
        *buf++ = '-';
        val = -val;
    }

    // Ganzzahlteil
    const auto int_part = static_cast<uint32_t>(val);
    float frac_part = val - static_cast<float>(int_part);

    char int_buf[32];
    UIntToStr(int_part, int_buf, 10);
    char* p = int_buf;
    while (*p)
    {
        *buf++ = *p++;
    }

    *buf++ = '.';

    // Nachkommateil
    for (int i = 0; i < precision; i++)
    {
        frac_part *= 10.0f;
        int digit = static_cast<int>(frac_part);
        *buf++ = static_cast<char>('0' + digit);
        frac_part -= static_cast<float>(digit);
    }

    *buf = '\0';
}

void Log::print_formatted(const char* fmt, __builtin_va_list args)
{
    char chr;
    while ((chr = *fmt++) != 0)
    {
        if (chr == '%')
        {
            // Flags & Width
            bool long_long = false;
            bool long_flag = false;
            char pad_char = ' ';
            int min_width = 0;
            int precision = -1;

            // Padding: z. B. %02x → '0' erkannt
            if (*fmt == '0')
            {
                pad_char = '0';
                fmt++;
            }

            // Breite (z. B. 2, 4, 8, etc.)
            while (*fmt >= '0' && *fmt <= '9')
            {
                min_width = min_width * 10 + (*fmt - '0');
                fmt++;
            }

            // Precision: .* oder .n
            if (*fmt == '.')
            {
                fmt++;
                if (*fmt == '*')
                {
                    precision = __builtin_va_arg(args, int);
                    fmt++;
                }
                else
                {
                    precision = 0;
                    while (*fmt >= '0' && *fmt <= '9')
                    {
                        precision = precision * 10 + (*fmt - '0');
                        fmt++;
                    }
                }
            }

            // Länge: l / ll
            if (*fmt == 'l')
            {
                fmt++;
                if (*fmt == 'l')
                {
                    long_long = true;
                    fmt++;
                }
                else
                {
                    long_flag = true;
                }
            }

            char specifier = *fmt++;
            char buffer[64];

            switch (specifier)
            {
            case 's':
                {
                    const char* str = __builtin_va_arg(args, const char*);
                    if (!str) str = "<null>";

                    if (precision >= 0)
                    {
                        // Maximal precision Zeichen ausgeben
                        int count = 0;
                        while (str[count] && count < precision)
                        {
                            renderer->put_char(str[count]);
                            count++;
                        }
                    }
                    else
                    {
                        renderer->print(str);
                    }
                    break;
                }
            case 'u':
            case 'x':
                {
                    uint64_t val = (long_long || long_flag)
                                       ? __builtin_va_arg(args, uint64_t)
                                       : __builtin_va_arg(args, uint32_t);
                    int base = (specifier == 'x') ? 16 : 10;
                    UIntToStr(val, buffer, base);

                    // Padding manuell
                    const size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        renderer->put_char(pad_char);

                    renderer->print(buffer);
                    break;
                }
            case 'c':
                {
                    int val = __builtin_va_arg(args, int);
                    renderer->put_char(static_cast<char>(val));
                    break;
                }
            case 'd':
                {
                    int64_t val = (long_long || long_flag)
                                      ? __builtin_va_arg(args, int64_t)
                                      : __builtin_va_arg(args, int32_t);
                    if (val < 0)
                    {
                        renderer->put_char('-');
                        val = -val;
                    }
                    UIntToStr(static_cast<uint64_t>(val), buffer, 10);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        renderer->put_char(pad_char);
                    renderer->print(buffer);
                    break;
                }
            case 'f':
                {
                    // Float-Support
                    double val = __builtin_va_arg(args, double);
                    int frac_digits = (precision >= 0) ? precision : 6;
                    float_to_str(static_cast<float>(val), buffer, frac_digits);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        renderer->put_char(pad_char);
                    renderer->print(buffer);
                    break;
                }
            case 'p':
                {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    renderer->print("0x");
                    UIntToStr(val, buffer, 16);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
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
        }
        else
        {
            renderer->put_char(chr);
        }
    }
}
