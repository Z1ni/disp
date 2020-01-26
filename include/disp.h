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

#ifndef _DISP_H_
#define _DISP_H_

#define _UNICODE
#define UNICODE

#include "app.h"

void free_monitors(app_ctx_t *ctx);
void populate_display_data(app_ctx_t *ctx);
BOOL change_display_orientation(app_ctx_t *ctx, monitor_t *mon, BYTE orientation);
int read_config(app_ctx_t *ctx, BOOL reload);
void flag_matching_presets(app_ctx_t *ctx);
void reload(app_ctx_t *ctx);
void apply_preset(app_ctx_t *ctx, display_preset_t *preset);
void apply_preset_by_name(app_ctx_t *ctx, const wchar_t *name);
void save_current_config(app_ctx_t *ctx);

#endif
