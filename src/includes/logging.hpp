#ifndef LOGGING_HPP
#define LOGGING_HPP

#if __cplusplus < 202002L
    #error "logging.hpp requires C++20 or later"
#endif

#include <chrono>
#include <ctime>
#include <cstring>
#include <format>
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
    if (level < current_log_level || !log_output)
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
