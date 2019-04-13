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

LPTSTR orientation_str[4] = {L"Landscape", L"Portrait", L"Landscape (flipped)", L"Portrait (flipped)"};

GUID notifyGuid;

HMENU notifMenu = NULL;

// TODO: Implement proper logging

static void free_monitors(app_ctx_t *ctx) {
    free(ctx->monitors);
    ctx->monitors = NULL;
}

BOOL CALLBACK MonitorEnumProc(HMONITOR mon, HDC hdc_mon, LPRECT lprc_mon, LPARAM dw_data) {
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

VOID PopulateDisplayData(app_ctx_t *ctx) {
    int virtWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virtHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    ctx->display_virtual_size.width = virtWidth;
    ctx->display_virtual_size.height = virtHeight;

    // Free monitor data if needed
    if (ctx->monitors != NULL) {
        free_monitors(ctx);
    }

    ctx->monitor_count = 0;
    // TODO: Reallocate when needed
    ctx->monitors = calloc(10, sizeof(monitor_t));

    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM) ctx);

    for (size_t i = 0; i < ctx->monitor_count; i++) {
        monitor_t mon = ctx->monitors[i];
        DEVMODE tmp = {0};
        tmp.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings(mon.name, ENUM_CURRENT_SETTINGS, &tmp);

        memcpy(&(ctx->monitors[i].devmode), &tmp, sizeof(DEVMODE));
        memcpy(&(ctx->monitors[i].virtPos), &tmp.dmPosition, sizeof(POINTL));
    }

    // Get GDI and SetupAPI display names so that we can associate correct friendly monitor names
    DISPLAY_DEVICE dd = {0};
    dd.cb = sizeof(DISPLAY_DEVICE);
    DWORD dev = 0;
    while (EnumDisplayDevices(NULL, dev, &dd, EDD_GET_DEVICE_INTERFACE_NAME)) {
        DISPLAY_DEVICE ddMon = {0};
        ddMon.cb = sizeof(DISPLAY_DEVICE);
        DWORD devMon = 0;

        if ((dd.StateFlags & DISPLAY_DEVICE_ACTIVE) != DISPLAY_DEVICE_ACTIVE) {
            // Skip non-active devices
            ZeroMemory(&dd, sizeof(DISPLAY_DEVICE));
            dd.cb = sizeof(DISPLAY_DEVICE);
            dev++;
            continue;
        }

        // Find corresponding monitor_t entry
        int monitorIdx = -1;
        for (size_t i = 0; i < ctx->monitor_count; i++) {
            if (wcscmp(ctx->monitors[i].name, dd.DeviceName) != 0) {
                continue;
            }
            monitorIdx = (int)i;
        }
        if (monitorIdx == -1) {
            // No corresponding monitor, skip
            wprintf(L"No monitor with name of %s\n", dd.DeviceName);
            ZeroMemory(&dd, sizeof(DISPLAY_DEVICE));
            dd.cb = sizeof(DISPLAY_DEVICE);
            dev++;
            continue;
        }

        // Enumerate monitors
        while (EnumDisplayDevices(dd.DeviceName, devMon, &ddMon, EDD_GET_DEVICE_INTERFACE_NAME)) {
            // Copy the device ID to the monitor_t entry
            StringCchCopy(ctx->monitors[monitorIdx].deviceId, 128, ddMon.DeviceID);

            devMon++;
            ZeroMemory(&ddMon, sizeof(DISPLAY_DEVICE));
            ddMon.cb = sizeof(DISPLAY_DEVICE);
        }

        ZeroMemory(&dd, sizeof(DISPLAY_DEVICE));
        dd.cb = sizeof(DISPLAY_DEVICE);
        dev++;
    }

    // Get friendly display name
    UINT32 numOfPaths;
    UINT32 numOfModes;
    LONG ret = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numOfPaths, &numOfModes);
    if (ret != ERROR_SUCCESS) {
        wprintf(L"GetDisplayConfigBufferSizes failed: 0x%04X\n", ret);
        return;
    }

    // Allocate memory
    DISPLAYCONFIG_PATH_INFO *displayPaths = (DISPLAYCONFIG_PATH_INFO *) calloc((int)numOfPaths, sizeof(DISPLAYCONFIG_PATH_INFO));
    DISPLAYCONFIG_MODE_INFO *displayModes = (DISPLAYCONFIG_MODE_INFO *) calloc((int)numOfModes, sizeof(DISPLAYCONFIG_MODE_INFO));

    // Query information
    ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numOfPaths, displayPaths, &numOfModes, displayModes, NULL);
    if (ret != ERROR_SUCCESS) {
        wprintf(L"QueryDisplayConfig failed: 0x%04X\n", ret);
        free(displayPaths);
        free(displayModes);
        return;
    }

    for (size_t o = 0; o < numOfModes; o++) {
        DISPLAYCONFIG_MODE_INFO_TYPE infoType = displayModes[o].infoType;
        if (infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET) {
            continue;
        }

        DISPLAYCONFIG_TARGET_DEVICE_NAME deviceName;
        DISPLAYCONFIG_DEVICE_INFO_HEADER header;
        header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        header.size = sizeof(DISPLAYCONFIG_TARGET_DEVICE_NAME);
        header.id = displayModes[o].id;
        header.adapterId = displayModes[o].adapterId;
        deviceName.header = header;
        ret = DisplayConfigGetDeviceInfo((DISPLAYCONFIG_DEVICE_INFO_HEADER *)&deviceName);
        if (ret != ERROR_SUCCESS) {
            wprintf(L"DisplayConfigGetDeviceInfo failed: 0x%04X\n", ret);
            free(displayPaths);
            free(displayModes);
            return;
        }
        // Find corresponding monitor entry and set the friendly name
        BOOL foundMonitor = FALSE;
        for (size_t i = 0; i < ctx->monitor_count; i++) {
            if (wcscmp(ctx->monitors[i].deviceId, deviceName.monitorDevicePath) != 0) {
                continue;
            }
            // Copy friendly name to the monitor entry
            StringCchCopy(ctx->monitors[i].friendlyName, 64, deviceName.monitorFriendlyDeviceName);
            foundMonitor = TRUE;
            break;
        }
        if (!foundMonitor) {
            wprintf(L"No corresponding monitor entry for %s\n", deviceName.monitorDevicePath);
        }
    }

    free(displayPaths);
    free(displayModes);
}

