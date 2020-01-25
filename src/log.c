/*
disp - Simple display settings manager for Windows 7+
Copyright (C) 2019-2020 Mark "zini" Mäkinen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define UNICODE
#include <time.h>
#include <strsafe.h>
#include "log.h"

static wchar_t *log_level_str[6] = {L"TRACE", L"DEBUG", L"INFO", L"WARNING", L"ERROR", L"NONE"};
static const wchar_t *log_level_colors[5] = {L"\x1b[94m", L"\x1b[36m", L"\x1b[32m", L"\x1b[33m", L"\x1b[31m"};
static int log_level = LOG_WARNING;
static int file_log_level = LOG_NONE;
static FILE *logfile = NULL;

void log_init(void) {
    // Open logfile, etc.
    if (file_log_level != LOG_NONE) {
        logfile = fopen("disp.log", "w");
    }
    log_trace(L"File log level: %s, console log level: %s", log_level_str[file_log_level], log_level_str[log_level]);
    log_info(L"Logging initialized");
}

void log_finish(void) {
    log_info(L"Finishing logging");
    // Flush & close logfile
    if (logfile != NULL) {
        fflush(logfile);
        fclose(logfile);
    }
}

void log_set_level(int level) {
    log_level = level;
}

void log_set_file_level(int level) {
    file_log_level = level;
}

void log_log(int level, const wchar_t *format, ...) {
    if (level < log_level && level < file_log_level) {
        return;
    }
    time_t now = time(NULL);
    struct tm *time_info = localtime(&now);

    wchar_t time_buf[28] = {0};
    wcsftime((wchar_t *) time_buf, 28, L"%Y-%m-%d %H:%M:%S", time_info);

    va_list args;
    va_start(args, format);

    wchar_t msg[1024] = {0};
    StringCbVPrintf((wchar_t *) msg, 1024, format, args);

    wchar_t level_str_color[50] = {0};
    wchar_t level_str_nocolor[50] = {0};

    StringCbPrintf((wchar_t *) level_str_color, 50, L"%s%-7s\x1b[0m", log_level_colors[level], log_level_str[level]);
    StringCbPrintf((wchar_t *) level_str_nocolor, 50, L"%-7s", log_level_str[level]);

    wchar_t log_entry[1536] = {0};
    wchar_t file_log_entry[1536] = {0};
    wchar_t *con_level_str_p = NULL;
#ifdef LOG_COLOR_OUTPUT
    con_level_str_p = (wchar_t *) &level_str_color;
#else
    con_level_str_p = (wchar_t *) &level_str_nocolor;
#endif
    StringCbPrintf((wchar_t *) log_entry, 1536, L"[%s] [%s] %s\n", time_buf, con_level_str_p, msg);
    StringCbPrintf((wchar_t *) file_log_entry, 1536, L"[%s] [%s] %s\n", time_buf, level_str_nocolor, msg);

    // TODO: Print to stderr?
    // Write to stdout
    if (level >= log_level) {
        wprintf(log_entry);
        fflush(stdout);
    }

    // Write to file
    if (level >= file_log_level && logfile != NULL) {
        fwprintf(logfile, file_log_entry);
        fflush(logfile);
    }

    va_end(args);
}
