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

#ifndef __PM_BATTERY_H_
#define __PM_BATTERY_H_

#define BATTERY_POLLING_PERIOD	30000 /* ms */

/*
* If system is suspend state for a long time,
* it's hard to expect power consumption.
* Therefore, Previous capacity value is ignored
* when system changes from suspend state to resume state.
* This flag indicates phone wakes up from suspend state.
* In this case, all nodes saved are ignored.
*/
extern int system_wakeup_flag;

int start_battinfo_gathering(int timeout);
void end_battinfo_gathering();

#endif
