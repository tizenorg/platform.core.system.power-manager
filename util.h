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
 * @file	util.h
 * @version	0.1
 * @brief	Utilities header for Power manager
 */
#ifndef __DEF_UTIL_H__
#define __DEF_UTIL_H__

/**
 * @addtogroup POWER_MANAGER
 * @{
 */

/*
 * @brief write the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return 0 (always)
 */
extern int writepid(char *pidpath);

/*
 * @brief read the pid
 *
 * get a pid and write it to pidpath
 *
 * @param[in] pidpath pid file path
 * @return  pid : success, -1 : failed
 */
extern int readpid(char *pidpath);

/*
 * @brief daemonize function 
 *
 * fork the process, kill the parent process 
 * and replace all the standard fds to /dev/null.
 *
 * @return 0 : success, -1 : fork() error
 */
extern int daemonize(void);

/**
 * @brief  function to run a process
 *
 * fork the process, and run the other process if it is child.
 *
 * @return new process pid on success, -1 on error
 */
extern int exec_process(char *name);

/**
 * @brief  function to get the pkg name for AUL (Application Util Library)
 *
 * remove the path of exepath and make the "com.samsung.<exe_file_name>" string.
 *
 * @return new process pid on success, -1 on error
 */
extern char *get_pkgname(char *exepath);

/*
 * @brief logging function
 *
 * This is log wrapper
 *
 * @param[in] priority log pritority
 * @param[in] fmt format string
 */
extern void pm_log(int priority, char *fmt, ...);

#if defined(ENABLE_DLOG_OUT)
#  include <dlog.h>
/*
 * @brief LOG_INFO wrapper
 */
#  define LOGINFO(fmt, arg...) pm_log(DLOG_INFO, fmt, ## arg)

/*
 * @brief LOG_ERR wrapper
 */
#  define LOGERR(fmt, arg...) pm_log(DLOG_ERROR, fmt, ## arg)
#else
#  include <syslog.h>
#  define LOGINFO(fmt, arg...) pm_log(LOG_INFO, fmt, ## arg)
#  define LOGERR(fmt, arg...) pm_log(LOG_ERR, fmt, ## arg)
#endif

/*
 * @brief LOG_DEBUG wrapper
 */
#if defined(DEBUG_PRINT) && defined(ENABLE_DLOG_OUT)
#  define LOGDBG(fmt, arg...) pm_log(DLOG_DEBUG, fmt, ## arg)
#elif defined(DEBUG_PRINT)
#  define LOGDBG(fmt, arg...) pm_log(LOG_DEBUG, fmt, ## arg)
#else
#  define LOGDBG(fmt, arg...) do { } while (0);
#endif

/**
 * @}
 */
#endif
