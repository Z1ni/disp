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
#include "app.h"
#include "ui.h"
#include "disp.h"

int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_previnst, LPSTR lp_cmd_line, int n_show_cmd) {

    log_set_level(LOG_WARNING);

    // Parse command line arguments
    int argc = 0;
    wchar_t **argv = NULL;
    argv = CommandLineToArgvW(GetCommandLine(), &argc);

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-v") == 0) {
            // Verbose
            log_set_level(LOG_TRACE);
        } else if (wcscmp(argv[i], L"-l") == 0) {
            // Create logfile
            log_set_file_level(LOG_TRACE);
        } else if (wcscmp(argv[i], L"-V") == 0) {
            // Version
            wprintf(APP_NAME L" " APP_VER L"\n");
            return 0;
        } else if (wcscmp(argv[i], L"-h") == 0) {
            // Help
            wprintf(L"Usage: %s [-hvlV]\n", argv[0]);
            return 0;
        }
    }

    LocalFree(argv);

    // Check if an instance is already running
    HANDLE instance_mutex = CreateMutex(NULL, FALSE, L"Zini.Disp");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        log_info(L"An instance is already running, exiting");
        return 0;
    }
    if (!instance_mutex) {
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            // Try to open
            instance_mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, L"Zini.Disp");
            if (!instance_mutex) {
                // Failed
                // TODO: MessageBox
                log_error(L"Failed to open mutex: 0x%04X", GetLastError());
                return 1;
            }
        }
        log_error(L"Could not create app mutex: 0x%04X", GetLastError());
        // TODO: MessageBox
        return 1;
    }

    // Init logging. All logging messages before this are not written to a file.
    log_init();

    log_info(L"Initializing");

    app_ctx_t app_context = {0};
    app_context.hinstance = h_inst;
    app_context.display_update_in_progress = FALSE;
    app_context.instance_mutex = instance_mutex;

    HWND hwnd = init_main_window(&app_context);
    init_virt_desktop_window(&app_context);

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
            ReleaseMutex(app_context.instance_mutex);
            log_finish();
            return 1;
        }
        log_info(L"Config file was created");
    }

    if (read_config(&app_context, FALSE) != 0) {
        DestroyWindow(hwnd);
        ReleaseMutex(app_context.instance_mutex);
        log_finish();
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
    ReleaseMutex(app_context.instance_mutex);

    log_info(L"Exiting");
    log_finish();
    return (int) msg.wParam;
}
