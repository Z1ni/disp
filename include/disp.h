#ifndef _DISP_H_
#define _DISP_H_

#define _UNICODE
#define UNICODE

#include <Windows.h>

#define APP_NAME L"disp"
#define APP_VER L"0.1.0"

#define MSG_NOTIFYICON (WM_APP+1)
#define NOTIF_MENU_EXIT 1
#define NOTIF_MENU_ABOUT_DISPLAYS 2
#define NOTIF_MENU_CONFIG_SAVE 3
#define NOTIF_MENU_MONITOR_ORIENTATION_SELECT 0x0000F000
#define NOTIF_MENU_MONITOR_ORIENTATION_MONITOR 0x000003FF
#define NOTIF_MENU_MONITOR_ORIENTATION_POSITION 0x00000C00
#define NOTIF_MENU_CONFIG_SELECT 0x0000E000
#define NOTIF_MENU_CONFIG_INDEX 0x00001FFF

typedef struct {
    wchar_t name[CCHDEVICENAME];
    wchar_t friendly_name[64];
    RECT rect;
    POINTL virt_pos;
    DEVMODE devmode;
    wchar_t device_id[128];
} monitor_t;

typedef struct {
    int width;
    int height;
} virt_size_t;

typedef struct {
	wchar_t preset_name[64];
	BOOL cancel;
} preset_dialog_data_t;

#endif