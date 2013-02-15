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


#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include <vconf.h>
#include <sysman.h>

#include "util.h"
#include "pm_core.h"
#include "pm_poll.h"

#include <linux/input.h>
#ifndef KEY_SCREENLOCK
#define KEY_SCREENLOCK		0x98
#endif

#define PREDEF_PWROFF_POPUP	"pwroff-popup"
#define PREDEF_LEAVESLEEP	"leavesleep"
#define PREDEF_POWEROFF		"poweroff"

#define USEC_PER_SEC			1000000
#define LONG_PRESS_INTERVAL		1000000	/* 1000 ms */
#define COMBINATION_INTERVAL		300000	/* 300 ms */
#define POWER_KEY_PRESS_IGNORE_TIME	700000  /* 700 ms */

#define KEY_RELEASED		0
#define KEY_PRESSED		1
#define KEY_BEING_PRESSED	2

#define KEY_COMBINATION_STOP		0
#define KEY_COMBINATION_START		1
#define KEY_COMBINATION_SCREENCAPTURE	2

static struct timeval pressed_time;
static guint longkey_timeout_id = 0;
static guint combination_timeout_id = 0;
static int cancel_lcdoff;
static int key_combination = KEY_COMBINATION_STOP;
static int powerkey_ignored = false;

void unlock()
{
	vconf_set_int(VCONFKEY_IDLE_LOCK_STATE, VCONFKEY_IDLE_UNLOCK);
}

static inline int current_state_in_on()
{
	return (cur_state == S_LCDDIM || cur_state == S_NORMAL);
}

static void longkey_pressed()
{
	int rc = -1, val = 0;
	LOGINFO("Power key long pressed!");
	cancel_lcdoff = 1;

	rc = vconf_get_int(VCONFKEY_TESTMODE_POWER_OFF_POPUP, &val);

	if (rc < 0 || val != 1) {
		if (sysman_call_predef_action(PREDEF_PWROFF_POPUP, 0) <
				0)
			LOGERR("poweroff popup exec failed");
	} else {
		if (sysman_call_predef_action(PREDEF_POWEROFF, 0) < 0) {
			LOGERR("poweroff exec failed");
			system("poweroff");
		}

	}
	(*g_pm_callback) (INPUT_POLL_EVENT, NULL);
}

static gboolean longkey_pressed_cb(gpointer data)
{
	longkey_pressed();
	longkey_timeout_id = 0;

	return FALSE;
}

static gboolean combination_failed_cb(gpointer data)
{
	key_combination = KEY_COMBINATION_STOP;
	combination_timeout_id = 0;

	return FALSE;
}

static unsigned long timediff_usec(struct timeval t1, struct timeval t2)
{
	unsigned long udiff;

	udiff = (t2.tv_sec - t1.tv_sec) * USEC_PER_SEC;
	udiff += (t2.tv_usec - t1.tv_usec);

	return udiff;
}

static void stop_key_combination()
{
	key_combination = KEY_COMBINATION_STOP;
	if (combination_timeout_id > 0) {
		g_source_remove(combination_timeout_id);
		combination_timeout_id = 0;
	}
}

