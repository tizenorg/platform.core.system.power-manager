/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.tizenopensource.org/license
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
#define PREDEF_POWEROFF 	"poweroff"

#define LONG_PRESS_INTERVAL		1000000	/* 1000 ms */
#define COMBINATION_INTERVAL	300000	/* 300 ms */

#define KEY_RELEASED		0
#define KEY_PRESSED			1
#define KEY_BEING_PRESSED	2

#define KEY_COMBINATION_STOP			0
#define KEY_COMBINATION_START			1
#define KEY_COMBINATION_SCREENCAPTURE	2

static guint longkey_timeout_id = 0;
static guint combination_timeout_id = 0;
static int cancel_lcdoff;
static int key_combination = KEY_COMBINATION_STOP;

void unlock()
{
	vconf_set_int(VCONFKEY_IDLE_LOCK_STATE, VCONFKEY_IDLE_UNLOCK);
}

static gboolean longkey_pressed(gpointer data)
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
	return FALSE;
}

static gboolean combination_failed(gpointer data)
{
	key_combination = KEY_COMBINATION_STOP;
	return 0;
}

int check_key_filter(int length, char buf[])
{
	struct input_event *pinput;
	static struct timeval pressed_time;
	int ignore = true;
	int idx = 0;
	int val = -1;

	do {
		pinput = (struct input_event *)&buf[idx];
		if (pinput->type == EV_SYN) ;
		else if (pinput->type == EV_KEY) {
			if (pinput->code == KEY_POWER) {
				if (pinput->value == KEY_RELEASED) {	/* release */
					if (!(cur_state == S_LCDOFF || cur_state == S_SLEEP) && !cancel_lcdoff && !(key_combination == KEY_COMBINATION_SCREENCAPTURE)) {
						check_processes(S_LCDOFF);
						check_processes(S_LCDDIM);
						if( check_holdkey_block(S_LCDOFF) == false &&
								check_holdkey_block(S_LCDDIM) == false) {
							delete_condition(S_LCDOFF);
							delete_condition(S_LCDDIM);
							/* LCD off forcly */
							recv_data.pid = -1;
							recv_data.cond = 0x400;
							if(vconf_get_int(VCONFKEY_FLASHPLAYER_FULLSCREEN, &val)<0 || val == 0)
								(*g_pm_callback)(PM_CONTROL_EVENT, &recv_data);
						}
					} else
						ignore = false;
					key_combination = KEY_COMBINATION_STOP;
					if (combination_timeout_id > 0) {
						g_source_remove(combination_timeout_id);
						combination_timeout_id = 0;
					}
					cancel_lcdoff = 0;
					if (longkey_timeout_id > 0) {
						g_source_remove(longkey_timeout_id);
						longkey_timeout_id = 0;
					}
				} else if (pinput->value == KEY_PRESSED) {
					LOGINFO("power key pressed");
					pressed_time.tv_sec = (pinput->time).tv_sec;
					pressed_time.tv_usec = (pinput->time).tv_usec;
					if (key_combination == KEY_COMBINATION_STOP) {
						/* add long key timer */
						longkey_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT, LONG_PRESS_INTERVAL / 1000,
								(GSourceFunc)longkey_pressed, NULL, NULL);
						key_combination = KEY_COMBINATION_START;
						combination_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT, COMBINATION_INTERVAL / 1000,
								(GSourceFunc)combination_failed, NULL, NULL);
					} else if (key_combination == KEY_COMBINATION_START) {
						if (combination_timeout_id > 0) {
							g_source_remove(combination_timeout_id);
							combination_timeout_id = 0;
						}
						LOGINFO("capture mode");
						key_combination = KEY_COMBINATION_SCREENCAPTURE;
						ignore = false;
					}

				} else if (pinput->value == KEY_BEING_PRESSED &&	/* being pressed */
						(((pinput->time).tv_sec - pressed_time.tv_sec) * 1000000 + ((pinput->time).tv_usec - pressed_time.tv_usec))
						> LONG_PRESS_INTERVAL) {
					longkey_pressed(NULL);
				}
			} else {
				if (pinput->code == KEY_VOLUMEDOWN) {
					if (pinput->value == KEY_PRESSED) {   /* press */
						if (key_combination == KEY_COMBINATION_STOP) {
							key_combination = KEY_COMBINATION_START;
							combination_timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT, COMBINATION_INTERVAL / 1000, 
									(GSourceFunc)combination_failed, NULL, NULL);
						} else {
							if (key_combination	== KEY_COMBINATION_START) {
								if (combination_timeout_id > 0) {
									g_source_remove(combination_timeout_id);
									combination_timeout_id = 0;
								}
								LOGINFO	("capture mode");
								key_combination	= KEY_COMBINATION_SCREENCAPTURE;
								ignore = false;
							}
						}
					} else if (pinput->value == KEY_RELEASED) {
						if (key_combination != KEY_COMBINATION_SCREENCAPTURE) {
							key_combination	= KEY_COMBINATION_STOP;
							if (combination_timeout_id > 0) {
								g_source_remove(combination_timeout_id);
								combination_timeout_id = 0;
							}
							if (cur_state == S_LCDDIM || cur_state == S_NORMAL)
								ignore = false;
						}
					}
				} else {
					key_combination = KEY_COMBINATION_STOP;
					if (combination_timeout_id > 0) {
						g_source_remove(combination_timeout_id);
						combination_timeout_id = 0;
					}
					if (pinput->code == KEY_MENU) {
						if (cur_state == S_LCDDIM || cur_state == S_NORMAL || cur_state == S_LCDOFF)
							ignore = false;
					} else if (pinput->code == KEY_VOLUMEUP
							|| pinput->code == KEY_VOLUMEDOWN
							|| pinput->code == KEY_CAMERA
							|| pinput->code == KEY_EXIT
							|| pinput->code == KEY_PHONE
							|| pinput->code == KEY_CONFIG
							|| pinput->code == KEY_SEARCH) {
						if (cur_state == S_LCDDIM || cur_state == S_NORMAL)
							ignore = false;
					} else if (pinput->code == KEY_SCREENLOCK
							|| pinput->code == 0x1DB
							|| pinput->code == 0x1DC
							|| pinput->code == 0x1DD
							|| pinput->code == 0x1DE) 
						;
					else
						ignore = false;
				}
			}
		}
		else if (pinput->type == EV_REL)
			ignore = false;
		else if (pinput->type == EV_ABS){
			if (cur_state == S_LCDDIM || cur_state == S_NORMAL)
				ignore = false;
		}

		idx += sizeof(struct input_event);
		if (ignore == true && length <= idx)
			return 1;
	} while (length > idx);
	return 0;
}
