#include <stdlib.h>
#include <string.h>
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

int disp_config_read_file(const char *path, app_config_t *app_config) {
    config_t conf;

    config_init(&conf);

    if (config_read_file(&conf, path) == CONFIG_FALSE) {
        // Fail
        // TODO: Get error string from libconfig
        fprintf(stderr, "libconfig: %s on line %d\n", config_error_text(&conf), config_error_line(&conf));
        return DISP_CONFIG_ERROR_GENERAL;
    }

    if (config_lookup_bool(&conf, "app.notify_on_start", &(app_config->notify_on_start)) == CONFIG_FALSE) {
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
        //preset_entry->name = wcsdup(L"Dummy");
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
        if (disp_config_preset_get_display(preset, ctx->monitors[i].deviceId, NULL) == DISP_CONFIG_ERROR_NO_ENTRY) {
            // No match
            return DISP_CONFIG_ERROR_NO_MATCH;
        }
    }
    // Full match
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
            free((wchar_t *)preset->name);
            free(preset);
        }
        free(config->presets);
    }
}
