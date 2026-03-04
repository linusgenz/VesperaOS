//
// Created by Linus on 10.07.25.
//

#include <log.h>
#include <string.h>
#include "../include/kernel/terminal.h"

Terminal* Log::t = nullptr;
spinlock_t Log::log_spin = {};
kernel::mutex_t Log::log_mutex = {};
bool Log::initialized = false;
bool Log::is_debug = false;

void Log::init()
{
    //  initialized = true;
    //  kernel::mutex_init(&log_mutex);
     log_spin.init("log_lock");
}

 void Log::log_prefix(
    const char* tag,
    uint32_t tag_fg
)
{
    t->set_colour(WHITE, BLACK);
    t->print("[ ");

    t->set_colour(tag_fg, BLACK);
    t->print(tag);

    t->set_colour(WHITE, BLACK);
    t->print(" ] ");
}


void Log::SetTerminal(Terminal* _t)
{
    t = _t;
}

void Log::enableDebug()
{
    is_debug = true;
}

void Log::disableDebug()
{
    is_debug = false;
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

    log_prefix("INFO", BLUE);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
}

void Log::Ok(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    log_prefix("OK", GREEN);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
}


void Log::Warning(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    log_prefix("WARNING", YELLOW);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
}

void Log::Error(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    log_prefix("ERROR", RED);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
}


void Log::LogMsg(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    log_prefix("LOG", GRAY);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
}

void Log::PrintLn(const char* fmt, ...)
{
    spinlock_guard g(log_spin);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
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
    if (!is_debug)
        return;

    spinlock_guard g(log_spin);

    log_prefix("DEBUG", ORANGE);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t->print("\n");
    t->flush();
}
/*
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
*/
void Log::print_formatted(const char* fmt, __builtin_va_list args)
{
    char chr = 0;
    while ((chr = *fmt++) != 0)
    {
        if (chr == '%')
        {
            // Flags & Width
            bool long_long = false;
            bool long_flag = false;
            char pad_char = ' ';
            size_t min_width = 0;
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
                            t->put_char(str[count]);
                            count++;
                        }
                    }
                    else
                    {
                        t->print(str);
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
                        t->put_char(pad_char);

                    t->print(buffer);
                    break;
                }
            case 'c':
                {
                    int val = __builtin_va_arg(args, int);
                    t->put_char(static_cast<char>(val));
                    break;
                }
            case 'd':
                {
                    int64_t val = (long_long || long_flag)
                                      ? __builtin_va_arg(args, int64_t)
                                      : __builtin_va_arg(args, int32_t);
                    if (val < 0)
                    {
                        t->put_char('-');
                        val = -val;
                    }
                    UIntToStr(static_cast<uint64_t>(val), buffer, 10);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        t->put_char(pad_char);
                    t->print(buffer);
                    break;
                }
   /*         case 'f':
                {
                    // Float-Support
                    double val = __builtin_va_arg(args, double);
                    int frac_digits = (precision >= 0) ? precision : 6;
                    float_to_str(static_cast<float>(val), buffer, frac_digits);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        t->put_char(pad_char);
                    t->print(buffer);
                    break;
                }*/
            case 'p':
                {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    t->print("0x");
                    UIntToStr(val, buffer, 16);
                    size_t len = strlen(buffer);
                    for (size_t i = len; i < min_width; i++)
                        t->put_char('0');
                    t->print(buffer);
                    break;
                }
            case '%':
                t->put_char('%');
                break;
            default:
                t->put_char('%');
                t->put_char(specifier);
                break;
            }
        }
        else
        {
            t->put_char(chr);
        }
    }
}
