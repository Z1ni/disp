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
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <Strsafe.h>
#include <shlobj.h>
#include <jansson.h>
#include "config.h"
#include "log.h"

static wchar_t *mbstowcsdup(const char *src, size_t *dest_sz) {
    int wbuf_size = mbstowcs(NULL, src, 0);
    if (wbuf_size < 0) {
        // TODO: Return NULL instead?
        log_error(L"wbuf_size query failed");
        abort();
    }
    // Allocate memory
    // TODO: Don't trust the src size (user controllable)
    wchar_t *tmp;
    tmp = calloc(wbuf_size + 1, sizeof(wchar_t));
    if (mbstowcs(tmp, src, wbuf_size + 1) <= 0) {
        // TODO: Return NULL instead?
        log_error(L"mbstowcs failed");
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
        log_error(L"wcstombs_s failed: 0x%04X", errno);
        abort();
    }
    if (dest_sz != NULL) {
        *dest_sz = converted_count;
    }
    return result;
}

static void set_error_info(app_config_t *app_config, const json_error_t *json_err) {
    // Get error from Jansson
    const wchar_t *err_str = mbstowcsdup((const char *) json_err->text, NULL);
    const wchar_t *source_str = mbstowcsdup((const char *) json_err->source, NULL);

    HRESULT res = StringCbPrintf(app_config->error_str, 512, L"%s in %s at line %d, column %d", err_str, source_str,
                                 json_err->line, json_err->column);

    free((wchar_t *) err_str);
    free((wchar_t *) source_str);

    if (FAILED(res)) {
        log_error(L"Failed to format Jansson error string");
        return;
    }

    log_error(L"Jansson error: %s", app_config->error_str);
}