VOID CreateTrayMenu(app_ctx_t *ctx) {
    // Create tray notification menu

    // Destroy existing menu if needed
    if (notifMenu != NULL) {
        DestroyMenu(notifMenu);
    }

    notifMenu = CreatePopupMenu();

    HMENU notifMenuConfig = CreatePopupMenu();
    AppendMenu(notifMenuConfig, 0, NOTIF_MENU_CONFIG_SAVE, L"Save current configuration…");
    AppendMenu(notifMenuConfig, MF_SEPARATOR, 0, NULL);
    AppendMenu(notifMenuConfig, MF_GRAYED, 0, L"Saved configurations");
    AppendMenu(notifMenuConfig, MF_SEPARATOR, 0, NULL);

    // TODO: Limit listed configurations? Scrollable menu?

    display_preset_t **presets;
    int preset_count = disp_config_get_presets(&ctx->config, &presets);

    if (preset_count > 0) {
        for (int i = 0; i < preset_count; i++) {
            display_preset_t *preset = presets[i];
            if (preset->applicable == 0) {
                continue;
            }
            AppendMenu(notifMenuConfig, 0, NOTIF_MENU_CONFIG_SELECT | (i & NOTIF_MENU_CONFIG_INDEX), preset->name);
        }
    } else {
        AppendMenu(notifMenuConfig, MF_GRAYED, 0, L"None");
    }

    AppendMenu(notifMenu, MF_GRAYED, 0, APP_NAME L" " APP_VER);
    AppendMenu(notifMenu, MF_SEPARATOR, 0, NULL);

    for (size_t i = 0; i < ctx->monitor_count; i++) {
        monitor_t mon = ctx->monitors[i];
        // Create monitor -> orientation menu
        HMENU monOrientMenuConf = CreatePopupMenu();
        for (size_t a = 0; a < 4; a++) {
            // The monitor index is ORred with the constant
            UINT itemId = NOTIF_MENU_MONITOR_ORIENTATION_SELECT | i | (a << 10);
            AppendMenu(monOrientMenuConf, (mon.devmode.dmDisplayOrientation == a ? MF_CHECKED : 0), itemId, orientation_str[a]);
        }
        // Create submenu for this monitor
        HMENU monSubMenuConf = CreatePopupMenu();
        AppendMenu(monSubMenuConf, MF_POPUP, (UINT_PTR)monOrientMenuConf, L"Orientation");
        // Create menu entry for this monitor
        wchar_t entStr[100];
        StringCbPrintf(entStr, 100, L"%s (%s)", mon.friendlyName, orientation_str[mon.devmode.dmDisplayOrientation]);
        AppendMenu(notifMenu, MF_POPUP, (UINT_PTR)monSubMenuConf, entStr);
    }

    AppendMenu(notifMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(notifMenu, 0, NOTIF_MENU_ABOUT_DISPLAYS, L"About displays");
    AppendMenu(notifMenu, MF_POPUP, (UINT_PTR)notifMenuConfig, L"Config");
    AppendMenu(notifMenu, MF_SEPARATOR, 0, NULL);

    AppendMenu(notifMenu, 0, NOTIF_MENU_EXIT, L"Exit");
}

BOOL ChangeDisplayPosition(monitor_t *mon) {
    DEVMODE tmp = mon->devmode;
    tmp.dmPosition = mon->virtPos;
    tmp.dmFields = DM_POSITION;

    LONG ret = ChangeDisplaySettingsEx(mon->name, &tmp, NULL, CDS_UPDATEREGISTRY | CDS_GLOBAL, NULL);
    return ret == DISP_CHANGE_SUCCESSFUL;
}

VOID ShowNotificationMessage(STRSAFE_LPCWSTR format, ...) {
    // Build the notification
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.guidItem = notifyGuid;
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

BOOL ChangeDisplayOrientation(monitor_t *mon, BYTE orientation) {
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
    DWORD diff = abs(orientation - temp_mon.devmode.dmDisplayOrientation) % 3;
    if (diff < 2) {
        // 90 degree change, swap dmPelsHeight and dmPelsWidth
        DWORD tempPelsHeight = tmp.dmPelsHeight;
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
        ShowNotificationMessage(L"Changed display %s orientation to %s", temp_mon.friendlyName, orientation_str[orientation]);
    }
    fflush(stdout);

    return ret == DISP_CHANGE_SUCCESSFUL;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    // Get window pointer that points to the app context
    app_ctx_t *ctx = (app_ctx_t *) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_DESTROY: ;
            wprintf(L"Shutting down\n");
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.uFlags = NIF_GUID;
            nid.guidItem = notifyGuid;
            BOOL ret = Shell_NotifyIcon(NIM_DELETE, &nid);
            if (!ret) {
                wprintf(L"Couldn't delete notifyicon: ");
                wprintf(L"%ld\n", GetLastError());
            }
            DestroyMenu(notifMenu);
            PostQuitMessage(0);
            break;

        case MSG_NOTIFYICON: ;
            switch (LOWORD(lParam)) {
                case WM_CONTEXTMENU: ;
                    // Tray icon was right clicked
                    int menuX = GET_X_LPARAM(wParam);
                    int menuY = GET_Y_LPARAM(wParam);

                    SetForegroundWindow(hWnd);
                    // Show popup menu
                    if (!TrackPopupMenuEx(notifMenu, 0, menuX, menuY, hWnd, NULL)) {
                        wchar_t errStr[100];
                        StringCbPrintf(errStr, 100, L"TrackPopupMenuEx failed: %ld", GetLastError());
                        MessageBoxW(hWnd, (LPCWSTR)errStr, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    }
                    break;
            }
            break;  

        case WM_COMMAND: ;
            // User did something with controls
            if (HIWORD(wParam) != 0) {
                break;
            }
            // Menu item selection
            SHORT selection = LOWORD(wParam);
            wprintf(L"User selected: 0x%04X\n", selection);
            fflush(stdout);
            switch (selection) {
                case NOTIF_MENU_EXIT: ;
                    // Exit item selected
                    DestroyWindow(hWnd);
                    break;

                case NOTIF_MENU_ABOUT_DISPLAYS: ;
                    // About displays
                    TCHAR aboutStr[2048];
                    LPTSTR end;
                    size_t rem;
                    StringCbPrintfEx(aboutStr, 2048, &end, &rem, 0, L"Display information:\n\n");
                    StringCbPrintfEx(end, rem, &end, &rem, 0, L"Display count: %d\n", ctx->monitor_count);
                    StringCbPrintfEx(end, rem, &end, &rem, 0, L"Virtual resolution: %ldx%ld\n", ctx->display_virtual_size.width, ctx->display_virtual_size.height);
                    for (size_t i = 0; i < ctx->monitor_count; i++) {
                        monitor_t mon = ctx->monitors[i];
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"%s (%s):\n", mon.friendlyName, mon.name);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Device ID: %s\n", mon.deviceId);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Resolution: %ldx%ld\n", mon.rect.right-mon.rect.left, mon.rect.bottom-mon.rect.top);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Orientation: %s\n", orientation_str[mon.devmode.dmDisplayOrientation]);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Virtual position: %ld, %ld\n", mon.virtPos.x, mon.virtPos.y);
                    }
                    MessageBox(hWnd, aboutStr, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    break;

                case NOTIF_MENU_CONFIG_SAVE: ;
                    // Save current config
                    // TODO
                    break;
            }

            if ((NOTIF_MENU_MONITOR_ORIENTATION_SELECT & selection) == NOTIF_MENU_MONITOR_ORIENTATION_SELECT) {
                // User made a monitor orientation selection
                int monitorIdx = selection & NOTIF_MENU_MONITOR_ORIENTATION_MONITOR;
                int orientation = (selection & NOTIF_MENU_MONITOR_ORIENTATION_POSITION) >> 10;
                wprintf(L"User wants to change monitor %d orientation to %d\n", monitorIdx, orientation);
                fflush(stdout);
                monitor_t mon = ctx->monitors[monitorIdx];
                ChangeDisplayOrientation(&mon, orientation);
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
            PopulateDisplayData(ctx);
            CreateTrayMenu(ctx);
            // TODO: Reload config to check for applicable presets
            break;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd) {

    // TODO: Extract initializing to a new function/functions

    wprintf(L"Initializing\n");
    fflush(stdout);

    app_ctx_t app_context = {0};

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInst;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = L"WinClass";

    if (!RegisterClassEx(&wcex)) {
        DWORD err = GetLastError();
        wchar_t errStr[100];
        StringCbPrintf(errStr, 100, L"RegisterClassEx failed: %ld", err);
        MessageBox(NULL, (LPCWSTR)errStr, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }

    HWND hWnd = CreateWindowW(L"WinClass", APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 100, NULL, NULL, hInst, NULL);
    if (!hWnd) {
        DWORD err = GetLastError();
        wchar_t errStr[100];
        StringCbPrintf(errStr, 100, L"CreateWindowW failed: %ld", err);
        MessageBox(NULL, (LPCWSTR)errStr, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }
    // Set app context as the window user data so WndProc can access the context without global variables
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR) &app_context);
    // SetWindowPos is used here because SetWindowLongPtr docs tell us the following:
    //   "Certain window data is cached, so changes you make using SetWindowLongPtr will
    //    not take effect until you call the SetWindowPos function."
    SetWindowPos(hWnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    //ShowWindow(hWnd, nShowCmd);
    UpdateWindow(hWnd);

    CoCreateGuid(&notifyGuid);

    // Create tray notification
    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
    nid.guidItem = notifyGuid;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = MSG_NOTIFYICON;
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"Display");
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    BOOL ret = Shell_NotifyIcon(NIM_ADD, &nid);
    if (!ret) {
        MessageBox(NULL, L"Shell_NotifyIcon failed", APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        DestroyWindow(hWnd);
        return 1;
    }
    Shell_NotifyIcon(NIM_SETVERSION, &nid);

    // Populate display data
    PopulateDisplayData(&app_context);

    // TODO: Create config file if it does not exist

    if (disp_config_read_file("disp.cfg", &app_context.config) != DISP_CONFIG_SUCCESS) {
        wprintf(L"Error\n");
        return 1;
    }

    display_preset_t **presets;
    int preset_count = disp_config_get_presets(&app_context.config, &presets);

    wprintf(L"Got %d presets\n", preset_count);

    // TODO: Realloc, do not hardcode
    int *match_indicies = calloc(10, sizeof(int));
    int match_idx_count = 0;

    for (int i = 0; i < preset_count; i++) {
        display_preset_t *preset = presets[i];

        if (disp_config_preset_matches_current(preset, &app_context) == DISP_CONFIG_SUCCESS) {
            wprintf(L"Preset \"%s\" matches with the current monitor setup\n", preset->name);
            preset->applicable = 1;
            match_indicies[match_idx_count++] = i;
        } else {
            wprintf(L"Preset \"%s\" does not match with the current monitor setup\n", preset->name);
        }
    }

    CreateTrayMenu(&app_context);

    // Show a notification
    ShowNotificationMessage(L"Display settings manager is running");

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
