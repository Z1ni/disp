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
#include <Objbase.h>    // This can be removed if the notification GUID is hardcoded

#include "disp.h"
#include "config.h"
#include "resource.h"

LPTSTR orientation_str[4] = {L"Landscape", L"Portrait", L"Landscape (flipped)", L"Portrait (flipped)"};

// TODO: Implement proper logging

static void free_monitors(app_ctx_t *ctx) {
    free(ctx->monitors);
    ctx->monitors = NULL;
}

static BOOL CALLBACK monitor_enum_proc(HMONITOR mon, HDC hdc_mon, LPRECT lprc_mon, LPARAM dw_data) {
    app_ctx_t *ctx = (app_ctx_t *) dw_data;

    MONITORINFOEX info = {
        .cbSize = sizeof(MONITORINFOEX)
    };
    GetMonitorInfo(mon, (LPMONITORINFO)&info);

    StringCchCopy(ctx->monitors[ctx->monitor_count].name, CCHDEVICENAME, info.szDevice);
    ctx->monitors[ctx->monitor_count].rect = info.rcMonitor;

    ctx->monitor_count++;

    return TRUE;
}

static void populate_display_data(app_ctx_t *ctx) {
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
            monitor_idx = (int)i;
        }
        if (monitor_idx == -1) {
            // No corresponding monitor, skip
            wprintf(L"No monitor with name of %s\n", dd.DeviceName);
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
        wprintf(L"GetDisplayConfigBufferSizes failed: 0x%04X\n", ret);
        return;
    }

    // Allocate memory
    DISPLAYCONFIG_PATH_INFO *display_paths = (DISPLAYCONFIG_PATH_INFO *) calloc((int)num_of_paths, sizeof(DISPLAYCONFIG_PATH_INFO));
    DISPLAYCONFIG_MODE_INFO *display_modes = (DISPLAYCONFIG_MODE_INFO *) calloc((int)num_of_modes, sizeof(DISPLAYCONFIG_MODE_INFO));

    // Query information
    ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &num_of_paths, display_paths, &num_of_modes, display_modes, NULL);
    if (ret != ERROR_SUCCESS) {
        wprintf(L"QueryDisplayConfig failed: 0x%04X\n", ret);
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
        ret = DisplayConfigGetDeviceInfo((DISPLAYCONFIG_DEVICE_INFO_HEADER *)&device_name);
        if (ret != ERROR_SUCCESS) {
            wprintf(L"DisplayConfigGetDeviceInfo failed: 0x%04X\n", ret);
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
            // Copy friendly name to the monitor entry
            StringCchCopy(ctx->monitors[i].friendly_name, 64, device_name.monitorFriendlyDeviceName);
            found_monitor = TRUE;
            break;
        }
        if (!found_monitor) {
            wprintf(L"No corresponding monitor entry for %s\n", device_name.monitorDevicePath);
        }
    }

    free(display_paths);
    free(display_modes);
}

