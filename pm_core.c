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


/**
 * @file	pm_core.c
 * @version	0.2
 * @brief	Power manager main loop.
 *
 * This file includes Main loop, the FSM, signal processing.
 */
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <heynoti.h>
#include <sysman.h>
#include <aul.h>
#include <vconf-keys.h>

#include "pm_device_plugin.h"
#include "pm_core.h"

#define USB_CON_PIDFILE			"/var/run/.system_server.pid"
#define PM_STATE_LOG_FILE		"/var/log/pm_state.log"
#define PM_WAKEUP_NOTI_NAME		"system_wakeup"
#define PM_EVENT_NOTI_NAME      "pm_event"
#define PM_EVENT_NOTI_PATH      "/opt/share/noti/"PM_EVENT_NOTI_NAME

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

#define LOCKSTATUS_TIMEOUT		3
#define TELEPHONY_SIGNAL_TIMEOUT	10

#define SET_BRIGHTNESS_IN_BOOTLOADER "/usr/bin/save_blenv SLP_LCD_BRIGHT"

/* default transition, action fuctions */
static int default_trans(int evt);
static int default_action(int timeout);
static int default_check(int next);

unsigned int status_flag;

static void (*power_saving_func) (int onoff);
static void reset_timeout(int timeout);

int cur_state;
int old_state;
static GMainLoop *mainloop;
guint timeout_src_id;
static time_t last_t;

struct state states[S_END] = {
	{S_START, default_trans, default_action, default_check,},
	{S_NORMAL, default_trans, default_action, default_check,},
	{S_LCDDIM, default_trans, default_action, default_check,},
	{S_LCDOFF, default_trans, default_action, default_check,},
	{S_SLEEP, default_trans, default_action, default_check,}
};

int (*pm_init_extention) (void *data);
void (*pm_exit_extention) (void);

static char state_string[5][10] =
    { "S_START", "S_NORMAL", "S_LCDDIM", "S_LCDOFF", "S_SLEEP" };

static int trans_table[S_END][EVENT_END] = {
	/* Timeout , Input */
	{S_START, S_START},	/* S_START */
	{S_LCDDIM, S_NORMAL},	/* S_NORMAL */
	{S_LCDOFF, S_NORMAL},	/* S_LCDDIM */
	{S_SLEEP, S_NORMAL},	/* S_LCDOFF */
	{S_LCDOFF, S_NORMAL},	/* S_SLEEP, When wake up by devices, go lcd_off state  */
};

#define SHIFT_UNLOCK		4
#define MASK_RESET_TIMEOUT	0x8	/* 1000 */
#define MASK_MARGIN_TIMEOUT	(0x1 << 8)
#define SHIFT_CHANGE_STATE	7
#define CHANGE_STATE_BIT	0xF00	/* 1111 0000 0000 */
#define LOCK_SCREEN_TIMEOUT	5
#define SHIFT_HOLD_KEY_BLOCK	16

#define DEFAULT_NORMAL_TIMEOUT	30
#define DEFAULT_DIM_TIMEOUT		5
#define DEFAULT_OFF_TIMEOUT		5
#define GET_HOLDKEY_BLOCK_STATE(x) ((x >> SHIFT_HOLD_KEY_BLOCK) & 0x1)

static int received_sleep_cmd = 0;

typedef struct _node {
	pid_t pid;
	int timeout_id;
	gboolean holdkey_block;
	struct _node *next;
} Node;

static Node *cond_head[S_END];

static int refresh_app_cond()
{
	trans_condition = 0;

	if (cond_head[S_LCDDIM] != NULL)
		trans_condition = trans_condition | MASK_DIM;
	if (cond_head[S_LCDOFF] != NULL)
		trans_condition = trans_condition | MASK_OFF;
	if (cond_head[S_SLEEP] != NULL)
		trans_condition = trans_condition | MASK_SLP;

	return 0;
}

static Node *find_node(enum state_t s_index, pid_t pid)
{
	Node *t = cond_head[s_index];

	while (t != NULL) {
		if (t->pid == pid)
			break;
		t = t->next;
	}
	return t;
}

static Node *add_node(enum state_t s_index, pid_t pid, int timeout_id,
		gboolean holdkey_block)
{
	Node *n;

	n = (Node *) malloc(sizeof(Node));
	if (n == NULL) {
		LOGERR("Not enough memory, add cond. fail");
		return NULL;
	}

	n->pid = pid;
	n->timeout_id = timeout_id;
	n->holdkey_block = holdkey_block;
	n->next = cond_head[s_index];
	cond_head[s_index] = n;

	refresh_app_cond();
	return n;
}

