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

#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include "disp.h"

typedef struct {
    app_config_t config;
    virt_size_t display_virtual_size;
    HMENU notif_menu;
    GUID notify_guid;
    HWND main_window_hwnd;
    BOOL display_update_in_progress;
    size_t monitor_count;
    monitor_t *monitors;
} app_ctx_t;

#endif