static void create_tray_menu(app_ctx_t *ctx) {
    // Create tray notification menu

    // Destroy existing menu if needed
    if (ctx->notif_menu != NULL) {
        DestroyMenu(ctx->notif_menu);
    }

    ctx->notif_menu = CreatePopupMenu();

    HMENU notif_menu_config = CreatePopupMenu();
    AppendMenu(notif_menu_config, 0, NOTIF_MENU_CONFIG_SAVE, L"Save current configuration…");
    AppendMenu(notif_menu_config, MF_SEPARATOR, 0, NULL);
    AppendMenu(notif_menu_config, MF_GRAYED, 0, L"Saved configurations");
    AppendMenu(notif_menu_config, MF_SEPARATOR, 0, NULL);

    // TODO: Limit listed configurations? Scrollable menu?

    display_preset_t **presets;
    int preset_count = disp_config_get_presets(&ctx->config, &presets);

    if (preset_count > 0) {
        for (int i = 0; i < preset_count; i++) {
            display_preset_t *preset = presets[i];
            if (preset->applicable == 0) {
                continue;
            }
            AppendMenu(notif_menu_config, 0, NOTIF_MENU_CONFIG_SELECT | (i & NOTIF_MENU_CONFIG_INDEX), preset->name);
        }
    } else {
        AppendMenu(notif_menu_config, MF_GRAYED, 0, L"None");
    }

    AppendMenu(ctx->notif_menu, MF_GRAYED, 0, APP_NAME L" " APP_VER);
    AppendMenu(ctx->notif_menu, MF_SEPARATOR, 0, NULL);

    for (size_t i = 0; i < ctx->monitor_count; i++) {
        monitor_t mon = ctx->monitors[i];
        // Create monitor -> orientation menu
        HMENU mon_orient_menu_conf = CreatePopupMenu();
        for (size_t a = 0; a < 4; a++) {
            // The monitor index is ORred with the constant
            UINT itemId = NOTIF_MENU_MONITOR_ORIENTATION_SELECT | i | (a << 10);
            AppendMenu(mon_orient_menu_conf, (mon.devmode.dmDisplayOrientation == a ? MF_CHECKED : 0), itemId, orientation_str[a]);
        }
        // Create submenu for this monitor
        HMENU mon_sub_menu_conf = CreatePopupMenu();
        AppendMenu(mon_sub_menu_conf, MF_POPUP, (UINT_PTR)mon_orient_menu_conf, L"Orientation");
        // Create menu entry for this monitor
        wchar_t entStr[100];
        StringCbPrintf(entStr, 100, L"%s (%s)", mon.friendly_name, orientation_str[mon.devmode.dmDisplayOrientation]);
        AppendMenu(ctx->notif_menu, MF_POPUP, (UINT_PTR)mon_sub_menu_conf, entStr);
    }

    AppendMenu(ctx->notif_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(ctx->notif_menu, 0, NOTIF_MENU_ABOUT_DISPLAYS, L"About displays");
    AppendMenu(ctx->notif_menu, MF_POPUP, (UINT_PTR)notif_menu_config, L"Config");
    AppendMenu(ctx->notif_menu, MF_SEPARATOR, 0, NULL);

    AppendMenu(ctx->notif_menu, 0, NOTIF_MENU_EXIT, L"Exit");
}

static BOOL change_display_position(monitor_t *mon) {
    DEVMODE tmp = mon->devmode;
    tmp.dmPosition = mon->virt_pos;
    tmp.dmFields = DM_POSITION;

    LONG ret = ChangeDisplaySettingsEx(mon->name, &tmp, NULL, CDS_UPDATEREGISTRY | CDS_GLOBAL, NULL);
    return ret == DISP_CHANGE_SUCCESSFUL;
}

static void show_notification_message(app_ctx_t *ctx, STRSAFE_LPCWSTR format, ...) {
    // Build the notification
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.guidItem = ctx->notify_guid;
    nid.uFlags = NIF_GUID | NIF_SHOWTIP | NIF_INFO;
    nid.dwInfoFlags = NIIF_RESPECT_QUIET_TIME;
    StringCchCopy(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), APP_NAME);
    // Format and copy message to szInfo
    va_list args;
    va_start(args, format);
    StringCbVPrintf(nid.szInfo, ARRAYSIZE(nid.szInfo), format, args);
    va_end(args);
    // Show the notification
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}

