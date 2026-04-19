/*
 * log.h — structured logging
 *
 * INCLUDE ORDER RULE (macOS/BSD):
 *   Always include log.h LAST in every .c file.
 *   <ev.h> and PHP headers pull in <sys/syslog.h> which defines
 *   LOG_DEBUG=7, LOG_INFO=6, LOG_ERROR=3 as integers.  The #undef
 *   block below is placed OUTSIDE the include guard so it re-executes
 *   on every include, ensuring our function-like macros always win.
 */

#ifndef LUX_LOG_H
#define LUX_LOG_H

#include <stdio.h>
#include <time.h>

typedef enum {
    SRV_LOG_DEBUG,
    SRV_LOG_INFO,
    SRV_LOG_WARN,
    SRV_LOG_ERROR
} log_level_t;

void server_log(log_level_t level, const char *file, int line, const char *fmt,
                ...) __attribute__((format(printf, 4, 5)));

#endif /* SERVER_LOG_H — type definitions end here */

/* ---- outside guard: re-runs on every include ---- */
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_WARN
#undef LOG_WARN
#endif
#ifdef LOG_ERROR
#undef LOG_ERROR
#endif
#ifdef LOG_ERR
#undef LOG_ERR
#endif

#define LOG(lvl, fmt, ...)                                                     \
    server_log(lvl, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(SRV_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG(SRV_LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG(SRV_LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG(SRV_LOG_ERROR, fmt, ##__VA_ARGS__)