static int del_node(enum state_t s_index, Node *n)
{
	Node *t;
	Node *prev;

	if (n == NULL)
		return 0;

	t = cond_head[s_index];
	prev = NULL;
	while (t != NULL) {
		if (t == n) {
			if (prev != NULL)
				prev->next = t->next;
			else
				cond_head[s_index] = cond_head[s_index]->next;
			free(t);
			break;
		}
		prev = t;
		t = t->next;
	}
	refresh_app_cond();
	return 0;
}

static gboolean del_dim_cond(gpointer data)
{
	Node *tmp = NULL;
	LOGINFO("delete prohibit dim condition by timeout\n");

	tmp = find_node(S_LCDDIM, (pid_t) data);
	del_node(S_LCDDIM, tmp);

	if (timeout_src_id == 0)
		states[cur_state].trans(EVENT_TIMEOUT);

	return FALSE;
}

static gboolean del_off_cond(gpointer data)
{
	Node *tmp = NULL;
	LOGINFO("delete prohibit off condition by timeout\n");

	tmp = find_node(S_LCDOFF, (pid_t) data);
	del_node(S_LCDOFF, tmp);

	if (timeout_src_id == 0)
		states[cur_state].trans(EVENT_TIMEOUT);

	return FALSE;
}

static gboolean del_sleep_cond(gpointer data)
{
	Node *tmp = NULL;
	LOGINFO("delete prohibit sleep condition by timeout\n");

	tmp = find_node(S_SLEEP, (pid_t) data);
	del_node(S_SLEEP, tmp);

	if (timeout_src_id == 0)
		states[cur_state].trans(EVENT_TIMEOUT);

	sysman_inform_inactive((pid_t) data);
	return FALSE;
}

/* update transition condition for application requrements */
static int proc_condition(PMMsg *data)
{
	Node *tmp = NULL;
	unsigned int val = data->cond;
	pid_t pid = data->pid;
	int cond_timeout_id = -1;
	gboolean holdkey_block = 0;

	if (val == 0)
		return 0;
	/* for debug */
	char pname[PATH_MAX];
	char buf[PATH_MAX];
	int fd_cmdline;
	snprintf(buf, PATH_MAX, "/proc/%d/cmdline", pid);
	fd_cmdline = open(buf, O_RDONLY);
	if (fd_cmdline < 0) {
		snprintf(pname, PATH_MAX,
				"does not exist now(may be dead without unlock)");
	} else {
		read(fd_cmdline, pname, PATH_MAX);
		close(fd_cmdline);
	}

	if (val & MASK_DIM) {
		if (data->timeout > 0) {
			cond_timeout_id =
				g_timeout_add_full(G_PRIORITY_DEFAULT,
						data->timeout,
						(GSourceFunc) del_dim_cond,
						(gpointer) pid, NULL);
		}
		holdkey_block = GET_HOLDKEY_BLOCK_STATE(val);
		tmp = find_node(S_LCDDIM, pid);
		if (tmp == NULL)
			tmp = add_node(S_LCDDIM, pid, cond_timeout_id, holdkey_block);
		else if (tmp->timeout_id > 0) {
			g_source_remove(tmp->timeout_id);
			tmp->timeout_id = cond_timeout_id;
			tmp->holdkey_block = holdkey_block;
		}
		/* for debug */
		LOGINFO("[%s] locked by pid %d - process %s\n", "S_NORMAL", pid,
				pname);
	}
	if (val & MASK_OFF) {
		if (data->timeout > 0) {
			cond_timeout_id =
				g_timeout_add_full(G_PRIORITY_DEFAULT,
						data->timeout,
						(GSourceFunc) del_off_cond,
						(gpointer) pid, NULL);
		}
		holdkey_block = GET_HOLDKEY_BLOCK_STATE(val);
		tmp = find_node(S_LCDOFF, pid);
		if (tmp == NULL)
			tmp = add_node(S_LCDOFF, pid, cond_timeout_id, holdkey_block);
		else if (tmp->timeout_id > 0) {
			g_source_remove(tmp->timeout_id);
			tmp->timeout_id = cond_timeout_id;
			tmp->holdkey_block = holdkey_block;
		}
		/* for debug */
		LOGINFO("[%s] locked by pid %d - process %s\n", "S_LCDDIM", pid,
				pname);
	}
	if (val & MASK_SLP) {
		if (data->timeout > 0) {
			cond_timeout_id =
				g_timeout_add_full(G_PRIORITY_DEFAULT,
						data->timeout,
						(GSourceFunc) del_sleep_cond,
						(gpointer) pid, NULL);
		}
		tmp = find_node(S_SLEEP, pid);
		if (tmp == NULL)
			tmp = add_node(S_SLEEP, pid, cond_timeout_id, 0);
		else if (tmp->timeout_id > 0) {
			g_source_remove(tmp->timeout_id);
			tmp->timeout_id = cond_timeout_id;
			tmp->holdkey_block = 0;
		}
		sysman_inform_active(pid);
		/* for debug */
		LOGINFO("[%s] locked by pid %d - process %s\n", "S_LCDOFF", pid,
				pname);
	}

	/* UNLOCK(GRANT) condition processing */
	val = val >> SHIFT_UNLOCK;
	if (val & MASK_DIM) {
		tmp = find_node(S_LCDDIM, pid);
		del_node(S_LCDDIM, tmp);
		LOGINFO("[%s] unlocked by pid %d - process %s\n", "S_LORMAL",
				pid, pname);
	}
	if (val & MASK_OFF) {
		tmp = find_node(S_LCDOFF, pid);
		del_node(S_LCDOFF, tmp);
		LOGINFO("[%s] unlocked by pid %d - process %s\n", "S_LCDDIM",
				pid, pname);
	}
	if (val & MASK_SLP) {
		tmp = find_node(S_SLEEP, pid);
		del_node(S_SLEEP, tmp);
		sysman_inform_inactive(pid);
		LOGINFO("[%s] unlocked by pid %d - process %s\n", "S_LCDOFF",
				pid, pname);
	}
	val = val >> 8;
	if (val != 0) {
		if ((val & 0x1)) {
			reset_timeout(states[cur_state].timeout);
			LOGINFO("reset timeout\n", "S_LCDOFF", pid, pname);
		}
	} else {
		/* guard time for suspend */
		if (cur_state == S_LCDOFF) {
			reset_timeout(5);
			LOGINFO("margin timeout (5 seconds)\n");
		}
	}

	if (timeout_src_id == 0)
		states[cur_state].trans(EVENT_TIMEOUT);

	return 0;
}

