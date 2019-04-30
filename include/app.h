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

#ifndef _APP_H_
#define _APP_H_

#define APP_NAME L"disp"
#define APP_VER L"0.1.0"

#define MSG_NOTIFYICON (WM_APP + 1)
#define NOTIF_MENU_EXIT 1
#define NOTIF_MENU_ABOUT_DISPLAYS 2
#define NOTIF_MENU_CONFIG_SAVE 3
#define NOTIF_MENU_MONITOR_ORIENTATION_SELECT 0x0000F000
#define NOTIF_MENU_MONITOR_ORIENTATION_MONITOR 0x000003FF
#define NOTIF_MENU_MONITOR_ORIENTATION_POSITION 0x00000C00
#define NOTIF_MENU_CONFIG_SELECT 0x0000E000
#define NOTIF_MENU_CONFIG_INDEX 0x00001FFF

#define UNICODE
#include <Windows.h>

typedef struct {
    unsigned int num;
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

#include "config.h"
#include "log.h"

#endif
