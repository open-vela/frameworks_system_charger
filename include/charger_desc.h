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

#ifndef __CHARGER_DESC_H
#define __CHARGER_DESC_H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define MAX_CHARGERS 2
#define MAX_PLOTS 5
#define MAX_BUF_LEN 32

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct charger_plot_parameter {
    int temp_range_min;
    int temp_range_max;
    int vol_range_min;
    int vol_range_max;
    int charger_index;
    int work_current; //mA
    int supply_vol;
};

struct charger_plot {
    struct charger_plot_parameter* tlbs;
    int parameters;
    unsigned int mask;
};

struct charger_desc {
    char charger_supply[MAX_BUF_LEN];
    char charger_adapter[MAX_BUF_LEN];
    char charger[MAX_CHARGERS][MAX_BUF_LEN];
    char algo[MAX_CHARGERS][MAX_BUF_LEN];
    int chargers;
    char fuel_gauge[MAX_BUF_LEN];
    unsigned int polling_interval_ms;
    unsigned int fullbatt_capacity;
    int fullbatt_current;
    unsigned int fullbatt_duration_ms;
    unsigned int fault_duration_ms;
    int temp_min;
    int temp_min_r;
    int temp_max;
    int temp_max_r;
    int temp_skin;
    int temp_skin_r;
    struct charger_plot plot[MAX_PLOTS];
    int plots;
    struct charger_plot_parameter fault;
    unsigned int enable_delay_ms;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int charger_desc_init(struct charger_desc* desc, const char* config);
void charger_desc_unit(struct charger_desc* desc);

#endif
