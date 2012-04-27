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


#ifndef __PM_X_LCD_ONOFF_C__
#define __PM_X_LCD_ONOFF_C__

#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include "pm_device_plugin.h"

#define CMD_STANDBY	"standby"
#define CMD_OFF		"off"

static int pm_x_set_lcd_backlight(struct _PMSys *p, int onoff)
{
	pid_t pid;
	char cmd_line[32];
	int ret;

	LOGDBG("Backlight onoff=%d", onoff);
	if (onoff == STATUS_ON)
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_STANDBY);
	else
		snprintf(cmd_line, sizeof(cmd_line), "%s", CMD_OFF);

	signal(SIGCHLD, SIG_DFL);
	pid = vfork();

	if (pid < 0) {
		LOGERR("[1] Failed to fork child process for LCD On/Off");
		return -1;
	}

	if (pid == 0) {
		LOGINFO("[1] Child proccess for LCD %s was created (%s)",
			((onoff == STATUS_ON) ? "ON" : "OFF"), cmd_line);
		execl("/usr/bin/xset", "/usr/bin/xset", "dpms", "force",
		      cmd_line, NULL);
		_exit(0);
	} else if (pid != (ret = waitpid(pid, NULL, 0))) {
		LOGERR
		    ("[1] Waiting failed for the child process pid: %d, ret: %d, errno: %d",
		     pid, ret, errno);
	}

	signal(SIGCHLD, SIG_IGN);
	return 0;
}

#endif				/*__PM_X_LCD_ONOFF_C__ */
