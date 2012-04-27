/* 
 * Copyright (c) 2000 - 2012 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This file is part of power-manager
 * Written by DongGi Jang <dg0402.jang@samsung.com>
 *
 * PROPRIETARY/CONFIDENTIAL
 *
 * This software is the confidential and proprietary information of
 * SAMSUNG ELECTRONICS ("Confidential Information"). You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered
 * into with SAMSUNG ELECTRONICS.
 *
 * SAMSUNG make no representations or warranties about the suitability
 * of the software, either express or implied, including but not limited
 * to the implied warranties of merchantability, fitness for a particular
 * purpose, or non-infringement. SAMSUNG shall not be liable for any
 * damages suffered by licensee as a result of using, modifying or
 * distributing this software or its derivatives.
*/


#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pm_conf.h"

enum {
	IDX_NAME = 0,
	IDX_DEFAULT,
	IDX_END
};

static char *def_values[][IDX_END] = {
	{"PM_INPUT", "/dev/event0:/dev/event1"},
	{"PM_TO_START", "0"},
	{"PM_TO_NORMAL", "600"},
	{"PM_TO_LCDDIM", "5"},
	{"PM_TO_LCDOFF", "5"},
	{"PM_TO_SLEEP", "0"},
	{"PM_SYS_POWER", "/sys/power/state"},
	{"PM_SYS_BRIGHT", "/sys/class/backlight/mobile-bl/brightness"},
	{"PM_SYS_BRIGHT", "/sys/class/backlight/mobile-bl/max_brightness"},
	{"PM_SYS_BLPWR", "/sys/class/backlight/mobile-bl/bl_power"},
	{"PM_SYS_DIMBRT", "0"},
	{"PM_SYS_BLON", "0"},
	{"PM_SYS_BLOFF", "4"},
	{"PM_SYS_FB_NORMAL", "1"},
	{"PM_SYS_STATE", "mem"},
	{"PM_EXEC_PRG", NULL},
	{"PM_END", ""},
};

static char *_find_default(char *name)
{
	char *ret = NULL;
	int i = 0;

	while (strcmp("PM_END", def_values[i][IDX_NAME])) {
		if (!strcmp(name, def_values[i][IDX_NAME])) {
			ret = def_values[i][IDX_DEFAULT];
			break;
		}
		i++;
	}
	return ret;
}

int get_env(char *name, char *buf, int size)
{
	char *ret;

	ret = getenv(name);
	if ((ret == NULL) || (strlen(ret) > 1024)) {
		ret = _find_default(name);
		if (ret)
			snprintf(buf, size, "%s", ret);
		else
			snprintf(buf, size, "");
	} else {
		snprintf(buf, size, "%s", ret);
	}

	return 0;
}
