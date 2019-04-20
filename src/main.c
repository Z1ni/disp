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
#include <shlwapi.h>
#include "app.h"
#include "ui.h"
#include "disp.h"

int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_previnst, LPSTR lp_cmd_line, int n_show_cmd) {

    log_info(L"Initializing");

    app_ctx_t app_context = {0};
    app_context.display_update_in_progress = FALSE;

    HWND hwnd = init_main_window(&app_context, h_inst);

    // Create tray icon
    create_tray_icon(&app_context, hwnd);

    // Populate display data
    populate_display_data(&app_context);

    // Check if a config file exists
    if (!PathFileExists(L"disp.cfg")) {
        // No config exists, create a config file
        if (disp_config_save_file("disp.cfg", &app_context.config) != DISP_CONFIG_SUCCESS) {
            // Config file creation failed
            log_error(L"Could not create a config file: %s", disp_config_get_err_msg(&app_context.config));
            MessageBox(hwnd, L"Could not create a config file", APP_NAME, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
            DestroyWindow(hwnd);
            return 1;
        }
        log_info(L"Config file was created");
    }

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

    log_info(L"Ready");

    // Init OK, start main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    log_info(L"Cleaning up");
    free_monitors(&app_context);
    disp_config_destroy(&app_context.config);

    log_info(L"Exiting");
    return (int) msg.wParam;
}
