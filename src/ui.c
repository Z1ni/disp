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
#include <Windows.h>
#include <windowsx.h>
#include "app.h"
#include "ui.h"
#include "config.h"
#include "resource.h"
#include "disp.h"

LPTSTR orientation_str[4] = {L"Landscape", L"Portrait", L"Landscape (flipped)", L"Portrait (flipped)"};

void create_tray_menu(app_ctx_t *ctx) {
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
    // TODO: Detect applied preset (even if the settings change wasn't done by this application)

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
            AppendMenu(mon_orient_menu_conf, (mon.devmode.dmDisplayOrientation == a ? MF_CHECKED : 0), itemId,
                       orientation_str[a]);
        }
        // Create submenu for this monitor
        HMENU mon_sub_menu_conf = CreatePopupMenu();
        AppendMenu(mon_sub_menu_conf, MF_POPUP, (UINT_PTR) mon_orient_menu_conf, L"Orientation");
        // Create menu entry for this monitor
        wchar_t entStr[100];
        StringCbPrintf(entStr, 100, L"%s (%s)", mon.friendly_name, orientation_str[mon.devmode.dmDisplayOrientation]);
        AppendMenu(ctx->notif_menu, MF_POPUP, (UINT_PTR) mon_sub_menu_conf, entStr);
    }

    AppendMenu(ctx->notif_menu, MF_SEPARATOR, 0, NULL);
    AppendMenu(ctx->notif_menu, 0, NOTIF_MENU_ABOUT_DISPLAYS, L"About displays");
    AppendMenu(ctx->notif_menu, MF_POPUP, (UINT_PTR) notif_menu_config, L"Config");
    AppendMenu(ctx->notif_menu, 0, NOTIF_MENU_SHOW_ALIGN_PATTERN, L"Show alignment pattern");
    AppendMenu(ctx->notif_menu, MF_SEPARATOR, 0, NULL);

    AppendMenu(ctx->notif_menu, 0, NOTIF_MENU_EXIT, L"Exit");
}

