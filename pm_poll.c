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
 * @file	pm_poll.c
 * @version	0.2
 * @brief	 Power Manager poll implementation (input devices & a domain socket file)
 *
 * This file includes the input device poll implementation.
 * Default input devices are /dev/event0 and /dev/event1
 * User can use "PM_INPUT" for setting another input device poll in an environment file (/etc/profile). 
 * (ex: PM_INPUT=/dev/event0:/dev/event1:/dev/event5 )
 */

#include <glib.h>
#include <stdio.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "util.h"
#include "pm_core.h"
#include "pm_poll.h"

#define DEV_PATH_DLM	":"

PMMsg recv_data;
int (*g_pm_callback) (int, PMMsg *);

#ifdef ENABLE_KEY_FILTER
extern int check_key_filter(int length, char buf[]);
#  define CHECK_KEY_FILTER(a, b) do {\
							if (check_key_filter(a, b) != 0)\
								return TRUE;\
							} while (0);

#else
#  define CHECK_KEY_FILTER(a, b)
#endif

#define DEFAULT_DEV_PATH "/dev/event1:/dev/event0"

static GSource *src;
static GSourceFuncs *funcs;
static int sockfd;

static gboolean pm_check(GSource *src)
{
	GSList *fd_list;
	GPollFD *tmp;

	fd_list = src->poll_fds;
	do {
		tmp = (GPollFD *) fd_list->data;
		if ((tmp->revents & (POLLIN | POLLPRI)))
			return TRUE;
		fd_list = fd_list->next;
	} while (fd_list);

	return FALSE;
}

static gboolean pm_dispatch(GSource *src, GSourceFunc callback, gpointer data)
{
	callback(data);
	return TRUE;
}

static gboolean pm_prepare(GSource *src, gint *timeout)
{
	return FALSE;
}

gboolean pm_handler(gpointer data)
{
	char buf[1024];
	struct sockaddr_un clientaddr;

	GPollFD *gpollfd = (GPollFD *) data;
	int ret;
	int clilen = sizeof(clientaddr);

	if (g_pm_callback == NULL) {
		return FALSE;
	}
	if (gpollfd->fd == sockfd) {
		ret =
		    recvfrom(gpollfd->fd, &recv_data, sizeof(recv_data), 0,
			     (struct sockaddr *)&clientaddr,
			     (socklen_t *)&clilen);
		(*g_pm_callback) (PM_CONTROL_EVENT, &recv_data);
	} else {
		ret = read(gpollfd->fd, buf, sizeof(buf));
		CHECK_KEY_FILTER(ret, buf);
		(*g_pm_callback) (INPUT_POLL_EVENT, NULL);
	}

	return TRUE;
}

static int init_sock(char *sock_path)
{
	struct sockaddr_un serveraddr;
	int fd;

	LOGINFO("initialize pm_socket for pm_control library");

	if (sock_path == NULL || strcmp(sock_path, SOCK_PATH)) {
		LOGERR("invalid sock_path= %s");
		return -1;
	}

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0) {
		LOGERR("socket error");
		return -1;
	}

	unlink(sock_path);

	bzero(&serveraddr, sizeof(serveraddr));
	serveraddr.sun_family = AF_UNIX;
	strncpy(serveraddr.sun_path, sock_path, sizeof(serveraddr.sun_path) - 1);

	if (bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
		LOGERR("bind error");
		return -1;
	}

	if (chmod(sock_path, (S_IRWXU | S_IRWXG | S_IRWXO)) < 0)	/* 0777 */
		LOGERR("failed to change the socket permission");

	if (!strcmp(sock_path, SOCK_PATH))
		sockfd = fd;

	LOGINFO("init sock() sueccess!");
	return fd;
}