static int proc_change_state(unsigned int cond)
{
	int next_state = 0;
	struct state *st;
	int i;

	for (i = S_NORMAL; i < S_END; i++) {
		if ((cond >> (SHIFT_CHANGE_STATE + i)) & 0x1) {
			next_state = i;
			break;
		}
	}
	LOGINFO("Change State to %s", state_string[next_state]);

	switch (next_state) {
		case S_NORMAL:
		case S_LCDDIM:
		case S_LCDOFF:
			/* state transition */
			old_state = cur_state;
			cur_state = next_state;
			st = &states[cur_state];

			/* enter action */
			if (st->action) {
				st->action(st->timeout);
			}
			break;
		case S_SLEEP:
			LOGINFO("Dangerous requests.");
			/* at first LCD_OFF and then goto sleep */
			/* state transition */
			old_state = cur_state;
			cur_state = S_LCDOFF;
			st = &states[cur_state];
			if (st->action) {
				st->action(0);
			}
			old_state = cur_state;
			cur_state = S_SLEEP;
			st = &states[cur_state];
			if (st->action) {
				st->action(0);
			}
			break;

		default:
			return -1;
	}

	return 0;
}

/* If some changed, return 1 */
int check_processes(enum state_t prohibit_state)
{
	Node *t = cond_head[prohibit_state];
	Node *tmp = NULL;
	int ret = 0;

	while (t != NULL) {
		if (kill(t->pid, 0) == -1) {
			LOGERR
				("%d process does not exist, delete the REQ - prohibit state %d ",
				 t->pid, prohibit_state);
			tmp = t;
			ret = 1;
		}
		t = t->next;

		if (tmp != NULL) {
			del_node(prohibit_state, tmp);
			tmp = NULL;
		}
	}

	return ret;
}

int check_holdkey_block(enum state_t state)
{
	Node *t = cond_head[state];
	int ret = 0;

	LOGINFO("check holdkey block : state of %s", state_string[state]);

	while(t != NULL) {
		if(t->holdkey_block == true) {
			ret = 1;
			LOGINFO("Hold key blocked by pid(%d)!", t->pid);
			break;
		}
		t = t->next;
	}

	return ret;
}

int delete_condition(enum state_t state)
{
	Node *t = cond_head[state];
	int ret = 0;
	Node *tmp = NULL;

	LOGINFO("delete condition : state of %s", state_string[state]);

	while(t != NULL) {
		if(t->timeout_id > 0) {
			g_source_remove(t->timeout_id);
		}
		tmp = t;
		t = t->next;
		LOGINFO("delete node of pid(%d)", tmp->pid);
		del_node(state, tmp);
	}

	return 0;
}

/* SIGINT, SIGTERM, SIGQUIT signal handler */
static void sig_quit(int signo)
{
	LOGINFO("received %d signal : stops a main loop", signo);
	if (mainloop)
		g_main_loop_quit(mainloop);
}

