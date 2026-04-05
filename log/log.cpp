//
// Created by Linus on 10.07.25.
//

#include <klib/string.h>
#include <vespera/graphics/colors.h>
#include <vespera/log.h>
#include <vespera/terminal.h>

Terminal* Log::t_ = nullptr;
Spinlock Log::log_spin_ = {};
kernel::Mutex Log::log_mutex_ = {};
bool Log::initialized_ = false;
bool Log::is_debug_ = false;

void Log::init() {
    //  initialized = true;
    //  kernel::mutex_init(&log_mutex);
    log_spin_.init("log_lock");
}

void Log::log_prefix(const char* tag, const u32 tag_fg) {
    t_->set_colour(WHITE, BLACK);
    t_->print("[ ");

    t_->set_colour(tag_fg, BLACK);
    t_->print(tag);

    t_->set_colour(WHITE, BLACK);
    t_->print(" ] ");
}

void Log::set_terminal(Terminal* t) {
    t_ = t;
}

void Log::enable_debug() {
    is_debug_ = true;
}

void Log::disable_debug() {
    is_debug_ = false;
}

void u_int_to_str(u64 value, char* buffer, const u8 base = 10, const bool prefix = false) {
    char temp[32];
    int i = 0;

    if (value == 0) {
        temp[i++] = '0';
    } else {
        while (value > 0) {
            const auto digits = "0123456789ABCDEF";
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

void Log::info(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    log_prefix("INFO", BLUE);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::ok(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    log_prefix("OK", GREEN);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::warning(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    log_prefix("WARNING", YELLOW);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::error(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    log_prefix("ERROR", RED);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::log_msg(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    log_prefix("LOG", GRAY);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::print_ln(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

void Log::print(const char* fmt, ...) {
    SpinlockGuard g(log_spin_);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);
}

void Log::print(const char* fmt, __builtin_va_list args) {
    SpinlockGuard g(log_spin_);

    print_formatted(fmt, args);
    t_->flush();
}

void Log::debug(const char* fmt, ...) {
    if (!is_debug_) return;

    SpinlockGuard g(log_spin_);

    log_prefix("DEBUG", ORANGE);

    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    print_formatted(fmt, args);
    __builtin_va_end(args);

    t_->print("\n");
    t_->flush();
}

static void term_write(void* ctx, char c) {
    static_cast<Terminal*>(ctx)->put_char(c);
}

void Log::print_formatted(const char* fmt, __builtin_va_list args) {
    vformat(term_write, t_, fmt, args);
}