static int process_power_key(struct input_event *pinput)
{
	int ignore = true;

	switch (pinput->value) {
	case KEY_RELEASED:
		if (current_state_in_on() && !cancel_lcdoff &&
			!(key_combination == KEY_COMBINATION_SCREENCAPTURE)) {
			check_processes(S_LCDOFF);
			check_processes(S_LCDDIM);

			if (!check_holdkey_block(S_LCDOFF) &&
				!check_holdkey_block(S_LCDDIM)) {
				delete_condition(S_LCDOFF);
				delete_condition(S_LCDDIM);
				/* LCD off forcly */
				recv_data.pid = -1;
				recv_data.cond = 0x400;
				(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);
			}
		} else {
			if (!powerkey_ignored)
				ignore = false;
		}

		stop_key_combination();
		cancel_lcdoff = 0;
		if (longkey_timeout_id > 0) {
			g_source_remove(longkey_timeout_id);
			longkey_timeout_id = 0;
		}
		break;
	case KEY_PRESSED:
		if (timediff_usec(pressed_time, pinput->time) <
		    POWER_KEY_PRESS_IGNORE_TIME) {
			LOGINFO("power key double pressed ignored");
			powerkey_ignored = true;
			break;
		} else {
			powerkey_ignored = false;
		}
		LOGINFO("power key pressed");
		pressed_time.tv_sec = (pinput->time).tv_sec;
		pressed_time.tv_usec = (pinput->time).tv_usec;
		if (key_combination == KEY_COMBINATION_STOP) {
			/* add long key timer */
			longkey_timeout_id = g_timeout_add_full(
					G_PRIORITY_DEFAULT,
					LONG_PRESS_INTERVAL / 1000,
					(GSourceFunc)longkey_pressed_cb,
					NULL, NULL);
			key_combination = KEY_COMBINATION_START;
			combination_timeout_id = g_timeout_add_full(
					G_PRIORITY_DEFAULT,
					COMBINATION_INTERVAL / 1000,
					(GSourceFunc)combination_failed_cb,
					NULL, NULL);
		} else if (key_combination == KEY_COMBINATION_START) {
			if (combination_timeout_id > 0) {
				g_source_remove(combination_timeout_id);
				combination_timeout_id = 0;
			}
			LOGINFO("capture mode");
			key_combination = KEY_COMBINATION_SCREENCAPTURE;
			ignore = false;
		}
		break;
	case KEY_BEING_PRESSED:
		if (timediff_usec(pressed_time, pinput->time) >
			LONG_PRESS_INTERVAL)
			longkey_pressed();
		break;
	}

	return ignore;
}

static int process_volumedown_key(struct input_event *pinput)
{
	int ignore = true;

	if (pinput->value == KEY_PRESSED) {
		if (key_combination == KEY_COMBINATION_STOP) {
			key_combination = KEY_COMBINATION_START;
			combination_timeout_id = g_timeout_add_full(
				G_PRIORITY_DEFAULT,
				COMBINATION_INTERVAL / 1000,
				(GSourceFunc)combination_failed_cb,
				NULL, NULL);
		} else if (key_combination == KEY_COMBINATION_START) {
			if (combination_timeout_id > 0) {
				g_source_remove(combination_timeout_id);
				combination_timeout_id = 0;
			}
			LOGINFO("capture mode");
			key_combination = KEY_COMBINATION_SCREENCAPTURE;
			ignore = false;
		}
	} else if (pinput->value == KEY_RELEASED) {
		if (key_combination != KEY_COMBINATION_SCREENCAPTURE) {
			stop_key_combination();
			if (current_state_in_on())
				ignore = false;
		}
	}

	return ignore;
}

static int check_key(struct input_event *pinput)
{
	int ignore = true;

	switch (pinput->code) {
	case KEY_POWER:
		ignore = process_power_key(pinput);
		break;
	case KEY_VOLUMEDOWN:
		ignore = process_volumedown_key(pinput);
		break;
	case KEY_VOLUMEUP:
	case KEY_CAMERA:
	case KEY_EXIT:
	case KEY_PHONE:
	case KEY_CONFIG:
	case KEY_SEARCH:
		stop_key_combination();
		if (current_state_in_on())
			ignore = false;
		break;
	case KEY_SCREENLOCK:
	case 0x1DB:
	case 0x1DC:
	case 0x1DD:
	case 0x1DE:
		stop_key_combination();
		break;
	default:
		stop_key_combination();
		ignore = false;
	}

	return ignore;
}

int check_key_filter(int length, char buf[])
{
	struct input_event *pinput;
	int ignore = true;
	int idx = 0;

	do {
		pinput = (struct input_event *)&buf[idx];
		switch (pinput->type) {
		case EV_KEY:
			ignore = check_key(pinput);
			break;
		case EV_REL:
			ignore = false;
			break;
		case EV_ABS:
			if (current_state_in_on())
				ignore = false;
			break;
		}

		idx += sizeof(struct input_event);
		if (ignore == true && length <= idx)
			return 1;
	} while (length > idx);

	return 0;
}

