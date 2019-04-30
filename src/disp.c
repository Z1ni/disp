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

#define _UNICODE
#define UNICODE

// Set Windows version to 10
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <Windowsx.h>
#include <Commctrl.h>
#include <Strsafe.h>
#include <shellapi.h>

#include "disp.h"
#include "config.h"
#include "resource.h"
#include "log.h"
#include "ui.h"

void free_monitors(app_ctx_t *ctx) {
    free(ctx->monitors);
    ctx->monitors = NULL;
}

static BOOL CALLBACK monitor_enum_proc(HMONITOR mon, HDC hdc_mon, LPRECT lprc_mon, LPARAM dw_data) {
    app_ctx_t *ctx = (app_ctx_t *) dw_data;

    MONITORINFOEX info = {.cbSize = sizeof(MONITORINFOEX)};
    GetMonitorInfo(mon, (LPMONITORINFO) &info);

    StringCchCopy(ctx->monitors[ctx->monitor_count].name, CCHDEVICENAME, info.szDevice);
    ctx->monitors[ctx->monitor_count].rect = info.rcMonitor;

    ctx->monitor_count++;

    return TRUE;
}

static int monitor_coordinate_compare(const void *a, const void *b) {
    monitor_t *a_mon = (monitor_t *) a;
    monitor_t *b_mon = (monitor_t *) b;

    LONG a_x = a_mon->virt_pos.x;
    LONG a_y = a_mon->virt_pos.y;
    LONG b_x = b_mon->virt_pos.x;
    LONG b_y = b_mon->virt_pos.y;

    if (a_y == b_y && a_x == b_x) {
        // The two monitors are in the same place
        // Shouldn't be possible
        return 0;
    }

    // If the A is more left than B
    // If A.X and B.X are the same, compare Y so that the topmost comes first
    if (a_x < b_x || (a_x == b_x && a_y < b_y)) {
        // A before B
        return -1;
    }
    // B before A
    return 1;
}

