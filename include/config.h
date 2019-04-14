#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <libconfig.h>

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

int disp_config_read_file(const char *path, app_config_t *config);
int disp_config_get_presets(const app_config_t *config, display_preset_t ***presets);    // returns count of presets or error
wchar_t *disp_config_get_err_msg(const app_config_t *config);
int disp_config_preset_get_display(const display_preset_t *preset, const wchar_t *path, display_settings_t **settings);   // returns DISP_CONFIG_SUCCESS or error
int disp_config_preset_matches_current(const display_preset_t *preset, const app_ctx_t *ctx);

void disp_config_destroy(app_config_t *config);

#endif
