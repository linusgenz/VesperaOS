#ifndef LOG_H
#define LOG_H

#include "../kernel/include/basic_renderer.h"
#include "../kernel/scheduling/spinlock.h"
#include "graphics.h"

class Log {
public:
    static void SetRenderer(BasicRenderer* r);

    static void Info(const char* fmt, ...);
    static void Ok(const char* fmt, ...);
    static void Warning(const char* fmt, ...);
    static void Error(const char* fmt, ...);
    static void LogMsg(const char* fmt, ...);

    static void PrintLn(const char *fmt, ...);
    static void Print(const char *fmt, ...);

private:
    static BasicRenderer* renderer;
    static void PrintFormatted(const char *fmt, __builtin_va_list args);
    static spinlock_t log_lock;
};

#endif // LOG_H
