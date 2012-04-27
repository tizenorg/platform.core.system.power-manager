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


/*
 * @file	pm_setting.h
 * @version	0.1
 * @brief	Power manager setting module header
 */
#ifndef __PM_SETTING_H__
#define __PM_SETTING_H__

#include <vconf.h>

/*
 * @addtogroup POWER_MANAGER
 * @{
 */

enum {
	SETTING_BEGIN = 0,
	SETTING_TO_NORMAL = SETTING_BEGIN,
	SETTING_LOW_BATT,
	SETTING_CHARGING,
	SETTING_BRT_LEVEL,
	SETTING_LOCK_SCREEN,
	SETTING_GET_END,
	SETTING_PM_STATE = SETTING_GET_END,
	SETTING_END
};

extern int get_setting_brightness();

/*
 * @brief setting initialization function
 *
 * get the variables if it exists. otherwise, set the default.
 * and register some callback functions.
 *
 * @internal
 * @param[in] func configuration change callback function
 * @return 0 : success, -1 : error
 */
extern int init_setting(int (*func) (int key_idx, int val));

extern int exit_setting();

/*
 * get normal state timeout from SLP-setting SLP_SETTING_LCD_TIMEOUT_NORMAL
 *
 * @internal
 * @param[out] timeout timeout variable pointer
 * @return 0 : success, -1 : error
 */
extern int get_run_timeout(int *timeout);

/*
 * get LCD dim state timeout from environment variable.
 *
 * @internal
 * @param[out] timeout timeout variable pointer
 * @return 0 : success, -1 : error
 */
extern int get_dim_timeout(int *timeout);

/*
 * get LCD off state timeout from environment variable.
 *
 * @internal
 * @param[out] timeout timeout variable pointer
 * @return 0 : success, -1 : error
 */
extern int get_off_timeout(int *timeout);

/*
 * get USB connection status from SLP-setting SLP_SETTING_USB_STATUS
 *
 * @internal
 * @param[out] val usb connection status variable pointer, 0 is disconnected, others is connected.
 * @return 0 : success, -1 : error
 */
extern int get_usb_status(int *val);

/*
 * set Current power manager state at SLP-setting "memory/pwrmgr/state"
 *
 * @internal
 * @param[in] val current power manager state.
 * @return 0 : success, -1 : error
 */
extern int set_setting_pmstate(int val);

/*
 * get charging status at SLP-setting "memory/Battery/Charger"
 *
 * @internal
 * @param[in] val charging or not (1 or 0 respectively).
 * @return 0 : success, -1 : error
 */
extern int get_charging_status(int *val);

/*
 * get current battery low status at SLP-setting "memory/Battery/Status/Low"
 *
 * @internal
 * @param[in] val current low battery status
 * @return 0 : success, -1 : error
 */
extern int get_lowbatt_status(int *val);

/*
 * @}
 */

#endif
