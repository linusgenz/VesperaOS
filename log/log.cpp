//
// Created by Linus on 10.07.25.
//

#include "../include/log.h"

#include <vector.h>

#include "../include/string.h"

BasicRenderer *Log::renderer = nullptr;
spinlock_t Log::log_spin = {};
kernel::mutex_t Log::log_mutex = {};
bool Log::initialized = false;
bool Log::is_debug = false;
Vector<LogEntry>* Log::log_queue = nullptr;

struct LogEntry {
    char text[256];
    Colour colour;
    size_t length;
};

void Log::init() {
    initialized = true;
    Log::log_queue = new Vector<LogEntry>;
    kernel::mutex_init(&log_mutex);
}

void Log::SetRenderer(BasicRenderer *r) {
    renderer = r;
}

void Log::enableDebug() {
    is_debug = true;
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

void Log::queue_log(const char *text, Colour col) {
    LogEntry entry{};
    size_t len = strlen(text);
    if (len > sizeof(entry.text) - 1) len = sizeof(entry.text) - 1;
    memcpy(entry.text, text, len);
    entry.text[len] = '\0';
    entry.length = len;
    entry.colour = col;

    log_queue->push_back(entry);
}


void Log::flush() {
    if (!renderer || !log_queue) return;

    if (!log_queue->empty()) {
      //  kernel::mutex_lock(&log_mutex);

        for (size_t i = 0; i < log_queue->size(); ++i) {
            LogEntry &entry = (*log_queue)[i];
            Colour old = renderer->get_colour();
            renderer->set_colour(entry.colour);
            renderer->print(entry.text, entry.length);
            renderer->set_colour(old);
        }

        log_queue->clear();
        renderer->present();
     //   kernel::mutex_unlock(&log_mutex);
    }
}

void Log::Info(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    queue_log("[  ");
    queue_log("INFO", Colour::BLUE);
    queue_log("   ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::Ok(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    queue_log("[   ");
    queue_log("OK", Colour::GREEN);
    queue_log("    ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::Warning(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    queue_log("[ ");
    queue_log("WARNING", Colour::YELLOW);
    queue_log(" ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::Error(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    queue_log("[  ");
    queue_log("ERROR", Colour::RED);
    queue_log("  ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::LogMsg(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    queue_log("[   ");
    queue_log("LOG", Colour::GRAY);
    queue_log("   ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::PrintLn(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::Print(const char *fmt, ...) {
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}

void Log::debug(const char *fmt, ...) {
    if (!is_debug) return;
    if (!initialized) {
        spinlock_guard g(log_spin);
    } else {
        kernel::mutex_lock(&log_mutex);
    }

    queue_log("[  ");
    queue_log("DEBUG", Colour::ORANGE);
    queue_log("  ] ");

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    queue_log_formatted(fmt, args);
    __builtin_va_end(args);

    queue_log("\n");

    if (initialized) {
        kernel::mutex_unlock(&log_mutex);
    }
}


static void float_to_str(float val, char *buf, int precision) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    // Ganzzahlteil
    uint32_t int_part = (uint32_t) val;
    float frac_part = val - (float) int_part;

    char int_buf[32];
    UIntToStr(int_part, int_buf, 10);
    char *p = int_buf;
    while (*p) {
        *buf++ = *p++;
    }

    *buf++ = '.';

    // Nachkommateil
    for (int i = 0; i < precision; i++) {
        frac_part *= 10.0f;
        int digit = (int) frac_part;
        *buf++ = '0' + digit;
        frac_part -= digit;
    }

    *buf = '\0';
}

void Log::queue_log_formatted(const char *fmt, __builtin_va_list args, Colour colour) {
    char buffer[256];

    size_t pos = 0;
    auto write_char = [&](char c) {
        if (pos < sizeof(buffer) - 1) buffer[pos++] = c;
    };

    char chr;
    const char *p = fmt;
    while ((chr = *p++) != 0) {
        if (chr == '%') {
            bool long_long = false;
            bool long_flag = false;
            char pad_char = ' ';
            int min_width = 0;

            if (*p == '0') {
                pad_char = '0';
                p++;
            }
            while (*p >= '0' && *p <= '9') {
                min_width = min_width * 10 + (*p - '0');
                p++;
            }
            if (*p == 'l') {
                p++;
                if (*p == 'l') {
                    long_long = true;
                    p++;
                } else long_flag = true;
            }

            char specifier = *p++;
            char numbuf[64];

            switch (specifier) {
                case 's': {
                    const char *s = __builtin_va_arg(args, const char*);
                    if (!s) s = "<null>";
                    while (*s) write_char(*s++);
                    break;
                }
                case 'd':
                case 'u':
                case 'x': {
                    uint64_t val = (long_long || long_flag)
                                       ? __builtin_va_arg(args, uint64_t)
                                       : __builtin_va_arg(args, uint32_t);
                    int base = (specifier == 'x') ? 16 : 10;
                    UIntToStr(val, numbuf, base);
                    for (char *c = numbuf; *c; ++c) write_char(*c);
                    break;
                }
                case 'f': {
                    double val = __builtin_va_arg(args, double);
                    float_to_str((float) val, numbuf, 6);
                    for (char *c = numbuf; *c; ++c) write_char(*c);
                    break;
                }
                case 'p': {
                    uintptr_t val = __builtin_va_arg(args, uintptr_t);
                    write_char('0');
                    write_char('x');
                    UIntToStr(val, numbuf, 16);
                    for (char *c = numbuf; *c; ++c) write_char(*c);
                    break;
                }
                case '%': write_char('%');
                    break;
                default: write_char('%');
                    write_char(specifier);
                    break;
            }
        } else {
            write_char(chr);
        }
    }

    buffer[pos] = '\0';
    LogEntry entry{};
    entry.colour = colour;
    memcpy(entry.text, buffer, pos + 1);
    entry.length = pos;
    log_queue->push_back(entry);
}
