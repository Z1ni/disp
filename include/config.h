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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define DISP_CONFIG_SUCCESS 0
#define DISP_CONFIG_ERROR_GENERAL -1
#define DISP_CONFIG_ERROR_IO -2
#define DISP_CONFIG_ERROR_NO_ENTRY -3
#define DISP_CONFIG_ERROR_NO_MATCH -4

typedef struct {
    const wchar_t *device_path;
    int orientation;
    int pos_x;
    int pos_y;
    int width;
    int height;
} display_settings_t;

typedef struct {
    const wchar_t *name;
    size_t display_count;
    display_settings_t **display_conf;
    int applicable;
} display_preset_t;

typedef struct {
    int notify_on_start;
    size_t preset_count;
    display_preset_t **presets;
    wchar_t error_str[512];
} app_config_t;

#include "context.h"
#include "app.h"

int disp_config_get_appdata_path(wchar_t **config_path_out);
int disp_config_read_file(const wchar_t *path, app_config_t *config);
int disp_config_save_file(const wchar_t *path, app_config_t *config);
int disp_config_get_presets(const app_config_t *config,
                            display_preset_t ***presets); // returns count of presets or error
wchar_t *disp_config_get_err_msg(const app_config_t *config);
int disp_config_preset_get_display(const display_preset_t *preset, const wchar_t *path,
                                   display_settings_t **settings); // returns DISP_CONFIG_SUCCESS or error
int disp_config_preset_matches_current(const display_preset_t *preset, const app_ctx_t *ctx);
int disp_config_create_preset(const wchar_t *name, app_ctx_t *ctx);

void disp_config_destroy(app_config_t *config);

#endif
