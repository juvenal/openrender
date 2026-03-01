#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <time.h>
#include <string.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE
} LogLevel;

#ifdef LOGGING_IMPLEMENTATION
    FILE* _log_output = NULL;
    LogLevel _log_level = LOG_LEVEL_INFO;
#else
    extern FILE* _log_output;
    extern LogLevel _log_level;
#endif

#define LOG_INIT(stream, level) do { \
    _log_output = (stream); \
    _log_level = (level); \
} while(0)

#define LOG_SET_LEVEL(level) do { \
    _log_level = (level); \
} while(0)

#define LOG_SET_OUTPUT(stream) do { \
    _log_output = (stream); \
} while(0)

static inline const char* _log_level_str(LogLevel level) {
    switch(level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        default: return "UNKWN";
    }
}

/* ##__VA_ARGS__ is a widely-supported GNU extension that removes a leading comma
 * when __VA_ARGS__ is empty, allowing LOG_DEBUG("msg") without extra format args.
 * Clang names it "-Wgnu-zero-variadic-macro-arguments"; GCC lumps it under
 * "-Wpedantic" with no dedicated sub-flag. Both compiler pragma blocks are provided.
 * Note: on GCC the pedantic warning fires at call sites, not at the definition;
 * the primary C-file suppression comes from the oshader CMakeLists.txt config. */
#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpedantic"
#endif

#define LOG(level, fmt, ...) do { \
    if ((level) >= _log_level && _log_output) { \
        struct timespec _ts; \
        timespec_get(&_ts, TIME_UTC); \
        struct tm *_tm = localtime(&_ts.tv_sec); \
        char _time_buf[24]; \
        strftime(_time_buf, sizeof(_time_buf), "%H:%M:%S", _tm); \
        snprintf(_time_buf + 8, sizeof(_time_buf) - 8, ".%03ld", _ts.tv_nsec / 1000000); \
        fprintf(_log_output, "[%s] [%s] %s:%d: " fmt "\n", \
                _time_buf, _log_level_str(level), __FILE__, __LINE__, ##__VA_ARGS__); \
        fflush(_log_output); \
    } \
} while(0)

#define LOG_DEBUG(fmt, ...) LOG(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#ifdef __clang__
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif // LOGGING_H
