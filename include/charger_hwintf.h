/*
 * Copyright (C) 2023 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __CHARGER_HWINTF_H
#define __CHARGER_HWINTF_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "charger_manager.h"
#include <nuttx/power/battery_charger.h>
#include <nuttx/power/battery_gauge.h>
#include <nuttx/power/battery_ioctl.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int enable_adapter(struct charger_manager* manager, bool enable);
int get_adapter_type(struct charger_manager* manager, int* type);
int set_supply_voltage(struct charger_manager* manager, int vol);
int get_supply_voltage(struct charger_manager* manager, int* vol);
int enable_charger(struct charger_manager* manager, int seq, bool enable);
int set_charger_voltage(struct charger_manager* manager, int seq, int vol);
int set_charger_current(struct charger_manager* manager, int seq, int current);
int get_charger_state(struct charger_manager* manager, int seq, unsigned int* state);
int get_adapter_type_by_charger(struct charger_manager* manager, int* type);
int get_battery_voltage(struct charger_manager* manager, int* voltage);
int get_battery_capacity(struct charger_manager* manager, int* capacity);
int get_battery_temp(struct charger_manager* manager, int* val);
int get_battery_current(struct charger_manager* manager, int* cur);
int get_battery_status(struct charger_manager* manager, enum battery_status_e* state);
int set_battery_vbus_state(struct charger_manager* manager, bool enable);
#ifdef CONFIG_CHARGERD_SYNC_CHARGE_STATE
int set_battery_charge_state(struct charger_manager* manager, unsigned int state);
#endif
#endif
