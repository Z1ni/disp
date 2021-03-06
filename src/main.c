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

static void print_help(wchar_t **argv) {
    wprintf(L"Usage: %s [OPTIONS]\n\n", argv[0]);

    wprintf(L"disp - Simple display settings manager for Windows 7+\n");
    wprintf(L"Copyright (C) 2019-2020 Mark \"zini\" Makinen\n\n");

    wprintf(L"Options:\n");
    wprintf(L"  -h, --help         Print (this) help\n");
    wprintf(L"  -c, --config path  Use the given config file\n");
    wprintf(L"  -p, --preset name  Apply preset with the given name. If there is an another\n");
    wprintf(L"                     disp process running, it will perform the change and the\n");
    wprintf(L"                     commanding process will exit immediately. Otherwise the\n");
    wprintf(L"                     started process will perform the change and keep running.\n");
    wprintf(L"  -v, --verbose      Verbose output: log all messages to stdout\n");
    wprintf(L"  --color-log        Force colored log output while verbose logging\n");
    wprintf(L"  -l                 Log to file: log all messages to \"disp.log\"\n");
    wprintf(L"  -V, --version      Print version information and exit\n");
}

static int enable_vt_mode() {
    // Try to enable support for Virtual Terminal Sequences
    // Requires Windows 10 1511 or newer

    HANDLE con_out_hndl = GetStdHandle(STD_OUTPUT_HANDLE);
    if (con_out_hndl == INVALID_HANDLE_VALUE) {
        int err = GetLastError();
        wchar_t *err_msg;
        get_error_msg(err, &err_msg);
        log_error(L"Failed to get output handle: %s (0x%08X)", err_msg, err);
        LocalFree(err_msg);
        return 1;
    }

    DWORD con_mode = 0;
    if (!GetConsoleMode(con_out_hndl, &con_mode)) {
        int err = GetLastError();
        wchar_t *err_msg;
        get_error_msg(err, &err_msg);
        log_error(L"Failed to get console mode: %s (0x%08X)", err_msg, err);
        LocalFree(err_msg);
        return 1;
    }

    con_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(con_out_hndl, con_mode)) {
        // Most likely not supported in this version of Windows
        log_debug(L"Failed to enable virtual terminal processing");
        return 1;
    }

    return 0;
}

