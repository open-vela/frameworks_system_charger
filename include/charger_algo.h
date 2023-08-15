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

#ifndef __CHARGER_ALGO_H
#define __CHARGER_ALGO_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "charger_desc.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct charger_algo;

enum {
    PUMP_CONF_VOUT_MAX = 9100,
    PUMP_CONF_VOUT_DEFAULT = 5500,
    PUMP_CONF_VOL_WORK_START = 3650,
    PUMP_CONF_VOUT_OFFSET = 578,
    PUMP_CONF_VOUT_STEP_DEC = 100,
    PUMP_CONF_VOUT_STEP_INC = 25,
    PUMP_CONF_COUT_STEP_DEC = 100,
    PUMP_CONF_COUT_STEP_INC = 25,
    PUMP_CONF_STARTUP_VOLTAGE = 300,
    PUMP_CONF_STARTUP_VOLTAGE_OFFSET = 25,
    PUMP_CONF_VOL_PUMP_UP_LOCKED = 3450,
    PUMP_CONF_VOL_PUMP_DOWN_LOCKED = 3850,
};

struct charger_algo_ops {
    int (*start)(struct charger_algo* algo);
    int (*update)(struct charger_algo* algo, struct charger_plot_parameter* pa);
    int (*stop)(struct charger_algo* algo);
};

struct charger_algo {
    struct charger_algo_ops* ops;
    void* cm;
    int index;
    struct charger_plot_parameter sp;
    void* priv;
};

struct charger_algo_class {
    const char* algo_id;
    struct charger_algo_ops* ops;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

struct charger_algo_ops* get_algo_ops(char* id);
#endif
