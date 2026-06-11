#ifndef LOG_H
#define LOG_H

#include <vespera/sync/mutex.h>
#include <vespera/sync/spinlock.h>

#include "terminal.h"

class IScreenRenderer;

#define FORMAT_FN [[gnu::format(printf, 1, 2)]]

class Log {
   public:
    Log() = delete;

    static auto set_terminal(Terminal* t) -> void;

    FORMAT_FN static void info(const char* fmt, ...);
    FORMAT_FN static void ok(const char* fmt, ...);
    FORMAT_FN static void warning(const char* fmt, ...);
    FORMAT_FN static void error(const char* fmt, ...);
    FORMAT_FN static void log_msg(const char* fmt, ...);
    FORMAT_FN static void debug(const char* fmt, ...);
    FORMAT_FN static void print_ln(const char* fmt, ...);
    FORMAT_FN static void print(const char* fmt, ...);

    static void print(const char* fmt, __builtin_va_list args);

    static void init();
    static void log_prefix(const char* tag, u32 tag_fg);

    static void enable_debug();
    static void disable_debug();

    /**
     * @brief Log only via xhci DbC. If DbC is not connected, this is a no-op.
     */
    static void log_dbc(const char* fmt, ...);

   private:
    static Terminal* t_;
    static void print_formatted_serial(const char* fmt, __builtin_va_list args);
    static void print_formatted(const char* fmt, __builtin_va_list args);
    static Spinlock log_lock_;

    static Spinlock log_spin_;
    static kernel::Mutex log_mutex_;
    static bool initialized_;
    static bool is_debug_;

    struct DbcLineBuf {
        u8 data[512]{};
        usize len = 0;
    };
    static DbcLineBuf dbc_buf_;

    /** @brief Appends a null-terminated string to dbc_buf_. */
    static void dbc_append(const char* s);

    /**
     * @brief Appends '\n' to dbc_buf_ and forwards the complete line to
     *        XhciDbcManager::write, then resets the buffer.
     *
     * No-ops gracefully if the DbC manager is not yet available - data is
     * handed off to the TX ring regardless; the ring drops it silently when
     * the port is not connected.
     */
    static void dbc_commit_line();

    /**
     * @brief Formats @p fmt into dbc_buf_ via va_copy, leaving @p args intact.
     *
     * Must be called *before* print_formatted() because print_formatted()
     * consumes the va_list.
     */
    static void print_formatted_dbc(const char* fmt, __builtin_va_list args);

    static void dbc_sink(void*, char c);

    static void serial_commit_line();

    static void serial_append(const char* s);

};

#endif  // LOG_H
