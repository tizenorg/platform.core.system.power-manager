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
#include "pm_setting.h"
#include "pm_conf.h"
#include "util.h"

static const char *setting_keys[SETTING_GET_END] = {
	[SETTING_TO_NORMAL] = VCONFKEY_SYSMAN_LCD_TIMEOUT_NORMAL,
	[SETTING_LOW_BATT] = VCONFKEY_SYSMAN_BATTERY_STATUS_LOW,
	[SETTING_CHARGING] = VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW,
	[SETTING_BRT_LEVEL] = VCONFKEY_SETAPPL_LCD_BRIGHTNESS,
	[SETTING_LOCK_SCREEN] = VCONFKEY_IDLE_LOCK_STATE,
};

static int (*update_setting) (int key_idx, int val);

int get_charging_status(int *val)
{
	return vconf_get_int(VCONFKEY_SYSMAN_BATTERY_CHARGE_NOW, val);
}

int get_lowbatt_status(int *val)
{
	return vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, val);
}

int get_usb_status(int *val)
{
	return vconf_get_int(VCONFKEY_SYSMAN_USB_STATUS, val);
}

int set_setting_pmstate(int val)
{
	return vconf_set_int(VCONFKEY_PM_STATE, val);
}

int get_setting_brightness(int *level)
{
	return vconf_get_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, level);
}

int get_run_timeout(int *timeout)
{
	int dim_timeout = -1, vconf_timeout = -1, ret;
	get_dim_timeout(&dim_timeout);

	if(dim_timeout < 0) {
		LOGERR("Can not get dim timeout. set default 5 seconds");
		dim_timeout = 5;
	}

	ret = vconf_get_int(setting_keys[SETTING_TO_NORMAL], &vconf_timeout);
	*timeout = vconf_timeout - dim_timeout;
	return ret;

}

int get_dim_timeout(int *timeout)
{
	char buf[255];
	/* TODO if needed */
	*timeout = 5;		/* default timeout */
	get_env("PM_TO_LCDDIM", buf, sizeof(buf));
	LOGDBG("Get lcddim timeout [%s]", buf);
	*timeout = atoi(buf);
	return 0;
}

int get_off_timeout(int *timeout)
{
	char buf[255];
	/* TODO if needed */
	*timeout = 5;		/* default timeout */
	get_env("PM_TO_LCDOFF", buf, sizeof(buf));
	LOGDBG("Get lcdoff timeout [%s]", buf);
	*timeout = atoi(buf);
	return 0;
}

static int setting_cb(keynode_t *key_nodes, void *data)
{
	keynode_t *tmp = key_nodes;

	if ((int)data > SETTING_END) {
		LOGERR("Unknown setting key: %s, idx= %d",
		       vconf_keynode_get_name, (int)data);
		return -1;
	}
	if (update_setting != NULL) {
		update_setting((int)data, vconf_keynode_get_int(tmp));
	}

	return 0;
}

int init_setting(int (*func) (int key_idx, int val))
{
	int i;

	if (func != NULL)
		update_setting = func;

	for (i = SETTING_BEGIN; i < SETTING_GET_END; i++) {
		vconf_notify_key_changed(setting_keys[i], (void *)setting_cb,
					 (void *)i);
	}

	return 0;
}

int exit_setting()
{
	int i;
	for (i = SETTING_BEGIN; i < SETTING_GET_END; i++) {
		vconf_ignore_key_changed(setting_keys[i], (void *)setting_cb);
	}

	return 0;
}
