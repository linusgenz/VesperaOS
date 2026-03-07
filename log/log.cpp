//
// Created by Linus on 10.07.25.
//

#include <vespera/terminal.h>
#include <klib/string.h>
#include <vespera/log.h>

Terminal* Log::t_ = nullptr;
Spinlock Log::log_spin_ = {};
kernel::Mutex Log::log_mutex_ = {};
bool Log::initialized_ = false;
bool Log::is_debug_ = false;

void Log::init()
{
    //  initialized = true;
    //  kernel::mutex_init(&log_mutex);
     log_spin_.init("log_lock");
}

 void Log::log_prefix(
    const char* tag,
    u32 tag_fg
)
{
    t_->set_colour(WHITE, BLACK);
    t_->print("[ ");

    t_->set_colour(tag_fg, BLACK);
    t_->print(tag);

    t_->set_colour(WHITE, BLACK);
    t_->print(" ] ");
}


void Log::set_terminal(Terminal* t)
{
    t_ = t;
}

void Log::enable_debug()
{
    is_debug_ = true;
}

void Log::disable_debug()
{
    is_debug_ = false;
}


void u_int_to_str(u64 value, char* buffer, u8 base = 10, bool prefix = false)
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

void Log::info(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    log_prefix("INFO", BLUE);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::ok(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    log_prefix("OK", GREEN);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}


void Log::warning(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    log_prefix("WARNING", YELLOW);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::error(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    log_prefix("ERROR", RED);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}


void Log::log_msg(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    log_prefix("LOG", GRAY);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::print_ln(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::print(const char* fmt, ...)
{
    SpinlockGuard g(log_spin_);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);
}

void Log::debug(const char* fmt, ...)
{
    if (!is_debug_)
        return;

    SpinlockGuard g(log_spin_);

    log_prefix("DEBUG", ORANGE);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
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
    const auto int_part = static_cast<u32>(val);
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
            usize min_width = 0;
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
                            t_->put_char(str[count]);
                            count++;
                        }
                    }
                    else
                    {
                        t_->print(str);
                    }
                    break;
                }
            case 'u':
            case 'x':
                {
                    u64 val = (long_long || long_flag)
                                       ? __builtin_va_arg(args, u64)
                                       : __builtin_va_arg(args, u32);
                    int base = (specifier == 'x') ? 16 : 10;
                    u_int_to_str(val, buffer, base);

                    // Padding manuell
                    const usize len = strlen(buffer);
                    for (usize i = len; i < min_width; i++)
                        t_->put_char(pad_char);

                    t_->print(buffer);
                    break;
                }
            case 'c':
                {
                    int val = __builtin_va_arg(args, int);
                    t_->put_char(static_cast<char>(val));
                    break;
                }
            case 'd':
                {
                    i64 val = (long_long || long_flag)
                                      ? __builtin_va_arg(args, i64)
                                      : __builtin_va_arg(args, i32);
                    if (val < 0)
                    {
                        t_->put_char('-');
                        val = -val;
                    }
                    u_int_to_str(static_cast<u64>(val), buffer, 10);
                    usize len = strlen(buffer);
                    for (usize i = len; i < min_width; i++)
                        t_->put_char(pad_char);
                    t_->print(buffer);
                    break;
                }
   /*         case 'f':
                {
                    // Float-Support
                    double val = __builtin_va_arg(args, double);
                    int frac_digits = (precision >= 0) ? precision : 6;
                    float_to_str(static_cast<float>(val), buffer, frac_digits);
                    usize len = strlen(buffer);
                    for (usize i = len; i < min_width; i++)
                        t->put_char(pad_char);
                    t->print(buffer);
                    break;
                }*/
            case 'p':
                {
                    uptr val = __builtin_va_arg(args, uptr);
                    t_->print("0x");
                    u_int_to_str(val, buffer, 16);
                    usize len = strlen(buffer);
                    for (usize i = len; i < min_width; i++)
                        t_->put_char('0');
                    t_->print(buffer);
                    break;
                }
            case '%':
                t_->put_char('%');
                break;
            default:
                t_->put_char('%');
                t_->put_char(specifier);
                break;
            }
        }
        else
        {
            t_->put_char(chr);
        }
    }
}
