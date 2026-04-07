#ifndef LOG_H
#define LOG_H

#include <vespera/sync/mutex.h>
#include <vespera/sync/spinlock.h>

#include "terminal.h"

class IScreenRenderer;

class Log {
   public:
    static auto set_terminal(Terminal* t) -> void;

    static void info(const char* fmt, ...);
    static void ok(const char* fmt, ...);
    static void warning(const char* fmt, ...);
    static void error(const char* fmt, ...);
    static void log_msg(const char* fmt, ...);

    static void debug(const char* fmt, ...);

    static void print_ln(const char* fmt, ...);
    static void print(const char* fmt, ...);

    static void print(const char* fmt, __builtin_va_list args);

    static void init();
    static void log_prefix(const char* tag, u32 tag_fg);

    static void enable_debug();
    static void disable_debug();

   private:
    static Terminal* t_;
    static void print_formatted(const char* fmt, __builtin_va_list args);
    static Spinlock log_lock_;

    static Spinlock log_spin_;
    static kernel::Mutex log_mutex_;
    static bool initialized_;
    static bool is_debug_;
};

#endif  // LOG_H