void print_info(int fd)
{
	int s_index = 0;
	char buf[255];
	int i = 1, ret;

	if (fd < 0)
		return;

	snprintf(buf, sizeof(buf), 
			"\n======================================================================\n");
	write(fd, buf, strlen(buf));
	snprintf(buf, sizeof(buf),"Timeout Info: Run[%d] Dim[%d] Off[%d]\n",
			states[S_NORMAL].timeout,
			states[S_LCDDIM].timeout, states[S_LCDOFF].timeout);
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "Tran. Locked : %s %s %s\n",
			(trans_condition & MASK_DIM) ? state_string[S_NORMAL] : "-",
			(trans_condition & MASK_OFF) ? state_string[S_LCDDIM] : "-",
			(trans_condition & MASK_SLP) ? state_string[S_LCDOFF] : "-");
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "Current State: %s\n", state_string[cur_state]);
	write(fd, buf, strlen(buf));

	snprintf(buf, sizeof(buf), "Current Lock Conditions: \n");
	write(fd, buf, strlen(buf));

	for (s_index = S_NORMAL; s_index < S_END; s_index++) {
		Node *t;
		char pname[PATH_MAX];
		int fd_cmdline;
		t = cond_head[s_index];

		while (t != NULL) {
			snprintf(buf, sizeof(buf), "/proc/%d/cmdline", t->pid);
			fd_cmdline = open(buf, O_RDONLY);
			if (fd_cmdline < 0) {
				snprintf(pname, PATH_MAX,
						"does not exist now(may be dead without unlock)");
			} else {
				read(fd_cmdline, pname, PATH_MAX);
				close(fd_cmdline);
			}
			snprintf(buf, sizeof(buf),
					" %d: [%s] locked by pid %d - process %s\n",
					i++, state_string[s_index - 1], t->pid, pname);
			write(fd, buf, strlen(buf));
			t = t->next;
		}
	}
}

/* SIGHUP signal handler 
 * For debug... print info to syslog
 */
static void sig_hup(int signo)
{
	int fd;
	char buf[255];
	time_t now_time;

	time(&now_time);

	fd = open(PM_STATE_LOG_FILE, O_CREAT | O_WRONLY, 0644);
	if (fd != -1) {
		snprintf(buf, sizeof(buf), "\npm_state_log now-time : %d (s)\n\n",
				(int)now_time);
		write(fd, buf, strlen(buf));

		snprintf(buf, sizeof(buf), "status_flag: %x\n", status_flag);
		write(fd, buf, strlen(buf));

		snprintf(buf, sizeof(buf), "received sleep cmd count : %d\n",
				received_sleep_cmd);
		write(fd, buf, strlen(buf));

		print_info(fd);
		close(fd);
	}

	fd = open("/dev/console", O_WRONLY);
	if (fd != -1) {
		print_info(fd);
		close(fd);
	}
}

/* timeout handler  */
gboolean timeout_handler(gpointer data)
{
	LOGINFO("Time out state %s\n", state_string[cur_state]);

	if (timeout_src_id != 0) {
		g_source_remove(timeout_src_id);
		timeout_src_id = 0;
	}

	if ((status_flag & VCALL_FLAG)
			&& (cur_state == S_LCDOFF || cur_state == S_SLEEP)) {
		status_flag &= ~(VCALL_FLAG);
		reset_timeout(TELEPHONY_SIGNAL_TIMEOUT);
		return FALSE;
	}
	states[cur_state].trans(EVENT_TIMEOUT);
	return FALSE;
}

static void reset_timeout(int timeout)
{
	if (timeout_src_id != 0) {
		g_source_remove(timeout_src_id);
		timeout_src_id = 0;
	}
	if (timeout > 0) {
		timeout_src_id = g_timeout_add_seconds_full(G_PRIORITY_HIGH,
				timeout,
				(GSourceFunc)
				timeout_handler,
				NULL, NULL);
	}
}

static void sig_usr(int signo)
{
	status_flag |= VCALL_FLAG;
}

/* 
 * default transition function
 *   1. call check 
 *   2. transition 
 *   3. call enter action function
 */
static int default_trans(int evt)
{
	struct state *st = &states[cur_state];
	int next_state;

	if(cur_state == S_NORMAL && st->timeout == 0) {
		LOGINFO("LCD always on enabled!");
		return 0;
	}

	next_state = (enum state_t)trans_table[cur_state][evt];

	/* check conditions */
	while (st->check && !st->check(next_state)) {
		/* There is a condition. */
		LOGINFO("%s -> %s : check fail", state_string[cur_state],
		       state_string[next_state]);
		if (!check_processes(next_state)) {
			/* this is valid condition - the application that sent the condition is running now. */
			return -1;
		}
	}

	/* state transition */
	old_state = cur_state;
	cur_state = next_state;
	st = &states[cur_state];

	/* enter action */
	if (st->action) {
		st->action(st->timeout);
	}

	return 0;
}