int init_pm_poll(int (*pm_callback) (int, PMMsg *))
{

	guint ret;
	char *dev_paths, *path_tok, *pm_input_env, *save_ptr;
	int dev_paths_size;

	GPollFD *gpollfd;

	g_pm_callback = pm_callback;

	LOGINFO
	    ("initialize pm poll - input devices and domain socket(libpmapi)");

	pm_input_env = getenv("PM_INPUT");
	if ((pm_input_env != NULL) && (strlen(pm_input_env) < 1024)) {
		LOGINFO("Getting input device path from environment: %s",
		       pm_input_env);
		/* Add 2 bytes for following strncat() */
		dev_paths_size =  strlen(pm_input_env) + strlen(SOCK_PATH) + strlen(DEV_PATH_DLM) + 1;
		dev_paths = (char *)malloc(dev_paths_size);
		snprintf(dev_paths, dev_paths_size, "%s", pm_input_env);
	} else {
		/* Add 2 bytes for following strncat() */
		dev_paths_size = strlen(DEFAULT_DEV_PATH) + strlen(SOCK_PATH) + strlen(DEV_PATH_DLM) + 1;
		dev_paths = (char *)malloc(dev_paths_size);
		snprintf(dev_paths, dev_paths_size, "%s", DEFAULT_DEV_PATH);
	}

	/* add the UNIX domain socket file path */
	strncat(dev_paths, DEV_PATH_DLM, strlen(DEV_PATH_DLM));
	strncat(dev_paths, SOCK_PATH, strlen(SOCK_PATH));
	dev_paths[dev_paths_size - 1] = '\0';

	path_tok = strtok_r(dev_paths, DEV_PATH_DLM, &save_ptr);
	if (path_tok == NULL) {
		LOGERR("Device Path Tokeninzing Failed");
		free(dev_paths);
		return -1;
	}

	funcs = (GSourceFuncs *) g_malloc(sizeof(GSourceFuncs));
	funcs->prepare = pm_prepare;
	funcs->check = pm_check;
	funcs->dispatch = pm_dispatch;
	funcs->finalize = NULL;

	do {
		src = g_source_new(funcs, sizeof(GSource));

		gpollfd = (GPollFD *) g_malloc(sizeof(GPollFD));
		gpollfd->events = POLLIN;

		if (strcmp(path_tok, SOCK_PATH) == 0) {
			gpollfd->fd = init_sock(SOCK_PATH);
			LOGINFO("pm_poll domain socket file: %s, fd: %d",
			       path_tok, gpollfd->fd);
		} else {
			gpollfd->fd = open(path_tok, O_RDONLY);
			LOGINFO("pm_poll input device file: %s, fd: %d",
			       path_tok, gpollfd->fd);
		}

		if (gpollfd->fd == -1) {
			LOGERR("Cannot open the file: %s", path_tok);
			free(dev_paths);
			return -1;
		}
		g_source_add_poll(src, gpollfd);
		g_source_set_callback(src, (GSourceFunc) pm_handler,
				      (gpointer) gpollfd, NULL);

		g_source_set_priority(src, G_PRIORITY_LOW);
		ret = g_source_attach(src, NULL);
		if (ret == 0) {
			LOGERR("Failed g_source_attach() in init_pm_poll()");
			free(dev_paths);
			return -1;
		}
		g_source_unref(src);

	} while ((path_tok = strtok_r(NULL, DEV_PATH_DLM, &save_ptr)));

	free(dev_paths);
	return 0;
}

int exit_pm_poll()
{
	g_free(funcs);
	close(sockfd);
	unlink(SOCK_PATH);
	LOGINFO("pm_poll is finished");
	return 0;
}

int init_pm_poll_input(int (*pm_callback)(int , PMMsg * ), const char *path)
{
	guint ret;
	indev *adddev = NULL;

	g_pm_callback = pm_callback;

	GPollFD *gpollfd;
	const char *devpath;
	GSource *devsrc;

	LOGINFO("initialize pm poll for bt %s",path);
	adddev=(indev *)malloc(sizeof(indev));
	adddev->dev_fd = (GPollFD *)g_malloc(sizeof(GPollFD));
	(adddev->dev_fd)->events = POLLIN;
	(adddev->dev_fd)->fd = open(path, O_RDONLY);
	if((adddev->dev_fd)->fd == -1) {
		LOGERR("Cannot open the file for BT: %s",path);
		g_free(adddev->dev_fd);
		free(adddev);
		return -1;
	}
	adddev->dev_path = (char *)malloc(strlen(path) + 1);
	strncpy(adddev->dev_path, path, strlen(path) + 1);
	adddev->dev_src = g_source_new(funcs, sizeof(GSource));
	LOGINFO("pm_poll for BT input device file(path: %s, gsource: %d, gpollfd: %d", adddev->dev_path, adddev->dev_src, adddev->dev_fd);
	indev_list=g_list_append(indev_list, adddev);
	
	g_source_add_poll(adddev->dev_src, adddev->dev_fd);
	
	g_source_set_callback(adddev->dev_src, (GSourceFunc) pm_handler, (gpointer)adddev->dev_fd, NULL);
	

	g_source_set_priority(adddev->dev_src, G_PRIORITY_LOW );
	
	ret = g_source_attach(adddev->dev_src, NULL);
	if(ret == 0)
	{
		LOGERR("Failed g_source_attach() in init_pm_poll()");
		return -1;
	}
	g_source_unref(adddev->dev_src);
	return 0;
}
