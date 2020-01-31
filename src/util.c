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
#include "log.h"
#include "util.h"

void get_error_msg(const int err_code, wchar_t **out_msg) {
    int fm = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           NULL, err_code, 0, (LPWSTR) out_msg, 0, NULL);
    if (fm == 0) {
        log_error(L"FormatMessage failed with error 0x%08X", GetLastError());
        *out_msg = NULL;
    } else {
        // Remove trailing "\r\n"
        wchar_t *newline_pos = wcsstr(*out_msg, L"\r\n");
        if (newline_pos != NULL) {
            *newline_pos = '\0';
        }
    }
}