static BOOL change_display_orientation(app_ctx_t *ctx, monitor_t *mon, BYTE orientation) {
    if (mon->devmode.dmDisplayOrientation == orientation) {
        // No change
        return TRUE;
    }

    // Copy monitor_t to temporary struct as a possible WM_DISPLAYCHANGE trigger
    // creates monitor data and by doing so invalidates the monitor pointer.
    monitor_t temp_mon = {0};
    memcpy(&temp_mon, mon, sizeof(monitor_t));

    DEVMODE tmp = temp_mon.devmode;
    tmp.dmDisplayOrientation = orientation;
    tmp.dmFields = DM_DISPLAYORIENTATION;
    // Check if we should swap dmPelsHeight and dmPelsWidth (if the change is 90 degrees)
    int diff = abs(orientation - temp_mon.devmode.dmDisplayOrientation) % 3;
    if (diff < 2) {
        // 90 degree change, swap dmPelsHeight and dmPelsWidth
        int tempPelsHeight = tmp.dmPelsHeight;
        tmp.dmPelsHeight = tmp.dmPelsWidth;
        tmp.dmPelsWidth = tempPelsHeight;
        tmp.dmFields |= DM_PELSWIDTH | DM_PELSHEIGHT;
    } else {
        // 180 degree change, don't swap
        wprintf(L"180 degree change, no need to swap dmPelsHeight and dmPelsWidth\n");
        fflush(stdout);
    }

    // NOTE: AFTER THIS CALL MON IS POINTING TO TRASH BECAUSE WM_DISPLAYCHANGE TRIGGERS
    LONG ret = ChangeDisplaySettingsEx(temp_mon.name, &tmp, NULL, CDS_UPDATEREGISTRY | CDS_GLOBAL, NULL);
    if (ret != DISP_CHANGE_SUCCESSFUL) {
        wprintf(L"Display change failed: 0x%04X\n", ret);
    } else {
        wprintf(L"Display change was successful\n");
        // Show a notification
        // TODO: Don't use friendly name as it isn't always available
        show_notification_message(ctx, L"Changed display %s orientation to %s", temp_mon.friendly_name, orientation_str[orientation]);
    }
    fflush(stdout);

    return ret == DISP_CHANGE_SUCCESSFUL;
}

