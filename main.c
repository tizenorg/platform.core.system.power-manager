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

/**
 * @file	main.c
 * @version	0.1
 * @brief	Power Manager main file
 *
 * Process the user options, Daemonize the process, Start Main loop
 */

/**
 * @addtogroup POWER_MANAGER
 * @{
 *
 * @section execution How to execute 
 *   # powermanager {options...} <br>
 *
 * Options: <br><br>
 *  &nbsp; -f --foreground<br>
 *  &nbsp; &nbsp; Run as foreground process<br> <br>
 *  &nbsp; -d --direct<br>
 *  &nbsp; &nbsp; Start without notification<br> <br>
 *
 * @}
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include "pm_core.h"

/**
 * Print the usage
 *
 * @internal
 */
void usage()
{
	printf("Options: \n");
	printf("  -f, --foreground         Run as foreground process\n");
	printf("  -d, --direct             Start without notification\n");
	printf
	    ("  -x, --xdpms              With LCD-onoff control by x-dpms \n");
	printf("\n");

	exit(0);
}

/**
 * Pid file path
 */
#define	DEFAULT_PID_PATH "/var/run/power-manager.pid"

int main(int argc, char *argv[])
{
	int c;
	int runflags = 0;	/* run as daemon */
	unsigned int flags = 0x0;	/* 0 : start with noti */

	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"foreground", no_argument, NULL, 'f'},
			{"direct", no_argument, NULL, 'd'},
			{"xdpms", no_argument, NULL, 'x'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "fdx", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			runflags = 1;
			break;

		case 'd':
			flags = flags | WITHOUT_STARTNOTI;	/* 0x1 : start without noti */
			break;

		case 'x':
			flags = flags | FLAG_X_DPMS;	/* 0x2 : X control LCD onoff */
			break;

		default:
			usage();
			break;
		}
	}

	if (optind < argc)
		usage();

	if (access(DEFAULT_PID_PATH, F_OK) == 0) {	/* pid file exist */
		printf
		    ("Check the PM is running. If it isn't, delete \"%s\" and retry.\n",
		     DEFAULT_PID_PATH);
		return -1;
	}

	if (!runflags)
		daemonize();

	writepid(DEFAULT_PID_PATH);

	/* this function is main loop, defined in pm_core.c */
	start_main(flags);

	unlink(DEFAULT_PID_PATH);
	return 0;
}
