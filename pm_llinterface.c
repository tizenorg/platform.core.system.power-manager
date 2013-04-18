/*
 * power-manager
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include <device-node.h>
#include <vconf.h>

#include "pm_llinterface.h"
#include "util.h"
#include "pm_conf.h"
#include "pm_core.h"

#define DISP_INDEX_BIT                      4
#define COMBINE_DISP_CMD(cmd, prop, index)  (cmd = (prop | (index << DISP_INDEX_BIT)))

typedef struct _PMSys PMSys;
struct _PMSys {
	int def_brt;
	int dim_brt;

	int (*sys_power_state) (PMSys *, int);
	int (*bl_onoff) (PMSys *, int);
	int (*bl_brt) (PMSys *, int);
	int (*sys_get_battery_capacity) (PMSys *p);
	int (*sys_get_battery_capacity_raw) (PMSys *p);
	int (*sys_get_battery_charge_full) (PMSys *p);
};

static PMSys *pmsys;

#ifdef ENABLE_X_LCD_ONOFF
#include "pm_x_lcd_onoff.c"
static bool x_dpms_enable = false;
#endif

/*
static void _update_curbrt(PMSys *p)
{
	int power_saving_stat = -1;
	int power_saving_display_stat = -1;
	vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_SYSMODE_STATUS, &power_saving_stat);
	if (power_saving_stat == 1)
		vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_DISPLAY, &power_saving_display_stat);
	if (power_saving_display_stat != 1)
		power_saving_display_stat = 0;
	plugin_intf->OEM_sys_get_backlight_brightness(DEFAULT_DISPLAY, &(p->def_brt), power_saving_display_stat);
}
*/

static int _bl_onoff(PMSys *p, int onoff)
{
	int cmd;
	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_ONOFF, DEFAULT_DISPLAY);
	return device_set_property(DEVICE_TYPE_DISPLAY, cmd, onoff);
}

static int _bl_brt(PMSys *p, int brt)
{
	int cmd;
	int ret;
	int old_brt;

	COMBINE_DISP_CMD(cmd, PROP_DISPLAY_BRIGHTNESS, DEFAULT_DISPLAY);
	ret = device_get_property(DEVICE_TYPE_DISPLAY, cmd, &old_brt);

	/* Update new brightness to vconf */
	if (!ret && (brt != old_brt))
		vconf_set_int(VCONFKEY_PM_CURRENT_BRIGHTNESS, brt);

	/* Update device brightness */
	ret = device_set_property(DEVICE_TYPE_DISPLAY, cmd, brt);

	LOGERR("set brightness %d,%d %d", DEFAULT_DISPLAY, brt, ret);

	return ret;
}

static int _sys_power_state(PMSys *p, int state)
{
	if (state < POWER_STATE_SUSPEND || state > POWER_STATE_POST_RESUME)
		return 0;
	return device_set_property(DEVICE_TYPE_POWER, PROP_POWER_STATE, state);
}

static int _sys_get_battery_capacity(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY, &value);
	if(ret < 0)
		return -1;

	if(value < 0)
		return 0;

	return value;
}

static int _sys_get_battery_capacity_raw(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CAPACITY_RAW, &value);
	if(ret < 0)
		return -1;

	if(value < 0)
		return 0;

	return value;
}

static int _sys_get_battery_charge_full(PMSys *p)
{
	int value = 0;
	int ret = -1;

	ret = device_get_property(DEVICE_TYPE_POWER, PROP_POWER_CHARGE_FULL, &value);
	if(ret < 0)
		return -1;

	if(value < 0)
		return 0;

	return value;
}

static void _init_bldev(PMSys *p, unsigned int flags)
{
	int ret;
	//_update_curbrt(p);
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
	p->sys_power_state = _sys_power_state;
	p->sys_get_battery_capacity = _sys_get_battery_capacity;
	p->sys_get_battery_capacity_raw = _sys_get_battery_capacity_raw;
	p->sys_get_battery_charge_full = _sys_get_battery_charge_full;
}

int system_suspend()
{
	LOGINFO("enter system suspend");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_SUSPEND);

	return 0;
}

int system_pre_suspend()
{
	LOGINFO("enter system pre suspend");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_PRE_SUSPEND);

	return 0;
}

int system_post_resume()
{
	LOGINFO("enter system post resume");
	if (pmsys && pmsys->sys_power_state)
		return pmsys->sys_power_state(pmsys, POWER_STATE_POST_RESUME);

	return 0;
}

int battery_capacity()
{
	if (pmsys && pmsys->sys_get_battery_capacity)
		return pmsys->sys_get_battery_capacity(pmsys);

	return 0;
}

int battery_capacity_raw()
{
	if (pmsys && pmsys->sys_get_battery_capacity_raw)
		return pmsys->sys_get_battery_capacity_raw(pmsys);

	return 0;
}

int battery_charge_full()
{
	if (pmsys && pmsys->sys_get_battery_charge_full)
		return pmsys->sys_get_battery_charge_full(pmsys);

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
	int val = -1;

	ret = vconf_get_int(VCONFKEY_PM_CUSTOM_BRIGHTNESS_STATUS, &val);
	if (ret == 0 && val == VCONFKEY_PM_CUSTOM_BRIGHTNESS_ON) {
		LOGINFO("custom brightness mode! brt no restored");
		return 0;
	}
	if ((status_flag & PWRSV_FLAG) && !(status_flag & BRTCH_FLAG))
		ret = backlight_dim();
	else if (pmsys && pmsys->bl_brt)
		ret = pmsys->bl_brt(pmsys, pmsys->def_brt);

	return ret;
}

int set_default_brt(int level)
{
	if (level < PM_MIN_BRIGHTNESS || level > PM_MAX_BRIGHTNESS)
		level = PM_DEFAULT_BRIGHTNESS;
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

	if (pmsys->bl_onoff == NULL && pmsys->sys_power_state == NULL) {
		LOGERR
		    ("We have no managable resource to reduce the power consumption");
		return -1;
	}

	return 0;
}

int exit_sysfs()
{
	int fd = -1;

	fd = open("/tmp/sem.pixmap_1", O_RDONLY);
	if (fd == -1) {
		LOGERR("X server disable");
		backlight_on();
	}

	backlight_restore();
	free(pmsys);
	pmsys = NULL;
	if (fd != -1)
		close(fd);

	return 0;
}
