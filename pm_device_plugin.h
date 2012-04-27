/*
 *  power-manager
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: DongGi Jang <dg0402.jang@samsung.com>
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


#ifndef __PM_DEVICE_PLUGIN_H__
#define __PM_DEVICE_PLUGIN_H__

#include "devman_plugin_intf.h"

#define DEVMAN_PLUGIN_PATH      "/usr/lib/libslp_devman_plugin.so"

int _pm_devman_plugin_init(void);
int _pm_devman_plugin_fini(void);

const OEM_sys_devman_plugin_interface *plugin_intf;

#endif  /* __PM_DEVICE_PLUGIN_H__ */
