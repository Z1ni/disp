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

#define UNICODE
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <Strsafe.h>
#include "config.h"

static wchar_t *mbstowcsdup(const char *src, size_t *dest_sz) {
    int wbuf_size = mbstowcs(NULL, src, 0);
    if (wbuf_size < 0) {
        // wfprintf to stderr?
        // TODO: Return NULL instead?
        wprintf(L"wbuf_size query failed\n");
        abort();
    }
    // Allocate memory
    // TODO: Don't trust the src size (user controllable)
    wchar_t *tmp;
    tmp = calloc(wbuf_size + 1, sizeof(wchar_t));
    if (mbstowcs(tmp, src, wbuf_size + 1) <= 0) {
        // TODO: Return NULL instead?
        wprintf(L"mbstowcs failed\n");
        abort();
    }
    if (dest_sz != NULL) {
        *dest_sz = wbuf_size;
    }
    return tmp;
}

static const char *wcstombs_alloc(const wchar_t *src, size_t *dest_sz) {
    // Convert (wchar_t *) name to (char *)
    size_t converted_count = 0;
    size_t src_sz = wcslen(src) + 1;
    size_t result_sz = (src_sz * sizeof(char)) * 2; // Each multibyte character is two bytes
    char *result = calloc(src_sz, result_sz);
    if (wcstombs_s(&converted_count, result, result_sz, src, _TRUNCATE) != 0) {
        fwprintf(stderr, L"wcstombs_s failed: 0x%04X\n", errno);
        abort();
    }
    if (dest_sz != NULL) {
        *dest_sz = converted_count;
    }
    return result;
}

static void set_error_info(app_config_t *app_config, const config_t *libconfig_config) {
    // Get error info from libconfig
    config_error_t err_type = config_error_type(libconfig_config);
    if (err_type == CONFIG_ERR_NONE) {
        return;
    }

    const wchar_t *w_err_str = mbstowcsdup(config_error_text(libconfig_config), NULL);
    int err_line = config_error_line(libconfig_config);

    wchar_t *w_err_file = NULL;
    const char *err_file = config_error_file(libconfig_config);
    if (err_file != NULL) {
        w_err_file = mbstowcsdup(err_file, NULL);
    }

    // Set app config error info
    // Construct the error string
    wchar_t combined_err_str[512] = {0};
    if (err_type == CONFIG_ERR_FILE_IO) {
        if (w_err_file == NULL) {
            StringCbPrintf((wchar_t *) &combined_err_str, 511, L"%s", w_err_str);
        } else {
            StringCbPrintf((wchar_t *) &combined_err_str, 511, L"%s in %s", w_err_str, w_err_file);
            free(w_err_file);
        }
    } else {
        if (w_err_file == NULL) {
            StringCbPrintf((wchar_t *) &combined_err_str, 511, L"%s at line %d", w_err_str, err_line);
        } else {
            StringCbPrintf((wchar_t *) &combined_err_str, 511, L"%s in %s at line %d", w_err_str, w_err_file, err_line);
            free(w_err_file);
        }
    }
    free((wchar_t *) w_err_str);
    memcpy(app_config->error_str, combined_err_str, 512);

    // Log
    fwprintf(stderr, L"libconfig error: %s\n", combined_err_str);
    fflush(stderr);
}

