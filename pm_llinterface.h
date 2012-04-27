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
 * @file	pm_llinterface.h
 * @author	Suchang Woo (suchang.woo@samsung.com)
 * @version	0.1
 * @brief	Power manager low-level interface module header
 */
#ifndef __PM_LLINTERFACE_H__
#define __PM_LLINTERFACE_H__

#define FLAG_X_DPMS		0x2

#define DEFAULT_DISPLAY 0

/*
 * Event type enumeration
 */
enum {
	EVENT_TIMEOUT = 0,	/*< time out event from timer */
	EVENT_DEVICE = EVENT_TIMEOUT,	/*< wake up by devices except input devices */
	EVENT_INPUT,		/*< input event from noti service */
	EVENT_END,
};

extern int init_sysfs(unsigned int);
extern int exit_sysfs(void);

extern int system_suspend(void);

extern int backlight_on(void);
extern int backlight_off(void);

extern int backlight_dim(void);
extern int backlight_restore(void);

extern int set_default_brt(int level);

extern int check_wakeup_src(void);

#endif