static int read_config(app_ctx_t *ctx, BOOL reload) {
    if (reload == TRUE) {
        // Free previous config
        wprintf(L"Freeing previous config\n");
        fflush(stdout);
        disp_config_destroy(&(ctx->config));
    }
    wprintf(L"Reading config\n");
    fflush(stdout);
    if (disp_config_read_file("disp.cfg", &(ctx->config)) != DISP_CONFIG_SUCCESS) {
        wchar_t err_msg[1024] = {0};
        StringCbPrintf((wchar_t *) &err_msg, 1024, L"Could not read configuration file:\n%s\n", disp_config_get_err_msg(&(ctx->config)));
        MessageBox(NULL, (wchar_t *) err_msg, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }
    return 0;
}

static void flag_matching_presets(app_ctx_t *ctx) {
    display_preset_t **presets;
    int preset_count = disp_config_get_presets(&(ctx->config), &presets);

    wprintf(L"Got %d presets\n", preset_count);

    for (int i = 0; i < preset_count; i++) {
        display_preset_t *preset = presets[i];

        if (disp_config_preset_matches_current(preset, ctx) == DISP_CONFIG_SUCCESS) {
            wprintf(L"Preset \"%s\" matches with the current monitor setup\n", preset->name);
            preset->applicable = 1;
        } else {
            wprintf(L"Preset \"%s\" does not match with the current monitor setup\n", preset->name);
        }
    }
}

static void reload(app_ctx_t *ctx) {
    wprintf(L"Reloading\n");
    fflush(stdout);
    populate_display_data(ctx);
    read_config(ctx, TRUE);
    flag_matching_presets(ctx);
    create_tray_menu(ctx);
}

static LRESULT CALLBACK save_dialog_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {

    switch (umsg) {
        case WM_INITDIALOG: ;
            // Set the name pointer from the init message
            SetWindowLongPtr(hwnd, GWLP_USERDATA, lparam);
            break;

        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDOK: ;
                    // User selected OK
                    // Get data pointer from the window
                    preset_dialog_data_t *data = (preset_dialog_data_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
                    // Get name pointer from the data struct
                    wchar_t *res_name = (wchar_t *) &data->preset_name;
                    // Get name from the text field
                    if (GetDlgItemText(hwnd, IDC_PRESET_NAME, res_name, 64) == 0) {
                        // Failed
                        wchar_t msg[100] = {0};
                        StringCbPrintf((wchar_t *) msg, 100, L"Failed to get dialog name string: 0x%08X", GetLastError());
                        MessageBox(hwnd, msg, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                        EndDialog(hwnd, wparam);
                        return TRUE;
                    }

                    wprintf(L"User selected OK\n");
                    fflush(stdout);
                    EndDialog(hwnd, wparam);
                    return TRUE;
                case IDCANCEL: ;
                    // User selected cancel
                    wprintf(L"User selected Cancel\n");
                    fflush(stdout);
                    // Get the data pointer and set the dialog to be canceled
                    preset_dialog_data_t *dialog_data = (preset_dialog_data_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
                    dialog_data->cancel = TRUE;
                    EndDialog(hwnd, wparam);
                    return TRUE;
            }
            break;

        default:
            return FALSE;
    }

    return FALSE;
}

static void save_current_config(app_ctx_t *ctx) {
    // Create input dialog
    wprintf(L"Showing preset name dialog\n");
    fflush(stdout);
    preset_dialog_data_t data = {0};
    //wchar_t preset_name[64] = {0};
    DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_PRESET_SAVE), ctx->main_window_hwnd, save_dialog_proc, (LPARAM) &data);
    if (data.cancel == TRUE) {
        // User canceled
        wprintf(L"User canceled name input\n");
        fflush(stdout);
        return;
    }
    wprintf(L"Preset name dialog closed, selected name: \"%s\"\n", data.preset_name);
    fflush(stdout);
    // TODO: Add new preset to the app_config
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {

    // Get window pointer that points to the app context
    app_ctx_t *ctx = (app_ctx_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (umsg) {
        case WM_DESTROY: ;
            wprintf(L"Shutting down\n");
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.uFlags = NIF_GUID;
            nid.guidItem = ctx->notify_guid;
            BOOL ret = Shell_NotifyIcon(NIM_DELETE, &nid);
            if (!ret) {
                wprintf(L"Couldn't delete notifyicon: ");
                wprintf(L"%ld\n", GetLastError());
            }
            DestroyMenu(ctx->notif_menu);
            PostQuitMessage(0);
            break;

        case MSG_NOTIFYICON: ;
            switch (LOWORD(lparam)) {
                case WM_CONTEXTMENU: ;
                    // Tray icon was right clicked
                    int menu_x = GET_X_LPARAM(wparam);
                    int menu_y = GET_Y_LPARAM(wparam);

                    SetForegroundWindow(hwnd);
                    // Show popup menu
                    if (!TrackPopupMenuEx(ctx->notif_menu, 0, menu_x, menu_y, hwnd, NULL)) {
                        wchar_t err_str[100];
                        StringCbPrintf(err_str, 100, L"TrackPopupMenuEx failed: %ld", GetLastError());
                        MessageBoxW(hwnd, (LPCWSTR)err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    }
                    break;
            }
            break;  

        case WM_COMMAND: ;
            // User did something with controls
            if (HIWORD(wparam) != 0) {
                break;
            }
            // Menu item selection
            SHORT selection = LOWORD(wparam);
            wprintf(L"User selected: 0x%04X\n", selection);
            fflush(stdout);
            switch (selection) {
                case NOTIF_MENU_EXIT: ;
                    // Exit item selected
                    DestroyWindow(hwnd);
                    break;

                case NOTIF_MENU_ABOUT_DISPLAYS: ;
                    // About displays
                    wchar_t about_str[2048];
                    LPTSTR end;
                    size_t rem;
                    StringCbPrintfEx(about_str, 2048, &end, &rem, 0, L"Display information:\n\n");
                    StringCbPrintfEx(end, rem, &end, &rem, 0, L"Display count: %d\n", ctx->monitor_count);
                    StringCbPrintfEx(end, rem, &end, &rem, 0, L"Virtual resolution: %ldx%ld\n", ctx->display_virtual_size.width, ctx->display_virtual_size.height);
                    for (size_t i = 0; i < ctx->monitor_count; i++) {
                        monitor_t mon = ctx->monitors[i];
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"%s (%s):\n", mon.friendly_name, mon.name);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Device ID: %s\n", mon.device_id);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Resolution: %ldx%ld\n", mon.rect.right-mon.rect.left, mon.rect.bottom-mon.rect.top);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Orientation: %s\n", orientation_str[mon.devmode.dmDisplayOrientation]);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Virtual position: %ld, %ld\n", mon.virt_pos.x, mon.virt_pos.y);
                    }
                    MessageBox(hwnd, about_str, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    break;

                case NOTIF_MENU_CONFIG_SAVE: ;
                    // Save current config
                    save_current_config(ctx);
                    break;
            }

            if ((NOTIF_MENU_MONITOR_ORIENTATION_SELECT & selection) == NOTIF_MENU_MONITOR_ORIENTATION_SELECT) {
                // User made a monitor orientation selection
                int monitor_idx = selection & NOTIF_MENU_MONITOR_ORIENTATION_MONITOR;
                int orientation = (selection & NOTIF_MENU_MONITOR_ORIENTATION_POSITION) >> 10;
                wprintf(L"User wants to change monitor %d orientation to %d\n", monitor_idx, orientation);
                fflush(stdout);
                monitor_t mon = ctx->monitors[monitor_idx];
                change_display_orientation(ctx, &mon, orientation);
                break;
            }

            if ((NOTIF_MENU_CONFIG_SELECT & selection) == NOTIF_MENU_CONFIG_SELECT) {
                // Config selected
                int config_idx = selection & NOTIF_MENU_CONFIG_INDEX;
                wprintf(L"User wants to apply preset %d (\"%s\")\n", config_idx, ctx->config.presets[config_idx]->name);
                fflush(stdout);
                // TODO: Apply preset
            }

            break;

        case WM_DISPLAYCHANGE: ;
            // Display settings have changed
            wprintf(L"WM_DISPLAYCHANGE: Display settings have changed\n");
            fflush(stdout);
            populate_display_data(ctx);
            create_tray_menu(ctx);
            // Reload config to check for applicable presets
            reload(ctx);
            break;

        default:
            return DefWindowProc(hwnd, umsg, wparam, lparam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_previnst, LPSTR lp_cmd_line, int n_show_cmd) {

    // TODO: Extract initializing to a new function/functions

    wprintf(L"Initializing\n");
    fflush(stdout);

    app_ctx_t app_context = {0};

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = wnd_proc;
    wcex.hInstance = h_inst;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = L"WinClass";

    if (!RegisterClassEx(&wcex)) {
        int err = GetLastError();
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"RegisterClassEx failed: %ld", err);
        MessageBox(NULL, (LPCWSTR)err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }

    HWND hwnd = CreateWindowW(L"WinClass", APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 100, NULL, NULL, h_inst, NULL);
    if (!hwnd) {
        int err = GetLastError();
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"CreateWindowW failed: %ld", err);
        MessageBox(NULL, (LPCWSTR)err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }
    // Set app context as the window user data so the window procedure can access the context without global variables
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) &app_context);
    // SetWindowPos is used here because SetWindowLongPtr docs tell us the following:
    //   "Certain window data is cached, so changes you make using SetWindowLongPtr will
    //    not take effect until you call the SetWindowPos function."
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    //ShowWindow(hwnd, n_show_cmd);
    UpdateWindow(hwnd);

    app_context.main_window_hwnd = hwnd;

    CoCreateGuid(&app_context.notify_guid);

    // Create tray notification
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
    nid.guidItem = app_context.notify_guid;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = MSG_NOTIFYICON;
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"Display");
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    BOOL ret = Shell_NotifyIcon(NIM_ADD, &nid);
    if (!ret) {
        MessageBox(NULL, L"Shell_NotifyIcon failed", APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        DestroyWindow(hwnd);
        return 1;
    }
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    // Populate display data
    populate_display_data(&app_context);

    // TODO: Create config file if it does not exist

    if (read_config(&app_context, FALSE) != 0) {
        DestroyWindow(hwnd);
        return 1;
    }

    flag_matching_presets(&app_context);

    create_tray_menu(&app_context);

    // Show a notification
    if (app_context.config.notify_on_start) {
        show_notification_message(&app_context, L"Display settings manager is running");
    }

    wprintf(L"Ready\n");
    fflush(stdout);

    // Init OK, start main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    free_monitors(&app_context);
    disp_config_destroy(&app_context.config);

    return (int)msg.wParam;
}
