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

#include <dlfcn.h>
#include <unistd.h>

#include "util.h"
#include "pm_device_plugin.h"

static void *dlopen_handle;

int _pm_devman_plugin_init()
{
	char *error;

	dlopen_handle = dlopen(DEVMAN_PLUGIN_PATH, RTLD_NOW);
	if (!dlopen_handle) {
		LOGERR("dlopen() failed");
		return -1;
	}

	const OEM_sys_devman_plugin_interface *(*get_devman_plugin_interface) ();
	get_devman_plugin_interface = dlsym(dlopen_handle, "OEM_sys_get_devman_plugin_interface");
	if ((error = dlerror()) != NULL) {
		LOGERR("dlsym() failed: %s", error);
		dlclose(dlopen_handle);
		return -1;
	}

	plugin_intf = get_devman_plugin_interface();
	if (!plugin_intf) {
		LOGERR("get_devman_plugin_interface() failed");
		dlclose(dlopen_handle);
		return -1;
	}

	return 0;
}


int _pm_devman_plugin_fini()
{
	if (dlopen_handle) {
		dlclose(dlopen_handle);
	}

	return 0;
}


