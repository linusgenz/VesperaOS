#ifndef LOG_H
#define LOG_H

#include <kernel/basic_renderer.h>
#include <kernel/sync/spinlock.h>
#include <kernel/sync/mutex.h>
#include "graphics.h"

class Log {
public:
    static void SetRenderer(BasicRenderer* r);

    static void Info(const char* fmt, ...);
    static void Ok(const char* fmt, ...);
    static void Warning(const char* fmt, ...);
    static void Error(const char* fmt, ...);
    static void LogMsg(const char* fmt, ...);

    static void debug(const char *fmt, ...);

    static void PrintLn(const char *fmt, ...);
    static void Print(const char *fmt, ...);

    static void init();

    static void enableDebug();

private:
    static BasicRenderer* renderer;
    static void print_formatted(const char *fmt, __builtin_va_list args);
    static spinlock_t log_lock;

    static spinlock_t log_spin;
    static kernel::mutex_t log_mutex;
    static bool initialized;
    static bool is_debug;
};

#endif // LOG_H
