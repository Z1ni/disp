/*
disp - Simple display settings manager for Windows 7+
Copyright (C) 2019 Mark "zini" Mäkinen

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

#ifndef _LOG_H_
#define _LOG_H_

#include <stdarg.h>
#include <stdio.h>

#define LOG_TRACE 0
#define LOG_DEBUG 1
#define LOG_INFO 2
#define LOG_WARNING 3
#define LOG_ERROR 4
#define LOG_NONE 5

#define log_trace(...) log_log(LOG_TRACE, __VA_ARGS__)
#define log_debug(...) log_log(LOG_DEBUG, __VA_ARGS__)
#define log_info(...) log_log(LOG_INFO, __VA_ARGS__)
#define log_warning(...) log_log(LOG_WARNING, __VA_ARGS__)
#define log_error(...) log_log(LOG_ERROR, __VA_ARGS__)

void log_init(void);
void log_finish(void);
void log_set_level(int level);
void log_set_file_level(int level);
void log_log(int level, const wchar_t *format, ...);

#endif