void show_notification_message(app_ctx_t *ctx, STRSAFE_LPCWSTR format, ...) {
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

static LRESULT CALLBACK save_dialog_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {

    switch (umsg) {
        case WM_INITDIALOG:;
            // Set the name pointer from the init message
            SetWindowLongPtr(hwnd, GWLP_USERDATA, lparam);
            // Focus to the text field
            HWND text_ctrl = GetDlgItem(hwnd, IDC_PRESET_NAME);
            SendMessage(hwnd, WM_NEXTDLGCTL, (WPARAM) text_ctrl, TRUE);
            break;

        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDOK:;
                    // User selected OK
                    // Get data pointer from the window
                    preset_dialog_data_t *data = (preset_dialog_data_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
                    // Get name pointer from the data struct
                    wchar_t *res_name = (wchar_t *) &data->preset_name;
                    // Get name from the text field
                    if (GetDlgItemText(hwnd, IDC_PRESET_NAME, res_name, 64) == 0) {
                        // Failed
                        wchar_t msg[100] = {0};
                        StringCbPrintf((wchar_t *) msg, 100, L"Failed to get dialog name string: 0x%08X",
                                       GetLastError());
                        log_error(msg);
                        MessageBox(hwnd, msg, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                        EndDialog(hwnd, wparam);
                        return TRUE;
                    }

                    log_debug(L"User selected OK");
                    EndDialog(hwnd, wparam);
                    return TRUE;
                case IDCANCEL:;
                    // User selected cancel
                    log_debug(L"User selected Cancel");
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

void show_save_dialog(app_ctx_t *ctx, preset_dialog_data_t *data) {
    DialogBoxParam(NULL, MAKEINTRESOURCE(IDD_DIALOG_PRESET_SAVE), ctx->main_window_hwnd, save_dialog_proc,
                   (LPARAM) data);
}

static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {

    // Get window pointer that points to the app context
    app_ctx_t *ctx = (app_ctx_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (umsg) {
        case WM_DESTROY:;
            log_info(L"Shutting down");
            NOTIFYICONDATA nid = {0};
            nid.cbSize = sizeof(NOTIFYICONDATA);
            nid.uFlags = NIF_GUID;
            nid.guidItem = ctx->notify_guid;
            BOOL ret = Shell_NotifyIcon(NIM_DELETE, &nid);
            if (!ret) {
                log_error(L"Couldn't delete notifyicon, error %ld", GetLastError());
            }
            DestroyMenu(ctx->notif_menu);
            PostQuitMessage(0);
            break;

        case MSG_NOTIFYICON:;
            switch (LOWORD(lparam)) {
                case WM_CONTEXTMENU:;
                    // Tray icon was right clicked
                    int menu_x = GET_X_LPARAM(wparam);
                    int menu_y = GET_Y_LPARAM(wparam);

                    SetForegroundWindow(hwnd);
                    // Show popup menu
                    if (!TrackPopupMenuEx(ctx->notif_menu, 0, menu_x, menu_y, hwnd, NULL)) {
                        wchar_t err_str[100];
                        StringCbPrintf(err_str, 100, L"TrackPopupMenuEx failed: %ld", GetLastError());
                        log_error(err_str);
                        MessageBoxW(hwnd, (LPCWSTR) err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                    }
                    break;
            }
            break;

        case WM_COMMAND:;
            // User did something with controls
            if (HIWORD(wparam) != 0) {
                break;
            }
            // Menu item selection
            SHORT selection = LOWORD(wparam);
            log_trace(L"User selected: 0x%04X", selection);

            switch (selection) {
                case NOTIF_MENU_EXIT:;
                    // Exit item selected
                    DestroyWindow(hwnd);
                    break;

                case NOTIF_MENU_ABOUT_DISPLAYS:;
                    // About displays
                    wchar_t about_str[2048];
                    LPTSTR end;
                    size_t rem;
                    StringCbPrintfEx(about_str, 2048, &end, &rem, 0, L"Display information:\n\n");
                    StringCbPrintfEx(end, rem, &end, &rem, 0, L"Display count: %d\n", ctx->monitor_count);
                    StringCbPrintfEx(end, rem, &end, &rem, 0, L"Virtual resolution: %ldx%ld\n",
                                     ctx->display_virtual_size.width, ctx->display_virtual_size.height);
                    for (size_t i = 0; i < ctx->monitor_count; i++) {
                        monitor_t mon = ctx->monitors[i];
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"%s (%s):\n", mon.friendly_name, mon.name);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Device ID: %s\n", mon.device_id);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Resolution: %ldx%ld\n",
                                         mon.rect.right - mon.rect.left, mon.rect.bottom - mon.rect.top);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Orientation: %s\n",
                                         orientation_str[mon.devmode.dmDisplayOrientation]);
                        StringCbPrintfEx(end, rem, &end, &rem, 0, L"  Virtual position: %ld, %ld\n", mon.virt_pos.x,
                                         mon.virt_pos.y);
                    }
                    MessageBox(hwnd, about_str, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
                    break;

                case NOTIF_MENU_CONFIG_SAVE:;
                    // Save current config
                    save_current_config(ctx);
                    break;

                case NOTIF_MENU_SHOW_ALIGN_PATTERN:;
                    // Show alignment pattern window
                    log_info(L"Showing alignment pattern window");
                    show_virt_desktop_window(ctx);
                    break;
            }

            if ((NOTIF_MENU_MONITOR_ORIENTATION_SELECT & selection) == NOTIF_MENU_MONITOR_ORIENTATION_SELECT) {
                // User made a monitor orientation selection
                int monitor_idx = selection & NOTIF_MENU_MONITOR_ORIENTATION_MONITOR;
                int orientation = (selection & NOTIF_MENU_MONITOR_ORIENTATION_POSITION) >> 10;
                log_debug(L"User wants to change monitor %d orientation to %d", monitor_idx, orientation);
                monitor_t mon = ctx->monitors[monitor_idx];
                change_display_orientation(ctx, &mon, orientation);
                break;
            }

            if ((NOTIF_MENU_CONFIG_SELECT & selection) == NOTIF_MENU_CONFIG_SELECT) {
                // Config selected
                int config_idx = selection & NOTIF_MENU_CONFIG_INDEX;
                display_preset_t *preset = ctx->config.presets[config_idx];
                log_debug(L"User wants to apply preset %d (\"%s\")", config_idx, preset->name);
                // Apply preset
                apply_preset(ctx, preset);
            }

            break;

        case WM_DISPLAYCHANGE:;
            // Display settings have changed
            log_debug(L"WM_DISPLAYCHANGE: Display settings have changed");
            if (ctx->display_update_in_progress) {
                // Display update in progress
                log_warning(L"Display update in progress, not reloading");
                break;
            }
            log_debug(L"Reloading information and config");
            ctx->display_update_in_progress = TRUE;
            populate_display_data(ctx);
            create_tray_menu(ctx);
            // Reload config to check for applicable presets
            reload(ctx);
            ctx->display_update_in_progress = FALSE;
            break;

        default:
            return DefWindowProc(hwnd, umsg, wparam, lparam);
    }
    return 0;
}

HWND init_main_window(app_ctx_t *ctx) {
    HINSTANCE h_inst = ctx->hinstance;

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = main_wnd_proc;
    wcex.hInstance = h_inst;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = L"WinClass";

    if (!RegisterClassEx(&wcex)) {
        int err = GetLastError();
        log_error(L"RegisterClassEx failed: %ld", err);
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"RegisterClassEx failed: %ld", err);
        MessageBox(NULL, (LPCWSTR) err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return NULL;
    }

    HWND hwnd = CreateWindow(L"WinClass", APP_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 100, NULL,
                             NULL, h_inst, NULL);
    if (!hwnd) {
        int err = GetLastError();
        log_error(L"CreateWindow failed: %ld", err);
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"CreateWindowW failed: %ld", err);
        MessageBox(NULL, (LPCWSTR) err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return NULL;
    }
    // Set app context as the window user data so the window procedure can access the context without global variables
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ctx);
    // SetWindowPos is used here because SetWindowLongPtr docs tell us the following:
    //   "Certain window data is cached, so changes you make using SetWindowLongPtr will
    //    not take effect until you call the SetWindowPos function."
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    // ShowWindow(hwnd, n_show_cmd);
    UpdateWindow(hwnd);

    ctx->main_window_hwnd = hwnd;
    return hwnd;
}

static LRESULT CALLBACK virt_desktop_wnd_proc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam) {
    // Get window pointer that points to the app context
    app_ctx_t *ctx = (app_ctx_t *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    PAINTSTRUCT ps;
    HDC hdc;

    RECT r;
    HBRUSH black = GetStockBrush(BLACK_BRUSH);
    HBRUSH white = GetStockBrush(WHITE_BRUSH);
    HBRUSH *brushes[2] = {&black, &white};
    int cur_brush = 0;
    int row_first_brush = 0;

    switch (umsg) {
        case WM_PAINT:;
            hdc = BeginPaint(hwnd, &ps);

            // Draw checkerboard pattern
            cur_brush = 0;
            for (int y = 0; y < ctx->display_virtual_size.height; y += 100) {
                for (int x = 0; x < ctx->display_virtual_size.width; x += 100) {
                    if (x == 0) {
                        row_first_brush = cur_brush;
                    }
                    SetRect(&r, x, y, x + 100, y + 100);
                    FillRect(hdc, &r, *(brushes[cur_brush]));
                    cur_brush = (cur_brush + 1) % 2;
                }
                if (row_first_brush == cur_brush) {
                    cur_brush = (cur_brush + 1) % 2;
                }
            }

            EndPaint(hwnd, &ps);
            break;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_CLOSE:
            // Close this window
            DestroyWindow(hwnd);
            break;

        default:
            return DefWindowProc(hwnd, umsg, wparam, lparam);
    }
    return 0;
}

int init_virt_desktop_window(app_ctx_t *ctx) {
    HINSTANCE h_inst = ctx->hinstance;

    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_NOCLOSE;
    wcex.lpfnWndProc = virt_desktop_wnd_proc;
    wcex.hInstance = h_inst;
    wcex.hIcon = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(0x80, 0, 0xFF));
    wcex.lpszClassName = L"VirtWinClass";

    if (!RegisterClassEx(&wcex)) {
        int err = GetLastError();
        log_error(L"RegisterClassEx failed for virtual desktop window: %ld", err);
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"RegisterClassEx failed: %ld", err);
        MessageBox(NULL, (LPCWSTR) err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return 1;
    }

    return 0;
}

HWND show_virt_desktop_window(app_ctx_t *ctx) {
    HINSTANCE h_inst = ctx->hinstance;

    long leftmost_x = ctx->min_monitor_pos.x;
    long leftmost_y = ctx->min_monitor_pos.y;
    virt_size_t *vs = &ctx->display_virtual_size;
    HWND hwnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"VirtWinClass", APP_NAME, WS_POPUP,
                               leftmost_x, leftmost_y, vs->width, vs->height, NULL, NULL, h_inst, NULL);

    if (!hwnd) {
        int err = GetLastError();
        log_error(L"CreateWindow failed for virtual desktop window: %ld", err);
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"CreateWindowW failed: %ld", err);
        MessageBox(NULL, (LPCWSTR) err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return NULL;
    }

    // Make the window transparent
    SetLayeredWindowAttributes(hwnd, RGB(0x80, 0, 0xFF), 0, LWA_COLORKEY);

    // Set app context as the window user data so the window procedure can access the context without global variables
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) ctx);
    SetWindowPos(hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hwnd, SW_SHOW);

    return hwnd;
}

int create_tray_icon(app_ctx_t *ctx, HWND hwnd) {
    // Create GUID for the icon
    CoCreateGuid(&ctx->notify_guid);

    NOTIFYICONDATA nid = {0};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
    nid.guidItem = ctx->notify_guid;
    nid.uVersion = NOTIFYICON_VERSION_4;
    nid.uCallbackMessage = MSG_NOTIFYICON;
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"Display");
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    BOOL ret = Shell_NotifyIcon(NIM_ADD, &nid);
    if (!ret) {
        int err = GetLastError();
        log_error(L"Shell_NotifyIcon failed: %ld", err);
        wchar_t err_str[100];
        StringCbPrintf(err_str, 100, L"Shell_NotifyIcon failed: %ld", err);
        MessageBox(NULL, (LPCWSTR) err_str, APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        DestroyWindow(hwnd);
        return 1;
    }
    Shell_NotifyIcon(NIM_SETVERSION, &nid);
    return 0;
}
