#ifndef LOGGING_HPP
#define LOGGING_HPP

#if __cplusplus < 202002L
    #error "logging.hpp requires C++20 or later"
#endif

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <source_location>
#include <string_view>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    NONE
};

inline bool          _log_init_done  = false;
inline bool          log_enabled     = false;
inline LogLevel      current_log_level = LogLevel::INFO;
inline std::ostream* log_output        = &std::clog;

constexpr std::string_view level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKWN";
    }
}

inline void set_log_level(LogLevel level)    { current_log_level = level; }
inline void set_log_output(std::ostream& os) { log_output = &os; }

// orender_log_init — reads env vars once and configures logging state.
// ORENDER_INSTR_LEVEL: "debug"|"info"|"warn"|"error" → enables logging at that threshold.
//   Absent or unrecognized → logging disabled.
// ORENDER_INSTR_OUTPUT: "stderr"|"stdout"|<filepath> → output destination (default: stderr).
// Idempotent: returns immediately on subsequent calls.
// Not thread-safe for concurrent first calls; startup races are benign (same env, same result).
inline void orender_log_init() {
    if (_log_init_done) return;
    _log_init_done = true;

    const char *lv = std::getenv("ORENDER_INSTR_LEVEL");
    if (!lv) return;

    std::string_view lvs(lv);
    if      (lvs == "debug") current_log_level = LogLevel::DEBUG;
    else if (lvs == "info")  current_log_level = LogLevel::INFO;
    else if (lvs == "warn")  current_log_level = LogLevel::WARN;
    else if (lvs == "error") current_log_level = LogLevel::ERROR;
    else    return;  // unrecognized level → stay disabled

    log_enabled = true;

    const char *out = std::getenv("ORENDER_INSTR_OUTPUT");
    if (!out || std::string_view(out) == "stderr") {
        log_output = &std::cerr;
    } else if (std::string_view(out) == "stdout") {
        log_output = &std::cout;
    } else {
        static std::ofstream file_out(out, std::ios::app);
        log_output = file_out.is_open() ? static_cast<std::ostream*>(&file_out) : &std::cerr;
    }
}

struct LogMessage {
    std::string_view     fmt;
    std::source_location loc;

    template <typename S>
    constexpr LogMessage(const S& s,
                         std::source_location l = std::source_location::current())
        : fmt(s), loc(l) {}
};

template <typename... Args>
void log(LogLevel level, LogMessage msg, Args&&... args) {
    if (!_log_init_done) orender_log_init();
    if (!log_enabled || level < current_log_level || !log_output)
        return;

    const auto now   = std::chrono::system_clock::now();
    const auto ms    = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now.time_since_epoch()) % 1000;
    const auto now_t = std::chrono::system_clock::to_time_t(now);
    struct tm  tm_buf{};
    localtime_r(&now_t, &tm_buf);

    char time_str[9];
    std::strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);

    const std::string message = std::vformat(msg.fmt, std::make_format_args(args...));
    *log_output << std::format("[{}.{:03d}] [{}] {}:{}: {}\n",
                               time_str, ms.count(),
                               level_to_string(level),
                               msg.loc.file_name(), msg.loc.line(),
                               message);
    log_output->flush();
}

#define log_debug(fmt, ...) log(LogLevel::DEBUG, LogMessage{fmt} __VA_OPT__(,) __VA_ARGS__)
#define log_info(fmt, ...)  log(LogLevel::INFO,  LogMessage{fmt} __VA_OPT__(,) __VA_ARGS__)
#define log_warn(fmt, ...)  log(LogLevel::WARN,  LogMessage{fmt} __VA_OPT__(,) __VA_ARGS__)
#define log_error(fmt, ...) log(LogLevel::ERROR, LogMessage{fmt} __VA_OPT__(,) __VA_ARGS__)

#endif // LOGGING_HPP