/* default enter action function */
static int default_action(int timeout)
{
	int ret;
	int wakeup_count = -1;
	char buf[NAME_MAX];
	char *pkgname = NULL;
	int i = 0;
	int lock_state = -1;

	if (cur_state != old_state && cur_state != S_SLEEP)
		set_setting_pmstate(cur_state);

	switch (cur_state) {
		case S_NORMAL:
			/* normal state : backlight on and restore the previous brightness */
			if (old_state == S_LCDOFF || old_state == S_SLEEP) {
				for (i = 0; i < 10; i++) {
				vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
				LOGERR("Idle lock check : %d, vonf : %d", i, lock_state);
					if (lock_state)
						break;
					usleep(50000);
				}
				backlight_on();
				backlight_restore();
			} else if (old_state == S_LCDDIM)
				backlight_restore();
			break;

		case S_LCDDIM:
			if (old_state == S_LCDOFF || old_state == S_SLEEP) {
				backlight_on();
			}
			/* lcd dim state : dim the brightness */
			backlight_dim();
			break;

		case S_LCDOFF:
			if (old_state != S_SLEEP && old_state != S_LCDOFF) {
				/* lcd off state : turn off the backlight */
				backlight_off();
			}

			break;

		case S_SLEEP:
			/* sleep state : set system mode to SUSPEND */
			if (0 > plugin_intf->OEM_sys_get_power_wakeup_count(&wakeup_count)) 
				LOGERR("wakeup count read error");

			if (wakeup_count < 0) {
				LOGINFO("Wakup Event! Can not enter suspend mode.");
				goto go_lcd_off;
			}

			if (0 > plugin_intf->OEM_sys_set_power_wakeup_count(wakeup_count)) {
				LOGERR("wakeup count write error");
				goto go_lcd_off;
			}

			goto go_suspend;
	}

	/* set timer with current state timeout */
	reset_timeout(timeout);
	LOGINFO("timout set: %s state %d sec", state_string[cur_state], timeout);

	return 0;

go_suspend:
	system_suspend();
	LOGINFO("system wakeup!!");
	heynoti_publish(PM_WAKEUP_NOTI_NAME);
	/* Resume !! */
	if (check_wakeup_src() == EVENT_DEVICE)
		/* system waked up by devices */
		states[cur_state].trans(EVENT_DEVICE);
	else
		/* system waked up by user input */
		states[cur_state].trans(EVENT_INPUT);
	return 0;

go_lcd_off:
	heynoti_publish(PM_WAKEUP_NOTI_NAME);
	/* Resume !! */
	states[cur_state].trans(EVENT_DEVICE);
	return 0;
}

/* 
 * default check function
 *   return 
 *    0 : can't transit, others : transitable
 */
static int default_check(int next)
{
	int trans_cond = trans_condition & MASK_BIT;
	int lock_state = -1;

	LOGINFO("trans_cond : %x", trans_cond);

	vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
	if(lock_state==VCONFKEY_IDLE_LOCK && next != S_SLEEP) {
		LOGINFO("default_check : LOCK STATE, it's transitable");
		return 1;
	}

	switch (next) {
		case S_LCDDIM:
			trans_cond = trans_cond & MASK_DIM;
			break;
		case S_LCDOFF:
			trans_cond = trans_cond & MASK_OFF;
			break;
		case S_SLEEP:
			trans_cond = trans_cond & MASK_SLP;
			break;
		default:		/* S_NORMAL is exceptional */
			trans_cond = 0;
			break;
	}

	if (trans_cond != 0)
		return 0;

	return 1;		/* transitable */
}

/* get configurations from setting */
static int get_settings()
{
	int i;
	int val = 0;
	int ret = -1;
	char buf[255];

	for (i = 0; i < S_END; i++) {
		switch (states[i].state) {
			case S_NORMAL:
				ret = get_run_timeout(&val);
				if (ret != 0) {
					ret = get_env("PM_TO_NORMAL", buf, 255);
					val = atoi(buf);
				}
				break;
			case S_LCDDIM:
				ret = get_dim_timeout(&val);
				break;
			case S_LCDOFF:
				ret = get_off_timeout(&val);
				break;
			default:
				/* This state doesn't need to set time out. */
				ret = -1;
				break;
		}
		if (ret == 0 || val < 0) {
			states[i].timeout = val;
		} else {
			switch (states[i].state) {
				case S_NORMAL:
					states[i].timeout = DEFAULT_NORMAL_TIMEOUT;
					break;
				case S_LCDDIM:
					states[i].timeout = DEFAULT_DIM_TIMEOUT;
					break;
				case S_LCDOFF:
					states[i].timeout = DEFAULT_OFF_TIMEOUT;
					break;
				default:
					states[i].timeout = 0;
					break;
			}
		}
		LOGINFO("%s state : %d timeout", state_string[i], states[i].timeout);
	}

	return 0;
}

