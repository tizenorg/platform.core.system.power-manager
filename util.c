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
 * @file	util.c
 * @version	0.1
 * @brief	Utilities for Power manager
 *
 * This file includes logging, daemonize
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#ifdef ENABLE_DLOG_OUT
#define LOG_TAG		"POWER_MANAGER"
#endif

#include "util.h"

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

/**
 * @brief logging function
 *
 * This is log wrapper
 *
 * @param[in] priority log pritority
 * @param[in] fmt format string
 */
void pm_log(int priority, char *fmt, ...)
{
	va_list ap;
	char buf[NAME_MAX];	/* NAME_MAX is 255 */
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
#ifdef ENABLE_DLOG_OUT
	switch (priority) {
	case DLOG_DEBUG:
		SLOGD("%s", buf);
		break;
	case DLOG_ERROR:
		SLOGE("%s", buf);
		break;
	case DLOG_INFO:
		SLOGI("%s", buf);
		break;
	default:
		SLOGV("%s", buf);
		break;
	}
#else
	syslog(priority, "%s", buf);
#endif
	printf("\x1b[1;33;44m[PowerManager] %s\x1b[0m\n\n", buf);
}

/**
 * @brief write the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return 0 (always)
 */
int writepid(char *pidpath)
{
	FILE *fp;

	fp = fopen(pidpath, "w");
	if (fp != NULL) {
		fprintf(fp, "%d", getpid());
		fclose(fp);
	}

	return 0;
}

/**
 * @brief read the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return  pid : success, -1 : failed
 */
int readpid(char *pidpath)
{
	FILE *fp;
	int ret = -1;

	fp = fopen(pidpath, "r");
	if (fp != NULL) {
		fscanf(fp, "%5d", &ret);
		fclose(fp);
	}

	return ret;
}

/** 
 * @brief daemonize function 
 *
 * fork the process, kill the parent process 
 * and replace all the standard fds to /dev/null.
 *
 * @return 0 : success, -1 : fork() error
 */
int daemonize(void)
{
	pid_t pid;

	pid = fork();
	if (pid < 0)
		return -1;
	else if (pid != 0)
		exit(0);

	setsid();
	chdir("/");

	close(0);
	close(1);
	close(2);

	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDWR);
	dup(1);

	return 0;
}

/** 
 * @brief  function to run a process
 *
 * fork the process, and run the other process if it is child. 
 *
 * @return new process pid on success, -1 on error
 */

int exec_process(char *name)
{
	int ret, pid;
	int i;

	if (name[0] == '\0')
		return 0;

	pid = fork();
	switch (pid) {
	case -1:
		LOGERR("Fork error");
		ret = -1;
		break;
	case 0:
		for (i = 0; i < _NSIG; i++)
			signal(i, SIG_DFL);
		execlp(name, name, NULL);
		LOGERR("execlp() error : %s\n", strerror(errno));
		exit(-1);
		break;
	default:
		ret = pid;
		break;
	}
	return ret;
}

char *get_pkgname(char *exepath)
{
	char *filename;
	char pkgname[NAME_MAX];

	filename = strrchr(exepath, '/');
	if (filename == NULL)
		filename = exepath;
	else
		filename = filename + 1;

	snprintf(pkgname, NAME_MAX, "deb.com.samsung.%s", filename);

	return strdup(pkgname);
}

/**
 * @}
 */