void populate_display_data(app_ctx_t *ctx) {
    int virt_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virt_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    ctx->display_virtual_size.width = virt_width;
    ctx->display_virtual_size.height = virt_height;

    // Free monitor data if needed
    if (ctx->monitors != NULL) {
        free_monitors(ctx);
    }

    ctx->monitor_count = 0;
    // TODO: Reallocate when needed
    ctx->monitors = calloc(10, sizeof(monitor_t));

    EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, (LPARAM) ctx);

    for (size_t i = 0; i < ctx->monitor_count; i++) {
        monitor_t mon = ctx->monitors[i];
        DEVMODE tmp = {0};
        tmp.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings(mon.name, ENUM_CURRENT_SETTINGS, &tmp);

        memcpy(&(ctx->monitors[i].devmode), &tmp, sizeof(DEVMODE));
        memcpy(&(ctx->monitors[i].virt_pos), &tmp.dmPosition, sizeof(POINTL));
    }

    // Sort monitors by their coordinates so we can number them (the leftmost is 1, etc.)
    qsort(ctx->monitors, ctx->monitor_count, sizeof(monitor_t), monitor_coordinate_compare);
    // Number the monitors
    for (size_t i = 0; i < ctx->monitor_count; i++) {
        monitor_t *mon = &(ctx->monitors[i]);
        mon->num = i + 1;
    }

    // Get GDI and SetupAPI display names so that we can associate correct friendly monitor names
    DISPLAY_DEVICE dd = {0};
    dd.cb = sizeof(DISPLAY_DEVICE);
    int dev = 0;
    while (EnumDisplayDevices(NULL, dev, &dd, EDD_GET_DEVICE_INTERFACE_NAME)) {
        DISPLAY_DEVICE dd_mon = {0};
        dd_mon.cb = sizeof(DISPLAY_DEVICE);
        int dev_mon = 0;

        if ((dd.StateFlags & DISPLAY_DEVICE_ACTIVE) != DISPLAY_DEVICE_ACTIVE) {
            // Skip non-active devices
            ZeroMemory(&dd, sizeof(DISPLAY_DEVICE));
            dd.cb = sizeof(DISPLAY_DEVICE);
            dev++;
            continue;
        }

        // Find corresponding monitor_t entry
        int monitor_idx = -1;
        for (size_t i = 0; i < ctx->monitor_count; i++) {
            if (wcscmp(ctx->monitors[i].name, dd.DeviceName) != 0) {
                continue;
            }
            monitor_idx = (int) i;
        }
        if (monitor_idx == -1) {
            // No corresponding monitor, skip
            log_debug(L"No monitor with name of %s", dd.DeviceName);
            ZeroMemory(&dd, sizeof(DISPLAY_DEVICE));
            dd.cb = sizeof(DISPLAY_DEVICE);
            dev++;
            continue;
        }

        // Enumerate monitors
        while (EnumDisplayDevices(dd.DeviceName, dev_mon, &dd_mon, EDD_GET_DEVICE_INTERFACE_NAME)) {
            // Copy the device ID to the monitor_t entry
            StringCchCopy(ctx->monitors[monitor_idx].device_id, 128, dd_mon.DeviceID);

            dev_mon++;
            ZeroMemory(&dd_mon, sizeof(DISPLAY_DEVICE));
            dd_mon.cb = sizeof(DISPLAY_DEVICE);
        }

        ZeroMemory(&dd, sizeof(DISPLAY_DEVICE));
        dd.cb = sizeof(DISPLAY_DEVICE);
        dev++;
    }

    // Get friendly display name
    UINT32 num_of_paths;
    UINT32 num_of_modes;
    LONG ret = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_of_paths, &num_of_modes);
    if (ret != ERROR_SUCCESS) {
        log_error(L"GetDisplayConfigBufferSizes failed: 0x%04X", ret);
        return;
    }

    // Allocate memory
    DISPLAYCONFIG_PATH_INFO *display_paths =
        (DISPLAYCONFIG_PATH_INFO *) calloc((int) num_of_paths, sizeof(DISPLAYCONFIG_PATH_INFO));
    DISPLAYCONFIG_MODE_INFO *display_modes =
        (DISPLAYCONFIG_MODE_INFO *) calloc((int) num_of_modes, sizeof(DISPLAYCONFIG_MODE_INFO));

    // Query information
    ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &num_of_paths, display_paths, &num_of_modes, display_modes, NULL);
    if (ret != ERROR_SUCCESS) {
        log_error(L"QueryDisplayConfig failed: 0x%04X", ret);
        free(display_paths);
        free(display_modes);
        return;
    }

    for (size_t o = 0; o < num_of_modes; o++) {
        DISPLAYCONFIG_MODE_INFO_TYPE infoType = display_modes[o].infoType;
        if (infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET) {
            continue;
        }

        DISPLAYCONFIG_TARGET_DEVICE_NAME device_name;
        DISPLAYCONFIG_DEVICE_INFO_HEADER header;
        header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        header.size = sizeof(DISPLAYCONFIG_TARGET_DEVICE_NAME);
        header.id = display_modes[o].id;
        header.adapterId = display_modes[o].adapterId;
        device_name.header = header;
        ret = DisplayConfigGetDeviceInfo((DISPLAYCONFIG_DEVICE_INFO_HEADER *) &device_name);
        if (ret != ERROR_SUCCESS) {
            log_error(L"DisplayConfigGetDeviceInfo failed: 0x%04X", ret);
            free(display_paths);
            free(display_modes);
            return;
        }
        // Find corresponding monitor entry and set the friendly name
        BOOL found_monitor = FALSE;
        for (size_t i = 0; i < ctx->monitor_count; i++) {
            if (wcscmp(ctx->monitors[i].device_id, device_name.monitorDevicePath) != 0) {
                continue;
            }
            if (device_name.monitorFriendlyDeviceName == NULL || wcslen(device_name.monitorFriendlyDeviceName) == 0) {
                // No friendly device name from OS, use numbering
                StringCbPrintf(ctx->monitors[i].friendly_name, 64, L"Display %u", ctx->monitors[i].num);
            } else {
                // Friendly name available, copy it to the monitor entry
                StringCchCopy(ctx->monitors[i].friendly_name, 64, device_name.monitorFriendlyDeviceName);
            }
            found_monitor = TRUE;
            break;
        }
        if (!found_monitor) {
            log_debug(L"No corresponding monitor entry for %s", device_name.monitorDevicePath);
        }
    }

    free(display_paths);
    free(display_modes);
}

