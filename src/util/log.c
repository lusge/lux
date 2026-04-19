#include "log.h"
#include <stdarg.h>
#include <unistd.h>

static const char *s_level_str[]  = {"DEBUG", "INFO ", "WARN ", "ERROR"};
static const char *s_level_color[] = {"\033[37m","\033[32m","\033[33m","\033[31m"};

void server_log(log_level_t level, const char *file, int line,
                const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char tbuf[16];
    strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm);
    fprintf(stderr, "%s[%s][%d][%s:%d] ",
            s_level_color[level], s_level_str[level], getpid(), file, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\033[0m\n");
}