static void disp_config_preset_destroy(display_preset_t *preset) {
    if (preset == NULL) {
        return;
    }
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

void disp_config_destroy(app_config_t *config) {
    if (config->preset_count > 0) {
        if (config->presets == NULL) {
            return;
        }
        for (size_t i = 0; i < config->preset_count; i++) {
            display_preset_t *preset = config->presets[i];
            disp_config_preset_destroy(preset);
        }
        free(config->presets);
    }
}

int disp_config_get_appdata_path(wchar_t **config_path_out) {
    wchar_t conf_path[MAX_PATH] = {0};

    // Get local AppData folder path
    HRESULT path_result = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, conf_path);
    if (FAILED(path_result)) {
        int err = GetLastError();
        wchar_t *err_msg;
        get_error_msg(err, &err_msg);
        log_error(L"Failed to get local AppData path: %s (0x%08X)", err_msg, err);
        LocalFree(err_msg);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    if (!PathAppend((wchar_t *) conf_path, APP_FQN)) {
        // Failure
        int err = GetLastError();
        wchar_t *err_msg;
        get_error_msg(err, &err_msg);
        log_error(L"Failed to append to AppData path: %s (0x%08X)", err_msg, err);
        LocalFree(err_msg);
        return DISP_CONFIG_ERROR_GENERAL;
    }
    // Create the directory if needed
    if (!CreateDirectory(conf_path, NULL)) {
        int err = GetLastError();
        if (err != ERROR_ALREADY_EXISTS) {
            int err = GetLastError();
            wchar_t *err_msg;
            get_error_msg(err, &err_msg);
            log_error(L"Failed to create local AppData directory: %s (0x%08X)", err_msg, err);
            LocalFree(err_msg);
            return DISP_CONFIG_ERROR_GENERAL;
        }
    }

    // Append the config filename
    if (!PathAppend((wchar_t *) conf_path, APPDATA_CONFIG_NAME)) {
        // Failure
        int err = GetLastError();
        wchar_t *err_msg;
        get_error_msg(err, &err_msg);
        log_error(L"Failed to append to config path: %s (0x%08X)", err_msg, err);
        LocalFree(err_msg);
        return DISP_CONFIG_ERROR_GENERAL;
    }
    // Copy the path to a dynamically allocated buffer
    // The caller is responsible for freeing the memory
    wchar_t *dyn_config_path = _wcsdup((wchar_t *) conf_path);
    *config_path_out = dyn_config_path;

    return DISP_CONFIG_SUCCESS;
}

int disp_config_read_file(const wchar_t *wpath, app_config_t *app_config) {
    json_t *conf_root;
    json_error_t json_err;

    const char *path = wcstombs_alloc(wpath, NULL);

    conf_root = json_load_file(path, 0, &json_err);
    free((char *) path);

    if (!conf_root) {
        set_error_info(app_config, &json_err);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    // TODO: Use JSON_STRICT when unpacking?

    json_t *app_obj, *disp_presets;
    // Validate and unpack {"app": ..., "presets": ...}
    if (json_unpack_ex(conf_root, &json_err, 0, "{s: o, s: o}", "app", &app_obj, "presets", &disp_presets) != 0) {
        set_error_info(app_config, &json_err);
        json_decref(conf_root);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    // Read app.notify_on_start
    if (json_unpack_ex(app_obj, &json_err, 0, "{s: b}", "notify_on_start", &(app_config->notify_on_start)) != 0) {
        set_error_info(app_config, &json_err);
        json_decref(conf_root);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    // Get preset configs
    size_t disp_presets_size = json_array_size(disp_presets);

    app_config->preset_count = disp_presets_size;
    app_config->presets = calloc(disp_presets_size, sizeof(display_preset_t *));

    for (size_t i = 0; i < disp_presets_size; i++) {
        display_preset_t *preset_entry = calloc(1, sizeof(display_preset_t));
        preset_entry->applicable = 0;

        // Parse the preset entry
        json_t *elem = json_array_get(disp_presets, i);
        if (!json_is_object(elem)) {
            log_error(L"Invalid preset type, expected object");
            json_decref(conf_root);
            disp_config_destroy(app_config);
            return DISP_CONFIG_ERROR_GENERAL;
        }

        json_t *disp_settings;
        char *temp_name;

        // Validate and unpack the preset entry structure {"name": "<str>", "displays": [...]}
        if (json_unpack_ex(elem, &json_err, 0, "{s: s, s: o}", "name", &temp_name, "displays", &disp_settings) != 0) {
            set_error_info(app_config, &json_err);
            json_decref(conf_root);
            disp_config_destroy(app_config);
            return DISP_CONFIG_ERROR_GENERAL;
        }

        preset_entry->name = mbstowcsdup(temp_name, NULL);

        // Displays
        if (!json_is_array(disp_settings)) {
            log_error(L"Invalid display config type, expected array");
            json_decref(conf_root);
            disp_config_destroy(app_config);
            return DISP_CONFIG_ERROR_GENERAL;
        }

        size_t disp_settings_size = json_array_size(disp_settings);
        preset_entry->display_count = disp_settings_size;
        preset_entry->display_conf = calloc(disp_settings_size, sizeof(display_settings_t *));

        for (size_t a = 0; a < disp_settings_size; a++) {
            display_settings_t *display_entry = calloc(1, sizeof(display_settings_t));

            json_t *disp_elem = json_array_get(disp_settings, a);
            if (!json_is_object(disp_elem)) {
                log_error(L"Invalid display type, expected object");
                json_decref(conf_root);
                disp_config_destroy(app_config);
                return DISP_CONFIG_ERROR_GENERAL;
            }

            // Validate and unpack the display settings
            char *display_path;
            int res = json_unpack_ex(disp_elem, &json_err, 0, "{s: s, s: i, s: {s: i, s: i}, s: {s: i, s: i}}",
                                     "display", &display_path, "orientation", &(display_entry->orientation), "position",
                                     "x", &(display_entry->pos_x), "y", &(display_entry->pos_y), "resolution", "width",
                                     &(display_entry->width), "height", &(display_entry->height));

            if (res != 0) {
                set_error_info(app_config, &json_err);
                json_decref(conf_root);
                disp_config_destroy(app_config);
                return DISP_CONFIG_ERROR_GENERAL;
            }

            display_entry->device_path = mbstowcsdup(display_path, NULL);

            preset_entry->display_conf[a] = display_entry;
        }

        app_config->presets[i] = preset_entry;
    }

    json_decref(conf_root);

    return DISP_CONFIG_SUCCESS;
}

int disp_config_save_file(const wchar_t *wpath, app_config_t *app_config) {
    json_error_t json_err;

    // App settings
    json_t *app_conf = json_pack_ex(&json_err, 0, "{s: b}", "notify_on_start", app_config->notify_on_start);
    if (!app_conf) {
        log_error(L"Failed to pack app settings");
        set_error_info(app_config, &json_err);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    // Presets
    json_t *preset_arr = json_array();

    for (size_t i = 0; i < app_config->preset_count; i++) {
        display_preset_t *preset = app_config->presets[i];

        // Display JSON array
        json_t *display_arr = json_array();

        // Displays
        for (size_t a = 0; a < preset->display_count; a++) {
            display_settings_t *disp_settings = preset->display_conf[a];

            // Display path
            const char *display_path = wcstombs_alloc(disp_settings->device_path, NULL);

            // Create the JSON object
            json_t *display_obj = json_pack_ex(
                &json_err, 0, "{s: s, s: i, s: {s: i, s: i}, s: {s: i, s: i}}", "display", display_path, "orientation",
                disp_settings->orientation, "position", "x", disp_settings->pos_x, "y", disp_settings->pos_y,
                "resolution", "width", disp_settings->width, "height", disp_settings->height);

            free((char *) display_path);

            if (!display_obj) {
                log_error(L"Failed to pack display settings");
                set_error_info(app_config, &json_err);
                json_decref(display_arr);
                json_decref(preset_arr);
                json_decref(app_conf);
                return DISP_CONFIG_ERROR_GENERAL;
            }

            // Add the display to the display array
            if (json_array_append_new(display_arr, display_obj) != 0) {
                log_error(L"Failed to append to display array");
                json_decref(display_arr);
                json_decref(preset_arr);
                json_decref(app_conf);
                return DISP_CONFIG_ERROR_GENERAL;
            }
        }

        // Name
        const char *name_str = wcstombs_alloc(preset->name, NULL);

        // Create the preset entry and add it to the array
        json_t *preset_entry = json_pack_ex(&json_err, 0, "{s: s, s: o}", "name", name_str, "displays", display_arr);
        free((char *) name_str);

        if (!preset_entry) {
            log_error(L"Failed to pack preset entry");
            set_error_info(app_config, &json_err);
            json_decref(display_arr);
            json_decref(preset_arr);
            json_decref(app_conf);
            return DISP_CONFIG_ERROR_GENERAL;
        }

        if (json_array_append(preset_arr, preset_entry) != 0) {
            log_error(L"Failed to append to preset array");
            json_decref(preset_entry);
            json_decref(preset_arr);
            json_decref(app_conf);
            return DISP_CONFIG_ERROR_GENERAL;
        }
    }

    // Combine app settings and preset array to the root config object
    json_t *conf_root = json_pack_ex(&json_err, 0, "{s: o, s: o}", "app", app_conf, "presets", preset_arr);
    if (!conf_root) {
        log_error(L"Failed to pack settings root");
        set_error_info(app_config, &json_err);
        json_decref(preset_arr);
        json_decref(app_conf);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    // Config file path
    const char *path = wcstombs_alloc(wpath, NULL);

    if (json_dump_file(conf_root, path, JSON_INDENT(4)) != 0) {
        log_error(L"Failed to write settings to file");
        json_decref(conf_root);
        return DISP_CONFIG_ERROR_IO;
    }

    json_decref(conf_root);

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

static int disp_config_get_preset_idx_by_name(const wchar_t *name, app_ctx_t *ctx) {
    for (size_t i = 0; i < ctx->config.preset_count; i++) {
        display_preset_t *preset = ctx->config.presets[i];
        int cmp_res =
            CompareStringEx(LOCALE_NAME_INVARIANT, NORM_IGNORECASE, name, -1, preset->name, -1, NULL, NULL, 0);
        if (cmp_res == 0) {
            // Comparison failed
            int err = GetLastError();
            wchar_t *err_msg;
            get_error_msg(err, &err_msg);
            log_error(L"Failed to compare preset names: %s (0x%08X)", err_msg, err);
            LocalFree(err_msg);
            return DISP_CONFIG_ERROR_GENERAL;
        }
        if (cmp_res == CSTR_EQUAL) {
            // Match
            return i;
        }
    }
    return DISP_CONFIG_ERROR_NO_MATCH;
}

int disp_config_exists(const wchar_t *name, app_ctx_t *ctx) {
    // Check for existing matching preset (ignore preset name case)
    int ret = disp_config_get_preset_idx_by_name(name, ctx);
    if (ret < 0) {
        return ret;
    }
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

    // Get the preset index of the possibly existing preset
    int ext_preset_idx = disp_config_get_preset_idx_by_name(name, ctx);
    if (ext_preset_idx >= 0) {
        log_debug(L"Replacing existing preset");
        // Free the existing preset
        disp_config_preset_destroy(config->presets[ext_preset_idx]);
        // Update the preset array pointer
        config->presets[ext_preset_idx] = preset;
    } else if (ext_preset_idx == DISP_CONFIG_ERROR_NO_MATCH) {
        // No existing preset
        log_debug(L"Adding new preset");
        // Reallocate
        void *realloc_ptr = realloc(config->presets, (config->preset_count + 1) * sizeof(display_preset_t *));
        if (realloc_ptr == NULL) {
            // Realloc failed
            log_error(L"realloc failed");
            abort();
        }
        config->presets = realloc_ptr;

        config->presets[config->preset_count] = preset;
        config->preset_count = config->preset_count + 1;
    } else {
        // Error
        disp_config_preset_destroy(preset);
        return DISP_CONFIG_ERROR_GENERAL;
    }

    return DISP_CONFIG_SUCCESS;
}