static void default_saving_mode(int onoff)
{
	if (onoff) {
		status_flag |= PWRSV_FLAG;
	} else {
		status_flag &= ~PWRSV_FLAG;
	}
	if (cur_state == S_NORMAL)
		backlight_restore();
}

static int poll_callback(int condition, PMMsg *data)
{
	time_t now;

	if (condition == INPUT_POLL_EVENT) {
		if (cur_state == S_LCDOFF || cur_state == S_SLEEP)
			LOGINFO("Power key input");
		time(&now);
		if (last_t != now) {
			states[cur_state].trans(EVENT_INPUT);
			last_t = now;
		}
	} else if (condition == PM_CONTROL_EVENT) {
		LOGINFO("process pid(%d) pm_control condition : %x ", data->pid,
				data->cond);

		if (data->cond & MASK_BIT
				|| (data->cond >> SHIFT_UNLOCK) & MASK_BIT)
			proc_condition(data);

		if (data->cond & CHANGE_STATE_BIT) {

			LOGINFO("Change state by pid(%d) request.", data->pid);
			proc_change_state(data->cond);
		}
	}

	return 0;
}

static int update_setting(int key_idx, int val)
{
	char buf[PATH_MAX];
	int ret = -1;
	int dim_timeout = -1;
	int run_timeout = -1;
	int power_saving_stat = -1;
	int power_saving_display_stat = -1;

	switch (key_idx) {
	case SETTING_TO_NORMAL:
		ret = get_dim_timeout(&dim_timeout);
		if(ret < 0 || dim_timeout < 0) {
			LOGERR("Can not get dim timeout. set default 5 seconds");
			dim_timeout = 5;
		}
		if(val < 0) {
			LOGERR("LCD timeout is wrong, set default 15 seconds");
			val = 15;
		}
		if(val == 0) {
			states[S_NORMAL].timeout = 0;
		} else if(val > dim_timeout) {
			states[S_NORMAL].timeout = val - dim_timeout;
		} else {
			states[S_NORMAL].timeout = 1;
		}
		states[cur_state].trans(EVENT_INPUT);
		break;
	case SETTING_LOW_BATT:
		if (val < VCONFKEY_SYSMAN_BAT_WARNING_LOW) {
			if (!(status_flag & CHRGR_FLAG))
				power_saving_func(true);
			status_flag |= LOWBT_FLAG;
		} else {
			if (status_flag & PWRSV_FLAG)
				power_saving_func(false);
			status_flag &= ~LOWBT_FLAG;
		}
		break;
	case SETTING_CHARGING:
		if (val) {
			if (status_flag & LOWBT_FLAG) {
				power_saving_func(false);
				status_flag &= ~LOWBT_FLAG;
			}
			status_flag |= CHRGR_FLAG;
		} else {
			int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
			vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state);
			if(bat_state < VCONFKEY_SYSMAN_BAT_NORMAL) {
				power_saving_func(true);
				status_flag |= LOWBT_FLAG;
			}
			status_flag &= ~CHRGR_FLAG;
		}
		break;
	case SETTING_BRT_LEVEL:
		if (status_flag & PWRSV_FLAG)
			   break;
		set_default_brt(val);
		snprintf(buf, sizeof(buf), "%s %d", SET_BRIGHTNESS_IN_BOOTLOADER, val);
		LOGINFO("Brightness set in bl : %d",val);
		system(buf);
		break;
	case SETTING_LOCK_SCREEN:
		if (val == VCONFKEY_IDLE_LOCK) {
			states[S_NORMAL].timeout = LOCK_SCREEN_TIMEOUT;
			LOGERR("LCD NORMAL timeout is set by %d seconds for lock screen", LOCK_SCREEN_TIMEOUT);
		} else {
			get_run_timeout(&run_timeout);
			if(run_timeout < 0) {
				LOGERR("Can not get run timeout. set default 15 seconds(includes dim 5 seconds)");
				run_timeout = 10;
			}
			states[S_NORMAL].timeout = run_timeout;
			LOGINFO("LCD NORMAL timeout is set by %d seconds because phone is unlocked", run_timeout);
		}
		if (cur_state == S_NORMAL) {
			states[cur_state].trans(EVENT_INPUT);
		}
		break;
	case SETTING_POWER_SAVING:
		if (val == 1)
			vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_CUSTMODE_DISPLAY, &power_saving_display_stat);
		if (power_saving_display_stat != 1)
			power_saving_display_stat = 0;
		plugin_intf->OEM_sys_set_display_frame_rate(power_saving_display_stat);
		backlight_restore();
		break;
	case SETTING_POWER_SAVING_DISPLAY:
		vconf_get_bool(VCONFKEY_SETAPPL_PWRSV_SYSMODE_STATUS, &power_saving_stat);
		if (power_saving_stat == 1) {
			if (val == 1)
				power_saving_display_stat = 1;
			else
				power_saving_display_stat = 0;
			plugin_intf->OEM_sys_set_display_frame_rate(power_saving_display_stat);
			backlight_restore();
		}
		break;
	default:
		return -1;
	}
	return 0;
}

