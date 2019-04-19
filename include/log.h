#ifndef _LOG_H_
#define _LOG_H_

#include <stdarg.h>
#include <stdio.h>

#define LOG_TRACE 0
#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARNING 3
#define LOG_ERROR 4

#define log_trace(...) log_log(LOG_TRACE, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __VA_ARGS__)
#define log_warning(...) log_log(LOG_WARNING, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __VA_ARGS__)

void log_log(int level, const wchar_t *format, ...);

#endif
