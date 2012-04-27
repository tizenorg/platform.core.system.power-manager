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


/**
 * @file	pm_poll.h
 * @version	0.2
 * @brief	Power Manager input device poll implementation
 *
 * This file includes the input device poll implementation.
 * Default input devices are /dev/event0 and /dev/event1
 * User can use "PM_INPUT_DEV" for setting another input device poll in an environment file (/etc/profile). 
 * (ex: PM_INPUT_DEV=/dev/event0:/dev/event1:/dev/event5 )
 */

#ifndef __PM_POLL_H__
#define __PM_POLL_H__

#include<glib.h>

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

enum {
	INPUT_POLL_EVENT = -9,
	SIDEKEY_POLL_EVENT,
	PWRKEY_POLL_EVENT,
	PM_CONTROL_EVENT,
};

#define SOCK_PATH "/tmp/pm_sock"

typedef struct {
	pid_t pid;
	unsigned int cond;
	unsigned int timeout;
} PMMsg;

typedef struct {
	char *dev_path;
	GSource *dev_src;
	GPollFD *dev_fd;
} indev;

GList *indev_list;

PMMsg recv_data;
int (*g_pm_callback) (int, PMMsg *);

extern int init_pm_poll(int (*pm_callback) (int, PMMsg *));
extern int exit_pm_poll();
extern int init_pm_poll_input(int (*pm_callback)(int , PMMsg * ), const char *path);

/**
 * @}
 */

#endif				/*__PM_POLL_H__ */