static void check_seed_status(void)
{
	int ret = -1;
	int tmp = 0;
	int bat_state = VCONFKEY_SYSMAN_BAT_NORMAL;
	int max_brt = 0;
	int brt = 0;
	int lock_state = -1;

	/* Charging check */
	if ((get_charging_status(&tmp) == 0) && (tmp > 0)) {
		status_flag |= CHRGR_FLAG;
	}

	ret = get_setting_brightness(&tmp);
	if (ret != 0 || tmp < 0) {
		LOGINFO("fail to read vconf value for brightness");

		if (0 > plugin_intf->OEM_sys_get_backlight_max_brightness(DEFAULT_DISPLAY, &max_brt))
			brt = 7;
		else
			brt = max_brt * 0.4;
		if(tmp < 0)
			vconf_set_int(VCONFKEY_SETAPPL_LCD_BRIGHTNESS, brt);
		tmp = brt;
	}

	vconf_get_int(VCONFKEY_SYSMAN_BATTERY_STATUS_LOW, &bat_state);
	if(bat_state >= VCONFKEY_SYSMAN_BAT_WARNING_LOW || bat_state == -1 ) {
		LOGINFO("Set brightness from Setting App. %d", tmp);
		set_default_brt(tmp);
	} else {
		if (!(status_flag & CHRGR_FLAG)) {
			power_saving_func(true);
			status_flag |= LOWBT_FLAG;
		}
	}
	backlight_restore();

	/* USB connection check
	 * If connected, add sleep prohibit condition */
	if ((get_usb_status(&tmp) == 0) && (tmp > 0)) {
		tmp = readpid(USB_CON_PIDFILE);
		if (tmp != -1) {
			add_node(S_SLEEP, tmp, -1, 0);
		}
	}

	/* lock screen check */
	ret = vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &lock_state);
	if(lock_state == VCONFKEY_IDLE_LOCK) {
		states[S_NORMAL].timeout = LOCK_SCREEN_TIMEOUT;
		LOGERR("LCD NORMAL timeout is set by %d seconds for lock screen", LOCK_SCREEN_TIMEOUT);
	}

	return;
}

enum {
	INIT_SETTING = 0,
	INIT_INTERFACE,
	INIT_POLL,
	INIT_FIFO,
	INIT_END
};

static char *errMSG[INIT_END] = {
	[INIT_SETTING] = "setting init error",
	[INIT_INTERFACE] = "lowlevel interface(sysfs or others) init error",
	[INIT_POLL] = "input devices poll init error",
	[INIT_FIFO] = "FIFO poll init error",
};

/* logging indev_list for debug */
void printglist()
{
        int i;
        guint total=0;
        indev *tmp;

        total=g_list_length(indev_list);
        LOGINFO("***** total list : %d *****",total);
        for(i=0;i<total;i++) {
                tmp=(indev*)(g_list_nth(indev_list, i)->data);
                LOGINFO("* %d | path:%s, gsource:%d, gfd:%d", i, tmp->dev_path, tmp->dev_src, tmp->dev_fd);
                if(i==total-1 && g_list_nth(indev_list, i)->next==NULL)
                        LOGINFO("okokok");
        }
        LOGINFO("***************************\n");
}

static GList *find_glist(GList *glist, char *path)
{
	int i;
	guint total=0;
	indev *tmp;

	total=g_list_length(indev_list);
	for(i=0;i<total;i++) {
		tmp=(indev*)(g_list_nth(indev_list, i)->data);
		if(!strcmp(tmp->dev_path, path)){
			LOGINFO("nth : %d, path:%s, gsource:%d, gfd:%d", i, tmp->dev_path, tmp->dev_src, tmp->dev_fd);
			return g_list_nth(indev_list, i);
		}
	}
	return NULL;
}

