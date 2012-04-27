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


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "pm_llinterface.h"
#include "pm_device_plugin.h"
#include "util.h"
#include "pm_conf.h"

typedef struct _PMSys PMSys;
struct _PMSys {
	int def_brt;
	int dim_brt;

	int (*sys_suspend) (PMSys *);
	int (*bl_onoff) (PMSys *, int);
	int (*bl_brt) (PMSys *, int);
};

static PMSys *pmsys;

#ifdef ENABLE_X_LCD_ONOFF
#include "pm_x_lcd_onoff.c"
static bool x_dpms_enable = false;
#endif

static void _update_curbrt(PMSys *p)
{
	plugin_intf->OEM_sys_get_backlight_brightness(DEFAULT_DISPLAY, &(p->def_brt));
}

static int _bl_onoff(PMSys *p, int onoff)
{
	return plugin_intf->OEM_sys_set_lcd_power(DEFAULT_DISPLAY, onoff);
}

static int _bl_brt(PMSys *p, int brightness)
{
	return plugin_intf->OEM_sys_set_backlight_brightness(DEFAULT_DISPLAY, brightness);
}

static int _sys_suspend(PMSys *p)
{
	return plugin_intf->OEM_sys_set_power_state(POWER_STATE_SUSPEND);
}

static void _init_bldev(PMSys *p, unsigned int flags)
{
	int ret;
		_update_curbrt(p);
		p->bl_brt = _bl_brt;
		p->bl_onoff = _bl_onoff;
#ifdef ENABLE_X_LCD_ONOFF
	if (flags & FLAG_X_DPMS) {
		p->bl_onoff = pm_x_set_lcd_backlight;
		x_dpms_enable = true;
	}
#endif
}

static void _init_pmsys(PMSys *p)
{
	char tmp[NAME_MAX];

	get_env(EN_SYS_DIMBRT, tmp, sizeof(tmp));

	p->dim_brt = atoi(tmp);
	p->sys_suspend = _sys_suspend;
}

int system_suspend()
{
	if (pmsys && pmsys->sys_suspend)
		return pmsys->sys_suspend(pmsys);

	return 0;
}

int backlight_on()
{
	LOGINFO("LCD on");

	if (pmsys && pmsys->bl_onoff) {
		return pmsys->bl_onoff(pmsys, STATUS_ON);
	}

	return 0;
}

int backlight_off()
{
	LOGINFO("LCD off");

	if (pmsys && pmsys->bl_onoff) {
#ifdef ENABLE_X_LCD_ONOFF
		if (x_dpms_enable == false)
#endif
			usleep(30000);
		return pmsys->bl_onoff(pmsys, STATUS_OFF);
	}

	return 0;
}

int backlight_dim()
{
	int ret = 0;
	if (pmsys && pmsys->bl_brt) {
		ret = pmsys->bl_brt(pmsys, pmsys->dim_brt);
	}
	return ret;
}

int backlight_restore()
{
	int ret = 0;
	if (pmsys && pmsys->bl_brt) {
		ret = pmsys->bl_brt(pmsys, pmsys->def_brt);
	}
	return ret;
}

int set_default_brt(int level)
{
	int max_brt;

	if (0 == plugin_intf->OEM_sys_get_backlight_max_brightness(DEFAULT_DISPLAY, &max_brt)) {
		if (level > max_brt) {
			pmsys->def_brt = max_brt;

			return 0;
		}
	}
	pmsys->def_brt = level;

	return 0;
}

inline int check_wakeup_src(void)
{
	/*  TODO if nedded.
	 * return wackeup source. user input or device interrupts? (EVENT_DEVICE or EVENT_INPUT)
	 */
	return EVENT_DEVICE;
}

int init_sysfs(unsigned int flags)
{
	int ret;

	pmsys = (PMSys *) malloc(sizeof(PMSys));
	if (pmsys == NULL) {
		LOGERR("Not enough memory to alloc PM Sys");
		return -1;
	}

	memset(pmsys, 0x0, sizeof(PMSys));

	_init_pmsys(pmsys);
	_init_bldev(pmsys, flags);

	if (pmsys->bl_onoff == NULL && pmsys->sys_suspend == NULL) {
		LOGERR
			("We have no managable resource to reduce the power consumption");
		return -1;
	}

	return 0;
}

int exit_sysfs()
{
	int fd;

	fd = open("/tmp/sem.pixmap_1", O_RDONLY);
	if (fd == -1) {
		LOGERR("X server disable");
		backlight_on();
	}

	backlight_restore();
	free(pmsys);
	pmsys = NULL;

	return 0;
}
