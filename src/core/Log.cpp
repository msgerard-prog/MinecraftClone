#include "core/Log.h"

#include <cstdarg>
#include <cstdio>

namespace mc::log {

void write(Level level, const char* fmt, ...) {
    static constexpr const char* kNames[] = {"debug", "info", "warn", "error"};
    std::fprintf(stderr, "[%s] ", kNames[static_cast<int>(level)]);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
    if (level == Level::Error) {
        std::fflush(stderr);
    }
}

} // namespace mc::log
