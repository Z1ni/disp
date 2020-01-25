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

#ifndef _UI_H_
#define _UI_H_

#include <strsafe.h>
#include "context.h"

typedef struct {
    wchar_t preset_name[64];
    BOOL cancel;
} preset_dialog_data_t;

void create_tray_menu(app_ctx_t *ctx);
void show_notification_message(app_ctx_t *ctx, STRSAFE_LPCWSTR format, ...);
void show_save_dialog(app_ctx_t *ctx, preset_dialog_data_t *data);
HWND init_main_window(app_ctx_t *ctx);
int init_virt_desktop_window(app_ctx_t *ctx);
HWND show_virt_desktop_window(app_ctx_t *ctx);
int create_tray_icon(app_ctx_t *ctx, HWND hwnd);

#endif
