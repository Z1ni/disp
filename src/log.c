#define UNICODE
#include <time.h>
#include <strsafe.h>
#include "log.h"

static wchar_t *log_level_str[5] = {L"TRACE", L"DEBUG", L"INFO", L"WARNING", L"ERROR"};

static const wchar_t *log_level_colors[5] = {L"\x1b[94m", L"\x1b[36m", L"\x1b[32m", L"\x1b[33m", L"\x1b[31m"};

void log_log(int level, const wchar_t *format, ...) {
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);

    wchar_t time_buf[28] = {0};
    wcsftime((wchar_t *) time_buf, 28, L"%Y-%m-%d %H:%M:%S", time_info);

    va_list args;
    va_start(args, format);

    wchar_t msg[1024] = {0};
    StringCbVPrintf((wchar_t *) msg, 1024, format, args);

    wchar_t level_str[50] = {0};
#ifdef LOG_COLOR_OUTPUT
    StringCbPrintf((wchar_t *) level_str, 50, L"%s%-7s\x1b[0m", log_level_colors[level], log_level_str[level]);
#else
    StringCbPrintf((wchar_t *) level_str, 50, L"%-7s", log_level_str[level]);
#endif

    wchar_t log_entry[1536] = {0};
    StringCbPrintf((wchar_t *) log_entry, 1536, L"[%s] [%s] %s\n", time_buf, level_str, msg);

    // TODO: Print to stderr?
    wprintf(log_entry);
    fflush(stdout);

    va_end(args);
}
