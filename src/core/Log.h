#pragma once

// Minimal printf-style logging to stderr. Not for per-frame use in release
// builds (formatting cost); fine for startup, errors and debug traces.

namespace mc::log {

enum class Level { Debug, Info, Warn, Error };

void write(Level level, const char* fmt, ...);

} // namespace mc::log

#define MC_LOG_DEBUG(...) ::mc::log::write(::mc::log::Level::Debug, __VA_ARGS__)
#define MC_LOG_INFO(...) ::mc::log::write(::mc::log::Level::Info, __VA_ARGS__)
#define MC_LOG_WARN(...) ::mc::log::write(::mc::log::Level::Warn, __VA_ARGS__)
#define MC_LOG_ERROR(...) ::mc::log::write(::mc::log::Level::Error, __VA_ARGS__)
