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


#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <glib.h>
#include <vconf.h>
#include <sensor.h>

#include "pm_core.h"
#include "pm_device_plugin.h"

#define SAMPLING_INTERVAL	1	/* 1 sec */
#define MAX_FAULT			5

static int (*prev_init_extention) (void *data);
static int (*_default_action) (int);
static int alc_timeout_id = 0;
static int sf_handle = -1;
static int fault_count = 0;

static gboolean alc_handler(gpointer data)
{
	int value = 0;
	static int cur_index = 1;
	static int old_index = 1;

	sensor_data_t light_data;
	if (cur_state != S_NORMAL){
		if (alc_timeout_id != 0)
			g_source_remove(alc_timeout_id);
		alc_timeout_id = 0;
	} else {
		if (sf_get_data(sf_handle, LIGHT_BASE_DATA_SET, &light_data) < 0) {
			fault_count++;
		} else {
			if (light_data.values[0] < 0.0 || light_data.values[0] > 10.0) {
				LOGINFO("fail to load light data : %d",	(int)light_data.values[0]);
				fault_count++;
			} else {
				int tmp_value;
				int power_saving_stat = -1;
				int power_saving_display_stat = -1;
				vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_SYSMODE_STATUS, &power_saving_stat);
				if (power_saving_stat == 1)
					vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_DISPLAY, &power_saving_display_stat);
				if (power_saving_display_stat != 1)
					power_saving_display_stat = 0;
				value = PM_MAX_BRIGHTNESS * (int)light_data.values[0] / 10;
				plugin_intf->OEM_sys_get_backlight_brightness(DEFAULT_DISPLAY, &tmp_value, power_saving_display_stat);
				if (tmp_value != value) {
					set_default_brt(value);
					backlight_restore();
				}
				LOGINFO("load light data : %d, brightness : %d", (int)light_data.values[0], value);
			}
		}
	}

	if (fault_count > MAX_FAULT) {
		if (alc_timeout_id != 0)
			g_source_remove(alc_timeout_id);
		alc_timeout_id = 0;
		vconf_set_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, SETTING_BRIGHTNESS_AUTOMATIC_OFF);
		LOGERR("Fault counts is over 5, disable automatic brightness");
		return FALSE;
	}

	if (alc_timeout_id != 0)
		return TRUE;

	return FALSE;
}

static int alc_action(int timeout)
{
	LOGINFO("alc action");
	/* sampling timer add */
	if (alc_timeout_id == 0 && !(status_flag & PWRSV_FLAG))
		alc_timeout_id =
		    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
					       SAMPLING_INTERVAL,
					       (GSourceFunc) alc_handler, NULL,
					       NULL);

	if (_default_action != NULL)
		return _default_action(timeout);

	/* unreachable code */
	return -1;
}

static int connect_sfsvc()
{
	int sf_state = -1;
	/* connect with sensor fw */
	LOGINFO("connect with sensor fw");
	sf_handle = sf_connect(LIGHT_SENSOR);
	if (sf_handle < 0) {
		LOGERR("sensor attach fail");
		return -1;
	}
	sf_state = sf_start(sf_handle, 0);
	if (sf_state < 0) {
		LOGERR("sensor attach fail");
		sf_disconnect(sf_handle);
		sf_handle = -1;
		return -2;
	}
	fault_count = 0;
	return 0;
}

static int disconnect_sfsvc()
{
	LOGINFO("disconnect with sensor fw");
	if(sf_handle >= 0)
	{
		sf_stop(sf_handle);
		sf_disconnect(sf_handle);
		sf_handle = -1;
	}

	if (_default_action != NULL) {
		states[S_NORMAL].action = _default_action;
		_default_action = NULL;
	}
	if (alc_timeout_id != 0) {
		g_source_remove(alc_timeout_id);
		alc_timeout_id = 0;
	}

	return 0;
}

static inline void set_brtch_state()
{
	if (status_flag & PWRSV_FLAG) {
		status_flag |= BRTCH_FLAG;
		vconf_set_bool(VCONFKEY_PM_BRIGHTNESS_CHANGED_IN_LPM, true);
		LOGINFO("brightness changed in low battery,"
		    "escape dim state (light)");
	}
}

static int set_alc_function(keynode_t *key_nodes, void *data)
{
	int onoff = 0;
	int ret = -1;
	int brt = -1;
	int default_brt = -1;
	int max_brt = -1;

	if (key_nodes == NULL) {
		LOGERR("wrong parameter, key_nodes is null");
		return -1;
	}

	onoff = vconf_keynode_get_int(key_nodes);

	if (onoff == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		if(connect_sfsvc() < 0)
			return -1;
		/* escape dim state if it's in low battery.*/
		set_brtch_state();

		/* change alc action func */
		if (_default_action == NULL)
			_default_action = states[S_NORMAL].action;
		states[S_NORMAL].action = alc_action;
		alc_timeout_id =
		    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
					       SAMPLING_INTERVAL,
					       (GSourceFunc) alc_handler, NULL,
					       NULL);
	} else if (onoff == SETTING_BRIGHTNESS_AUTOMATIC_PAUSE) {
		LOGINFO("auto brightness paused!");
		disconnect_sfsvc();
	} else {
		disconnect_sfsvc();
		/* escape dim state if it's in low battery.*/
		set_brtch_state();

		ret = get_setting_brightness(&default_brt);
		if (ret != 0 || (default_brt < PM_MIN_BRIGHTNESS || default_brt > PM_MAX_BRIGHTNESS)) {
			LOGINFO("fail to read vconf value for brightness");
			brt = PM_DEFAULT_BRIGHTNESS;
			if(default_brt < PM_MIN_BRIGHTNESS || default_brt > PM_MAX_BRIGHTNESS)
				vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt);
			default_brt = brt;
		}

		set_default_brt(default_brt);
		backlight_restore();
	}

	return 0;
}

static gboolean check_sfsvc(gpointer data)
{
	/* this function will return opposite value for re-callback in fail */
	int vconf_auto;
	int sf_state = 0;

	LOGINFO("register sfsvc");

	vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &vconf_auto);
	if (vconf_auto == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		if(connect_sfsvc() < 0)
			return TRUE;

		/* change alc action func */
		if (_default_action == NULL)
			_default_action = states[S_NORMAL].action;
		states[S_NORMAL].action = alc_action;
		alc_timeout_id =
			g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
					SAMPLING_INTERVAL,
					(GSourceFunc) alc_handler, NULL,
					NULL);
		if (alc_timeout_id != 0)
			return FALSE;
		disconnect_sfsvc();
		return TRUE;
	}
	LOGINFO("change vconf value before registering sfsvc");
	return FALSE;
}

static int prepare_lsensor(void *data)
{
	int alc_conf;
	int sf_state = 0;

	vconf_get_int(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT, &alc_conf);

	if (alc_conf == SETTING_BRIGHTNESS_AUTOMATIC_ON) {
		g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
				SAMPLING_INTERVAL,
				(GSourceFunc) check_sfsvc, NULL,
				NULL);
	}

	/* add auto_brt_setting change handler */
	vconf_notify_key_changed(VCONFKEY_SETAPPL_BRIGHTNESS_AUTOMATIC_INT,
				 (void *)set_alc_function, NULL);

	if (prev_init_extention != NULL)
		return prev_init_extention(data);

	return 0;
}

static void __attribute__ ((constructor)) pm_lsensor_init()
{
	_default_action = NULL;
	if (pm_init_extention != NULL)
		prev_init_extention = pm_init_extention;
	pm_init_extention = prepare_lsensor;
}

static void __attribute__ ((destructor)) pm_lsensor_fini()
{

}