int read_config(app_ctx_t *ctx, BOOL reload) {
    if (reload == TRUE) {
        // Free previous config
        log_debug(L"Freeing previous config");
        disp_config_destroy(&(ctx->config));
    }
    log_info(L"Reading config");
    if (disp_config_read_file("disp.cfg", &(ctx->config)) != DISP_CONFIG_SUCCESS) {
        wchar_t err_msg[1024] = {0};
        StringCbPrintf((wchar_t *) &err_msg, 1024, L"Could not read configuration file:\n%s",
                       disp_config_get_err_msg(&(ctx->config)));
        log_error(err_msg);
        MessageBox(NULL, (wchar_t *) err_msg, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }
    return 0;
}

void flag_matching_presets(app_ctx_t *ctx) {
    display_preset_t **presets;
    int preset_count = disp_config_get_presets(&(ctx->config), &presets);

    log_trace(L"Got %d presets", preset_count);

    for (int i = 0; i < preset_count; i++) {
        display_preset_t *preset = presets[i];

        if (disp_config_preset_matches_current(preset, ctx) == DISP_CONFIG_SUCCESS) {
            log_trace(L"Preset \"%s\" matches with the current monitor setup", preset->name);
            preset->applicable = 1;
        } else {
            log_trace(L"Preset \"%s\" does not match with the current monitor setup", preset->name);
        }
    }
}

void reload(app_ctx_t *ctx) {
    log_debug(L"Reloading");
    populate_display_data(ctx);
    read_config(ctx, TRUE);
    flag_matching_presets(ctx);
    create_tray_menu(ctx);
}

static BOOL get_matching_monitor(app_ctx_t *ctx, const wchar_t *device_id, monitor_t **monitor_out) {
    for (size_t i = 0; i < ctx->monitor_count; i++) {
        monitor_t *monitor = &(ctx->monitors[i]);
        // Compare paths
        if (wcscmp((wchar_t *) monitor->device_id, device_id) == 0) {
            // Found
            *monitor_out = monitor;
            return TRUE;
        }
    }
    return FALSE;
}

// TODO: Use this when changing the display orientation from the tray menu
static void change_orientation_devmode(DEVMODE *devmode, int orientation) {
    if ((int) devmode->dmDisplayOrientation == orientation) {
        // No change
        return;
    }

    devmode->dmDisplayOrientation = orientation;
    devmode->dmFields |= DM_DISPLAYORIENTATION;
    // Check if we should swap dmPelsHeight and dmPelsWidth (if the change is 90 degrees)
    int diff = abs(orientation - devmode->dmDisplayOrientation) % 3;
    if (diff < 2) {
        // 90 degree change, swap dmPelsHeight and dmPelsWidth
        int tempPelsHeight = devmode->dmPelsHeight;
        devmode->dmPelsHeight = devmode->dmPelsWidth;
        devmode->dmPelsWidth = tempPelsHeight;
        devmode->dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT;
    } else {
        // 180 degree change, don't swap
        log_debug(L"180 degree change, no need to swap dmPelsHeight and dmPelsWidth");
    }
}

static void change_position_devmode(DEVMODE *devmode, int pos_x, int pos_y) {
    if (devmode->dmPosition.x == pos_x && devmode->dmPosition.y == pos_y) {
        // No change
        return;
    }

    devmode->dmPosition.x = pos_x;
    devmode->dmPosition.y = pos_y;
    devmode->dmFields |= DM_POSITION;
}

static BOOL change_display_settings(wchar_t *monitor_name, DEVMODE *devmode) {
    LONG ret = ChangeDisplaySettingsEx(monitor_name, devmode, NULL, CDS_UPDATEREGISTRY | CDS_GLOBAL, NULL);
    if (ret != DISP_CHANGE_SUCCESSFUL) {
        log_error(L"Display change failed: 0x%04X", ret);
        return FALSE;
    } else {
        log_debug(L"Display change was successful");
        return TRUE;
    }
}

void apply_preset(app_ctx_t *ctx, display_preset_t *preset) {
    // For now we support changing display positions and orientations

    if (ctx->display_update_in_progress) {
        // TODO: What to do? Can't sleep because it will block the whole application
        // Just give up for now
        log_warning(L"Display update already in progress, can't change settings");
        return;
    }
    ctx->display_update_in_progress = TRUE;
    log_info(L"Applying preset \"%s\"", preset->name);

    size_t success_count = 0;
    for (size_t i = 0; i < preset->display_count; i++) {
        display_settings_t *settings = preset->display_conf[i];

        // Find the matching current monitor
        // TODO: Check that all the monitors match before applying so that we don't end up in a inconsistent state
        monitor_t *monitor;
        if (get_matching_monitor(ctx, settings->device_path, &monitor) != TRUE) {
            // No matching monitor (this shouldn't happen as we check the monitors on WM_DISPLAYCHANGE)
            log_error(L"Failed to apply preset: no matching monitor");
            MessageBox(ctx->main_window_hwnd, L"Failed to apply preset: no matching monitor", APP_NAME,
                       MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            ctx->display_update_in_progress = FALSE;
            return;
        }
        // monitor now contains the matching monitor_t
        // Copy base DEVMODE from the monitor
        DEVMODE tmp = {0};
        memcpy(&tmp, &(monitor->devmode), sizeof(DEVMODE));
        // Make the needed devmode changes to change the orientation (if needed)
        change_orientation_devmode(&tmp, settings->orientation);
        // Make the needed position changes
        change_position_devmode(&tmp, settings->pos_x, settings->pos_y);
        // Apply the devmode
        if (change_display_settings(monitor->name, &tmp)) {
            // Success
            success_count++;
        }
    }

    if (success_count == preset->display_count) {
        log_info(L"Display preset changed to %s", preset->name);
        // Show a notification
        show_notification_message(ctx, L"Changed display preset to \"%s\"", preset->name);
        // TODO: Auto-revert period?
    } else {
        log_warning(L"Display preset change failed, %d fails", (preset->display_count - success_count));
        // One or more changes failed
        show_notification_message(ctx, L"Failed to change display preset to \"%s\"", preset->name);
        // TODO: Try to revert?
    }

    // Reload display info
    populate_display_data(ctx);
    create_tray_menu(ctx);
    // Reload config to check for applicable presets
    reload(ctx);

    // All done
    ctx->display_update_in_progress = FALSE;
}

BOOL change_display_orientation(app_ctx_t *ctx, monitor_t *mon, BYTE orientation) {
    if (mon->devmode.dmDisplayOrientation == orientation) {
        // No change
        return TRUE;
    }

    DEVMODE tmp = {0};
    memcpy(&tmp, &(mon->devmode), sizeof(DEVMODE));
    // Make the needed devmode changes to change the orientation (if needed)
    change_orientation_devmode(&tmp, orientation);

    // Apply the devmode
    if (change_display_settings(mon->name, &tmp)) {
        // Success
        log_debug(L"Display change was successful");
        // Show a notification
        // TODO: Don't use friendly name as it isn't always available
        LPTSTR orientation_str[4] = {L"Landscape", L"Portrait", L"Landscape (flipped)", L"Portrait (flipped)"};
        show_notification_message(ctx, L"Changed display %s orientation to %s", mon->friendly_name,
                                  orientation_str[orientation]);
        return TRUE;
    }

    return FALSE;
}

void save_current_config(app_ctx_t *ctx) {
    // Create input dialog
    log_debug(L"Showing preset name dialog");
    preset_dialog_data_t data = {0};
    show_save_dialog(ctx, &data);
    if (data.cancel == TRUE) {
        // User canceled
        log_debug(L"User canceled name input");
        return;
    }
    log_debug(L"Preset name dialog closed, selected name: \"%s\"", data.preset_name);
    // Add new preset to the app_config
    if (disp_config_create_preset(data.preset_name, ctx) != DISP_CONFIG_SUCCESS) {
        // Failed
        MessageBox(ctx->main_window_hwnd, L"Preset creation failed", APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return;
    }
    // Preset created, save
    if (disp_config_save_file("disp.cfg", &(ctx->config)) != DISP_CONFIG_SUCCESS) {
        // Failed
        wchar_t err_msg[600] = {0};
        StringCbPrintf((wchar_t *) err_msg, 600, L"Preset was created, but saving it failed:\n%s",
                       disp_config_get_err_msg(&(ctx->config)));
        MessageBox(ctx->main_window_hwnd, err_msg, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        // TODO: Handle better
        PostQuitMessage(1);
        return;
    }
    // Reload config etc.
    reload(ctx);
    // Save done, notify user
    show_notification_message(ctx, L"Preset \"%s\" was saved", data.preset_name);
}