int disp_config_read_file(const char *path, app_config_t *app_config) {
    config_t conf;

    config_init(&conf);

    if (config_read_file(&conf, path) == CONFIG_FALSE) {
        // Fail
        set_error_info(app_config, &conf);
        config_destroy(&conf);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    if (config_lookup_bool(&conf, "app.notify_on_start", &(app_config->notify_on_start)) == CONFIG_FALSE) {
        set_error_info(app_config, &conf);
        config_destroy(&conf);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    // Get configs
    config_setting_t *disp_presets = config_lookup(&conf, "presets");
    if (disp_presets == NULL) {
        return DISP_CONFIG_ERROR_GENERAL;
    }

    int disp_presets_len = config_setting_length(disp_presets);

    app_config->preset_count = (size_t) disp_presets_len;

    app_config->presets = calloc(disp_presets_len, sizeof(display_preset_t *));

    for (int i = 0; i < disp_presets_len; i++) {
        display_preset_t *preset_entry = calloc(1, sizeof(display_preset_t));
        preset_entry->applicable = 0;

        // config_setting_get_elem can also return NULL if the index is out of range - use it in while?
        config_setting_t *elem = config_setting_get_elem(disp_presets, i);

        // Name
        const char *name;
        config_setting_lookup_string(elem, "name", &name);
        preset_entry->name = mbstowcsdup(name, NULL);

        // Displays
        config_setting_t *disp_settings = config_setting_get_member(elem, "displays");
        if (disp_settings == NULL) {
            // No display settings
            // TODO: Is this free working here?
            disp_config_destroy(app_config);
            return DISP_CONFIG_ERROR_GENERAL;
        }
        int disp_settings_len = config_setting_length(disp_settings);
        preset_entry->display_count = (size_t) disp_settings_len;
        preset_entry->display_conf = calloc(disp_settings_len, sizeof(display_settings_t *));

        for (int a = 0; a < disp_settings_len; a++) {
            display_settings_t *display_entry = calloc(1, sizeof(display_settings_t));

            config_setting_t *disp_elem = config_setting_get_elem(disp_settings, a);
            const char *display_path;

            config_setting_lookup_string(disp_elem, "display", &display_path);
            config_setting_lookup_int(disp_elem, "orientation", &(display_entry->orientation));
            config_setting_t *pos = config_setting_get_member(disp_elem, "position");
            config_setting_lookup_int(pos, "x", &(display_entry->pos_x));
            config_setting_lookup_int(pos, "y", &(display_entry->pos_y));
            config_setting_t *resolution = config_setting_get_member(disp_elem, "resolution");
            config_setting_lookup_int(resolution, "width", &(display_entry->width));
            config_setting_lookup_int(resolution, "height", &(display_entry->height));

            display_entry->device_path = mbstowcsdup(display_path, NULL);

            preset_entry->display_conf[a] = display_entry;
        }

        app_config->presets[i] = preset_entry;
    }

    config_destroy(&conf);

    return DISP_CONFIG_SUCCESS;
}

int disp_config_save_file(const char *path, app_config_t *app_config) {
    // Create config_t
    config_t config = {0};

    config_init(&config);

    config_setting_t *root = config_root_setting(&config);
    config_setting_t *app_group, *presets_list, *setting;

    // Create app group
    app_group = config_setting_add(root, "app", CONFIG_TYPE_GROUP);

    // Add app group settings
    setting = config_setting_add(app_group, "notify_on_start", CONFIG_TYPE_BOOL);
    config_setting_set_bool(setting, app_config->notify_on_start);

    // Create presets list
    presets_list = config_setting_add(root, "presets", CONFIG_TYPE_LIST);

    // Add presets
    for (size_t i = 0; i < app_config->preset_count; i++) {
        config_setting_t *preset_entry = config_setting_add(presets_list, NULL, CONFIG_TYPE_GROUP);

        // Name
        const char *name_str = wcstombs_alloc(app_config->presets[i]->name, NULL);

        setting = config_setting_add(preset_entry, "name", CONFIG_TYPE_STRING);
        config_setting_set_string(setting, name_str);

        // libconfig copies the passed string so we can now free the converted local version
        free((char *) name_str);

        // Displays
        config_setting_t *displays_list = config_setting_add(preset_entry, "displays", CONFIG_TYPE_LIST);

        for (size_t a = 0; a < app_config->presets[i]->display_count; a++) {
            display_settings_t *disp_settings = app_config->presets[i]->display_conf[a];

            config_setting_t *display_entry = config_setting_add(displays_list, NULL, CONFIG_TYPE_GROUP);

            // Display path
            // TODO: Does this need to be escaped?
            const char *display_path = wcstombs_alloc(disp_settings->device_path, NULL);

            setting = config_setting_add(display_entry, "display", CONFIG_TYPE_STRING);
            config_setting_set_string(setting, display_path);
            free((char *) display_path);

            // Orientation
            setting = config_setting_add(display_entry, "orientation", CONFIG_TYPE_INT);
            config_setting_set_int(setting, disp_settings->orientation);

            // Position
            config_setting_t *position = config_setting_add(display_entry, "position", CONFIG_TYPE_GROUP);
            // X
            setting = config_setting_add(position, "x", CONFIG_TYPE_INT);
            config_setting_set_int(setting, disp_settings->pos_x);
            // Y
            setting = config_setting_add(position, "y", CONFIG_TYPE_INT);
            config_setting_set_int(setting, disp_settings->pos_y);

            // Resolution
            config_setting_t *resolution = config_setting_add(display_entry, "resolution", CONFIG_TYPE_GROUP);
            // Width
            setting = config_setting_add(resolution, "width", CONFIG_TYPE_INT);
            config_setting_set_int(setting, disp_settings->width);
            // Height
            setting = config_setting_add(resolution, "height", CONFIG_TYPE_INT);
            config_setting_set_int(setting, disp_settings->height);
        }
    }

    if (config_write_file(&config, path) == CONFIG_FALSE) {
        set_error_info(app_config, &config);
        config_destroy(&config);
        return DISP_CONFIG_ERROR_GENERAL;
    }
    config_destroy(&config);

    return DISP_CONFIG_SUCCESS;
}

wchar_t *disp_config_get_err_msg(const app_config_t *config) {
    return (wchar_t *) config->error_str;
}

int disp_config_get_presets(const app_config_t *config, display_preset_t ***presets) {
    // Returns count of presets or error
    *presets = config->presets;
    return config->preset_count;
}

int disp_config_preset_get_display(const display_preset_t *preset, const wchar_t *path, display_settings_t **settings) {
    // returns DISP_CONFIG_SUCCESS or error
    for (size_t i = 0; i < preset->display_count; i++) {
        if (wcscmp(preset->display_conf[i]->device_path, path) != 0) {
            continue;
        }
        // Match
        // TODO: Can this null check fail? settings is probably uninitialized
        if (settings != NULL) {
            *settings = preset->display_conf[i];
        }
        return DISP_CONFIG_SUCCESS;
    }
    return DISP_CONFIG_ERROR_NO_ENTRY;
}

int disp_config_preset_matches_current(const display_preset_t *preset, const app_ctx_t *ctx) {
    // Check that the current monitor setup contains all the needed displays
    if (ctx->monitor_count != preset->display_count) {
        return DISP_CONFIG_ERROR_NO_MATCH;
    }
    for (size_t i = 0; i < ctx->monitor_count; i++) {
        if (disp_config_preset_get_display(preset, ctx->monitors[i].device_id, NULL) == DISP_CONFIG_ERROR_NO_ENTRY) {
            // No match
            return DISP_CONFIG_ERROR_NO_MATCH;
        }
    }
    // Full match
    return DISP_CONFIG_SUCCESS;
}

int disp_config_create_preset(const wchar_t *name, app_ctx_t *ctx) {
    // Get display count
    size_t display_count = ctx->monitor_count;

    display_preset_t *preset = calloc(1, sizeof(display_preset_t));
    display_settings_t *disp_settings;
    monitor_t cur_monitor;

    preset->name = wcsdup(name);
    preset->display_count = display_count;
    // Alloc memory for all display settings
    preset->display_conf = calloc(display_count, sizeof(display_settings_t *));

    for (size_t i = 0; i < display_count; i++) {
        cur_monitor = ctx->monitors[i];
        // Alloc memory for display settings for this monitor
        disp_settings = calloc(1, sizeof(display_settings_t));
        // Copy path
        disp_settings->device_path = wcsdup((wchar_t *) cur_monitor.device_id);
        // Copy other info
        disp_settings->orientation = cur_monitor.devmode.dmDisplayOrientation;
        disp_settings->pos_x = cur_monitor.virt_pos.x;
        disp_settings->pos_y = cur_monitor.virt_pos.y;
        disp_settings->width = cur_monitor.rect.right - cur_monitor.rect.left;
        disp_settings->height = cur_monitor.rect.bottom - cur_monitor.rect.top;

        preset->display_conf[i] = disp_settings;
    }

    // Add to config presets
    app_config_t *config = &(ctx->config);
    // Reallocate
    void *realloc_ptr = realloc(config->presets, (config->preset_count + 1) * sizeof(display_preset_t *));
    if (realloc_ptr == NULL) {
        // Realloc failed
        fwprintf(stderr, L"realloc failed\n");
        abort();
    }
    config->presets = realloc_ptr;

    config->presets[config->preset_count] = preset;
    config->preset_count = config->preset_count + 1;

    return DISP_CONFIG_SUCCESS;
}

void disp_config_destroy(app_config_t *config) {
    if (config->preset_count > 0) {
        for (size_t i = 0; i < config->preset_count; i++) {
            display_preset_t *preset = config->presets[i];
            if (preset->display_count > 0) {
                for (size_t a = 0; a < preset->display_count; a++) {
                    free((wchar_t *) preset->display_conf[a]->device_path);
                    free(preset->display_conf[a]);
                }
                free(preset->display_conf);
            }
            free((wchar_t *) preset->name);
            free(preset);
        }
        free(config->presets);
    }
}
