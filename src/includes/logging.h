#ifndef LOGGING_H
#define LOGGING_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE
} LogLevel;

#ifdef LOGGING_IMPLEMENTATION
    FILE*    _log_output   = NULL;
    LogLevel _log_level    = LOG_LEVEL_INFO;
    bool     _log_init_done = false;
    bool     _log_enabled   = false;
#else
    extern FILE*    _log_output;
    extern LogLevel _log_level;
    extern bool     _log_init_done;
    extern bool     _log_enabled;
#endif

// orender_log_init — reads env vars once and configures logging state.
// ORENDER_INSTR_LEVEL: "debug"|"info"|"warn"|"error" → enables logging at that threshold.
//   Absent or unrecognized → logging disabled.
// ORENDER_INSTR_OUTPUT: "stderr"|"stdout"|<filepath> → output destination (default: stderr).
// Idempotent: returns immediately on subsequent calls.
#ifdef __cplusplus
extern "C" {
#endif
#ifdef LOGGING_IMPLEMENTATION
void orender_log_init(void) {
    if (_log_init_done) return;
    _log_init_done = true;

    const char *lv = getenv("ORENDER_INSTR_LEVEL");
    if (!lv) return;

    if      (strcmp(lv, "debug") == 0) _log_level = LOG_LEVEL_DEBUG;
    else if (strcmp(lv, "info")  == 0) _log_level = LOG_LEVEL_INFO;
    else if (strcmp(lv, "warn")  == 0) _log_level = LOG_LEVEL_WARN;
    else if (strcmp(lv, "error") == 0) _log_level = LOG_LEVEL_ERROR;
    else    return;  // unrecognized level → stay disabled

    _log_enabled = true;

    const char *out = getenv("ORENDER_INSTR_OUTPUT");
    if      (!out || strcmp(out, "stderr") == 0) _log_output = stderr;
    else if (strcmp(out, "stdout") == 0)          _log_output = stdout;
    else {
        _log_output = fopen(out, "a");
        if (!_log_output) _log_output = stderr;
    }
}
#else
void orender_log_init(void);
#endif
#ifdef __cplusplus
}
#endif

#define LOG_SET_LEVEL(level) do { _log_level = (level); } while(0)
#define LOG_SET_OUTPUT(stream) do { _log_output = (stream); } while(0)

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
    if (!_log_init_done) orender_log_init(); \
    if (_log_enabled && (level) >= _log_level && _log_output) { \
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