static void input_cb(void* data)
{
	FILE *fp;
	char input_act[NAME_MAX], input_path[MAX_INPUT];
	char args[NAME_MAX + MAX_INPUT];
	int i, ret = -1;
	GList *glist = NULL;

	fp = fopen((char *) data, "r");
	if (fp == NULL) {
		LOGERR("input file open fail");
		return ;
	}

	while (fgets(args, NAME_MAX + MAX_INPUT, fp) != NULL){
		if( args[strlen(args)-1] != '\n'){
			LOGERR("input file must be terminated with new line character\n");
			break;
		}
		args[strlen(args) - 1] = '\0';
		for(i = 0; i< NAME_MAX + MAX_INPUT; i++){
			if(args[i] == ' '){
				if( i >= NAME_MAX ){
					LOGERR("bsfile name is over the name_max(255)\n");
					break;
				}
				strncpy(input_act, args, i < NAME_MAX ? i : NAME_MAX);
				input_act[i]='\0';
				strncpy(input_path, args + i + 1, MAX_INPUT);
				input_path[MAX_INPUT - 1] = '\0';

				if( !strcmp("add", input_act) ){
					LOGINFO("add input path:%s",input_path);

					ret=init_pm_poll_input(poll_callback, input_path);

				} else if ( !strcmp("remove", input_act) ){
					glist=find_glist(indev_list, input_path);
					if(glist != NULL){
						LOGINFO("remove input dev");
						g_source_remove_poll(((indev*)(glist->data))->dev_src, ((indev*)(glist->data))->dev_fd);
						g_source_destroy(((indev*)(glist->data))->dev_src);
						close(((indev*)(glist->data))->dev_fd->fd);
						g_free(((indev*)(glist->data))->dev_fd);
						free(((indev*)(glist->data))->dev_path);
						indev_list=g_list_remove(indev_list, glist->data);
					}
					ret=0;
				}
				/* debug */
				printglist();

				break;
			}
		}
		if(ret<0)
			break;
	}
	fclose(fp);

	if ( ret != -1) {
		fp = fopen((char *) data, "w");
		if (fp == NULL) {
			return ;
		}
		fclose(fp);
	}

	return ;
}

static int set_noti(int noti_fd)
{
	int fd;
	char buf[PATH_MAX];

	noti_fd = heynoti_init();
	if (noti_fd < 0) {
		LOGERR("heynoti_init error");
		return -1;
	}

	if (heynoti_subscribe(noti_fd, PM_EVENT_NOTI_NAME, input_cb, PM_EVENT_NOTI_PATH) < 0) {
		LOGERR("input file change noti add failed(%s). %s", buf, strerror(errno));
		return -1;
	} else {
		LOGERR("input file change noti add ok");
	}

	if (heynoti_attach_handler(noti_fd) < 0) {
		LOGERR("heynoti_attach_handler error");
		return -1;
	}

	return 0;
}

static int unset_noti(int noti_fd)
{
	if (noti_fd < 0) {
		LOGINFO("set noti already failed. nothing to do in unset");
		return 0;
	}

	if (heynoti_unsubscribe(noti_fd, PM_EVENT_NOTI_NAME, input_cb) < 0 || heynoti_detach_handler(noti_fd) < 0) {
		LOGERR("heynoti unsubsribe or detach error");
		return -1;
	}
	return 0;
}

/**
 * Power manager Main loop
 *
 * @internal
 * @param[in] flags If the first bit of this is set, start managing without Start notification.
 * 					If the second bit of ths is set, use unified device manager functions.
 */
void start_main(unsigned int flags)
{
	int ret, i;

	if (0 > _pm_devman_plugin_init()) {
		LOGERR("Device Manager Plugin initialize failed");
		exit (-1);
	}

	LOGINFO("Start power manager daemon");

	signal(SIGINT, sig_quit);
	signal(SIGTERM, sig_quit);
	signal(SIGQUIT, sig_quit);
	signal(SIGHUP, sig_hup);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGUSR1, sig_usr);

	mainloop = g_main_loop_new(NULL, FALSE);
	power_saving_func = default_saving_mode;

	/* noti init for new input device like bt mouse */
	int noti_fd;
	indev_list=NULL;
	set_noti(noti_fd);

	for (i = INIT_SETTING; i < INIT_END; i++) {
		switch (i) {
			case INIT_SETTING:
				ret = init_setting(update_setting);
				break;
			case INIT_INTERFACE:
				get_settings();
				ret = init_sysfs(flags);
				break;
			case INIT_POLL:
				LOGINFO("poll init");
				ret = init_pm_poll(poll_callback);
				break;
		}
		if (ret != 0) {
			LOGERR(errMSG[i]);
			break;
		}
	}

	if (i == INIT_END) {
		check_seed_status();

		if (pm_init_extention != NULL)
			pm_init_extention(NULL);

		if (flags & WITHOUT_STARTNOTI) {	/* start without noti */
			LOGINFO("Start Power managing without noti");
			cur_state = S_NORMAL;
			set_setting_pmstate(cur_state);
			reset_timeout(states[S_NORMAL].timeout);
		}

		g_main_loop_run(mainloop);
		g_main_loop_unref(mainloop);
	}

	for (i = i - 1; i >= INIT_SETTING; i--) {
		switch (i) {
			case INIT_SETTING:
				exit_setting();
				break;
			case INIT_INTERFACE:
				exit_sysfs();
				break;
			case INIT_POLL:
				unset_noti(noti_fd);
				exit_pm_poll();
				break;
		}
	}

	if (pm_exit_extention != NULL)
		pm_exit_extention();

	LOGINFO("Terminate power manager daemon");
}

/**
 * @}
 */
