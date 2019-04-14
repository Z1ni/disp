#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include "disp.h"

typedef struct {
	app_config_t config;
	virt_size_t display_virtual_size;
	HMENU notif_menu;
	GUID notify_guid;
	size_t monitor_count;
	monitor_t *monitors;
} app_ctx_t;

#endif