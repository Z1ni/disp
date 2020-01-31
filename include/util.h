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

#ifndef _UTIL_H_
#define _UTIL_H_

#include <windows.h>

void get_error_msg(const int err_code, wchar_t **out_msg);

#endif