int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_previnst, LPSTR lp_cmd_line, int n_show_cmd) {

    log_set_level(LOG_WARNING);

    // Try to enable VT mode for colored logging support
    // Default to no colors
    if (enable_vt_mode() == 0) {
        // VT mode enabled, use colors
        log_set_color_mode(LOG_COLOR);
    }

    // Parse command line arguments
    int argc = 0;
    wchar_t **argv = NULL;
    argv = CommandLineToArgvW(GetCommandLine(), &argc);

    wchar_t *config_file_path = NULL;
    wchar_t *apply_preset_name = NULL;

    int is_verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-c") == 0 || wcscmp(argv[i], L"--config") == 0) {
            // Config file path
            // A file path should follow
            if (i + 1 >= argc) {
                // No file path
                wprintf(L"Missing config file path\n");
                print_help(argv);
                return 1;
            }
            // Read config path
            config_file_path = _wcsdup(argv[++i]);
        } else if (wcscmp(argv[i], L"-p") == 0 || wcscmp(argv[i], L"--preset") == 0) {
            // Preset
            // A preset name should follow
            if (i + 1 >= argc) {
                // No preset name
                wprintf(L"Missing preset name\n");
                print_help(argv);
                return 1;
            }
            // Read preset name
            apply_preset_name = _wcsdup(argv[++i]);
        } else if (wcscmp(argv[i], L"-v") == 0 || wcscmp(argv[i], L"--verbose") == 0) {
            // Verbose
            log_set_level(LOG_TRACE);
            is_verbose = 1;
        } else if (wcscmp(argv[i], L"-l") == 0) {
            // Create logfile
            log_set_file_level(LOG_TRACE);
        } else if (wcscmp(argv[i], L"-V") == 0 || wcscmp(argv[i], L"--version") == 0) {
            // Version
            wprintf(APP_NAME L" " APP_VER L"\n");
            return 0;
        } else if (wcscmp(argv[i], L"--color-log") == 0) {
            // Force color log output
            log_set_color_mode(LOG_COLOR);
        } else if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--help") == 0) {
            // Help
            print_help(argv);
            return 0;
        }
    }

    LocalFree(argv);

    if (!is_verbose) {
        // If we're not outputting verbose output, detach the console
        // Otherwise the program would have an open console window floating about
        // We could build the program without -mconsole flag, but then the console output (-h, -V, etc.)
        // wouldn't work.
        FreeConsole();
    }

    // Check if an instance is already running
    HANDLE instance_mutex = CreateMutex(NULL, FALSE, APP_FQN);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // An instance is running
        // Check if we have a message to send to it
        if (apply_preset_name != NULL) {
            // We should apply a preset
            log_info(L"Requesting the running process to change the preset to \"%s\"", apply_preset_name);
            // Find out the running instance window
            HWND existing_main_wnd = FindWindow(MAIN_WND_CLASS, APP_NAME);
            if (existing_main_wnd == NULL) {
                log_error(L"No running instance found even though mutex exists");
                MessageBox(NULL, APP_NAME, L"Could not find a running instance of " APP_NAME,
                           MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                return 1;
            }
            // Send a message to the existing main window
            ipc_preset_change_request change_req = {0};
            StringCbCopy(change_req.preset_name, sizeof(change_req.preset_name), apply_preset_name);
            change_req.preset_name_len = wcsnlen_s(apply_preset_name, 127);

            COPYDATASTRUCT copydata = {0};
            copydata.dwData = IPC_APPLY_PRESET;
            copydata.cbData = sizeof(ipc_preset_change_request);
            copydata.lpData = &change_req;

            SendMessage(existing_main_wnd, WM_COPYDATA, (WPARAM) NULL, (LPARAM)(LPVOID) &copydata);
            log_info(L"Sent preset change request to the running process");
            free(apply_preset_name);
        }
        log_info(L"An instance is already running, exiting");
        return 0;
    }
    if (!instance_mutex) {
        if (GetLastError() == ERROR_ACCESS_DENIED) {
            // Try to open
            instance_mutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, APP_FQN);
            if (!instance_mutex) {
                // Failed
                int err = GetLastError();
                wchar_t *err_msg;
                get_error_msg(err, &err_msg);
                log_error(L"Failed to open mutex: %s (0x%08X)", err_msg, err);
                LocalFree(err_msg);
                MessageBox(NULL, APP_NAME, L"Failed to open mutex, exiting", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
                return 1;
            }
        }
        int err = GetLastError();
        wchar_t *err_msg;
        get_error_msg(err, &err_msg);
        log_error(L"Could not create app mutex: %s (0x%08X)", err_msg, err);
        LocalFree(err_msg);
        MessageBox(NULL, APP_NAME, L"Could not create app mutex, exiting", MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
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
    create_tray_icon(&app_context);

    // Populate display data
    populate_display_data(&app_context);

    // Determine a config file path if it's not supplied in the command line arguments
    if (config_file_path == NULL) {
        // Check if there exists a config file in the current working directory
        if (!PathFileExists(DEFAULT_CONFIG_NAME)) {
            // No local config file, default to AppData if possible
            if (disp_config_get_appdata_path(&config_file_path) != DISP_CONFIG_SUCCESS) {
                log_warning(L"Failed to get AppData config path, using \"" DEFAULT_CONFIG_NAME
                            "\" relative to the working directory");
                config_file_path = _wcsdup(DEFAULT_CONFIG_NAME);
            }
        } else {
            config_file_path = _wcsdup(DEFAULT_CONFIG_NAME);
        }
    }
    log_debug(L"Using config file: %s", config_file_path);

    // Check if a config file exists
    if (!PathFileExists(config_file_path)) {
        // No config exists, create a config file
        if (disp_config_save_file(config_file_path, &app_context.config) != DISP_CONFIG_SUCCESS) {
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

    app_context.config_file_path = _wcsdup(config_file_path);
    if (read_config(&app_context, FALSE) != 0) {
        DestroyWindow(hwnd);
        ReleaseMutex(app_context.instance_mutex);
        log_finish();
        return 1;
    }
    free(config_file_path);

    flag_matching_presets(&app_context);

    create_tray_menu(&app_context);

    // Show a notification
    if (app_context.config.notify_on_start) {
        show_notification_message(&app_context, L"Display settings manager is running");
    }

    log_info(L"Ready");

    if (apply_preset_name != NULL) {
        // Apply a preset
        log_info(L"Preset change requested, preset name: \"%s\"", apply_preset_name);
        apply_preset_by_name(&app_context, apply_preset_name);
        free(apply_preset_name);
    }

    // Init OK, start main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    log_info(L"Cleaning up");
    free_monitors(&app_context);
    disp_config_destroy(&app_context.config);
    free(app_context.config_file_path);
    DeleteObject(app_context.align_pattern_font);
    ReleaseMutex(app_context.instance_mutex);

    log_info(L"Exiting");
    log_finish();
    return (int) msg.wParam;
}
