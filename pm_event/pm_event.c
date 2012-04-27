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
#include <string.h>
#include <limits.h>

#define PM_EVENT_NOTI_PATH     "/opt/share/noti/pm_event"

int main(int argc, char *argv[])
{
	if(argc != 3) {
		return -1;
	}

	if(strlen(argv[1]) + strlen(argv[2]) > PATH_MAX) {
		return -1;
	}

	int i;
	char buf[PATH_MAX + 25];
	snprintf(buf, PATH_MAX + 25, "echo %s %s >> %s", argv[1], argv[2], PM_EVENT_NOTI_PATH);
	system(buf);

	return 1;
}
