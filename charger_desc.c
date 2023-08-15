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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "charger_desc.h"
#include "charger_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define CHARGER_DEFAULT_DEV "/dev/charge/batt_charger"
#define GAUGE_DEFAULT_DEV "/dev/charge/batt_gauge"
#define PUMP_DEFAULT_DEV "/dev/charge/charger_pump"
#define ADPATER_DEFAULT_DEV "/dev/charge/wireless_rx"
#define SUPPLY_DEFAULT_DEV "/dev/charge/wireless_rx"
#define CHARGER_DEFAUL_ALGO "buck"
#define PUMP_DEFAUL_ALGO "pump"
#define ADAPTER_EN_DELAY 3000
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef enum {
    CHARGE_CURRENT_0_00C = 0,
    CHARGE_CURRENT_0_04C = 20,
    CHARGE_CURRENT_0_06C = 30,
    CHARGE_CURRENT_0_30C = 145,
    CHARGE_CURRENT_0_61C = 300,
    CHARGE_CURRENT_0_70C = 340,
    CHARGE_CURRENT_1_00C = 486,
    CHARGE_CURRENT_1_50C = 729,
    CHARGE_CURRENT_2_00C = 920,
} charge_current_e; //1C=486mA

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct charger_plot_parameter g_charger_plot_table[] = {
    { -500, 0, 0, 0xffff, -1, CHARGE_CURRENT_0_00C, 0 },
    { 1, 119, 2100, 3000, 0, CHARGE_CURRENT_0_04C, 5500 },
    { 1, 119, 3000, 4435, 0, CHARGE_CURRENT_0_30C, 5500 },
    { 1, 119, 4435, 0xffff, 0, CHARGE_CURRENT_0_30C, 5500 },

    { 120, 159, 2100, 3000, 0, CHARGE_CURRENT_0_04C, 5500 },
    { 120, 159, 3000, 3600, 0, CHARGE_CURRENT_0_30C, 5500 },
    { 120, 159, 3600, 4140, 0, CHARGE_CURRENT_1_00C, 5500 },
    { 120, 159, 4140, 4435, 0, CHARGE_CURRENT_0_70C, 5500 },
    { 120, 159, 4435, 0xffff, 0, CHARGE_CURRENT_0_70C, 5500 },

    { 160, 449, 2100, 3000, 0, CHARGE_CURRENT_0_04C, 5500 },
    { 160, 449, 3000, 3650, 0, CHARGE_CURRENT_0_30C, 5500 },
    { 160, 449, 3650, 4140, 1, CHARGE_CURRENT_2_00C, 0 },
    { 160, 449, 4140, 4435, 0, CHARGE_CURRENT_1_50C, 5500 },
    { 160, 449, 4435, 0xffff, 0, CHARGE_CURRENT_1_50C, 5500 },
    { 450, 0xffff, 0, 0xffff, -1, CHARGE_CURRENT_0_00C, 0 }
};

static const struct charger_plot_parameter g_charger_plot_table_noStand[] = {
    { -500, 0, 0, 0xffff, -1, CHARGE_CURRENT_0_00C, 0 },
    { 1, 39, 2100, 0xffff, 0, 140, 5500 },
    { 40, 449, 2100, 0xffff, 0, 150, 5500 },
    { 450, 0xffff, 0, 0xffff, -1, CHARGE_CURRENT_0_00C, 0 }
};

static const struct charger_plot_parameter g_charger_plot_table_noStand_type3[] = {
    { -500, 0, 0, 0xffff, -1, CHARGE_CURRENT_0_00C, 0 },
    { 1, 159, 2100, 3000, 0, CHARGE_CURRENT_0_04C, 5500 },
    { 1, 159, 3000, 4435, 0, CHARGE_CURRENT_0_30C, 5500 },
    { 1, 159, 4435, 0xffff, 0, CHARGE_CURRENT_0_30C, 5500 },

    { 160, 449, 2100, 3000, 0, CHARGE_CURRENT_0_04C, 5500 },
    { 160, 449, 3000, 3600, 0, CHARGE_CURRENT_0_30C, 5500 },
    { 160, 449, 3600, 4435, 0, CHARGE_CURRENT_1_00C, 5500 },
    { 160, 449, 4435, 0xffff, 0, CHARGE_CURRENT_1_00C, 5500 },
    { 450, 0xffff, 0, 0xffff, -1, CHARGE_CURRENT_0_00C, 0 }
};

static const struct charger_plot_parameter g_fault_pa = { 160, 449, 2100, 0xffff, 0, CHARGE_CURRENT_0_30C, 5500 };

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int charger_desc_default(struct charger_desc* desc)
{
    struct charger_plot_parameter* tlbs;
    int pnum;
    int i;

    desc->chargers = 2;
    strlcpy(desc->charger_supply, ADPATER_DEFAULT_DEV, MAX_BUF_LEN);
    strlcpy(desc->charger_adapter, ADPATER_DEFAULT_DEV, MAX_BUF_LEN);
    strlcpy(desc->charger[0], CHARGER_DEFAULT_DEV, MAX_BUF_LEN);
    strlcpy(desc->charger[1], PUMP_DEFAULT_DEV, MAX_BUF_LEN);
    strlcpy(desc->fuel_gauge, GAUGE_DEFAULT_DEV, MAX_BUF_LEN);
    strlcpy(desc->algo[0], CHARGER_DEFAUL_ALGO, MAX_BUF_LEN);
    strlcpy(desc->algo[1], PUMP_DEFAUL_ALGO, MAX_BUF_LEN);
    desc->polling_interval_ms = 1000;
    desc->fullbatt_capacity = 100;
    desc->fullbatt_current = 0;
    desc->fullbatt_duration_ms = 180000;
    desc->fault_duration_ms = 60000;
    desc->temp_min = 10;
    desc->temp_min_r = 20;
    desc->temp_max = 450;
    desc->temp_max_r = 440;
    desc->temp_skin = 400;
    desc->temp_skin_r = 390;
    desc->plots = 3;
    pnum = ARRAY_SIZE(g_charger_plot_table);
    tlbs = zalloc(pnum * sizeof(struct charger_plot_parameter));
    if (NULL == tlbs) {
        chargererr("alloc plot tables no memory\n");
        goto fail;
    }
    for (i = 0; i < pnum; i++) {
        tlbs[i] = g_charger_plot_table[i];
    }
    desc->plot[0].tlbs = tlbs;
    desc->plot[0].parameters = pnum;
    desc->plot[0].mask = 1 << 3;
    pnum = ARRAY_SIZE(g_charger_plot_table_noStand_type3);
    tlbs = zalloc(pnum * sizeof(struct charger_plot_parameter));
    if (NULL == tlbs) {
        chargererr("alloc plot tables no memory\n");
        goto fail;
    }
    for (i = 0; i < pnum; i++) {
        tlbs[i] = g_charger_plot_table_noStand_type3[i];
    }
    desc->plot[1].tlbs = tlbs;
    desc->plot[1].parameters = pnum;
    desc->plot[1].mask = 1 << 2;
    pnum = ARRAY_SIZE(g_charger_plot_table_noStand);
    tlbs = zalloc(pnum * sizeof(struct charger_plot_parameter));
    if (NULL == tlbs) {
        chargererr("alloc plot tables no memory\n");
        goto fail;
    }
    for (i = 0; i < pnum; i++) {
        tlbs[i] = g_charger_plot_table_noStand[i];
    }
    desc->plot[2].tlbs = tlbs;
    desc->plot[2].parameters = pnum;
    desc->plot[2].mask = (1 << 0) | (1 << 1);
    desc->fault = g_fault_pa;
    desc->enable_delay_ms = ADAPTER_EN_DELAY;
    return CHARGER_OK;
fail:
    for (i = 0; i < desc->plots; i++) {
        if (desc->plot[i].parameters) {
            free(desc->plot[i].tlbs);
        }
    }
    return CHARGER_FAILED;
}

static int charger_desc_by_config(struct charger_desc* desc, const char* config)
{
    /*TODO: return failed*/

    return CHARGER_FAILED;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int charger_desc_init(struct charger_desc* desc, const char* config)
{
    int ret;

    memset(desc, 0, sizeof(struct charger_desc));
    if (NULL == config) {
        ret = charger_desc_default(desc);
        if (ret < 0) {
            chargererr("charger_desc_default failed");
        }
    } else {
        ret = charger_desc_by_config(desc, config);
        if (ret < 0) {
            chargererr("charger_desc_by_config failed");
        }
    }

    return ret;
}

void charger_desc_unit(struct charger_desc* desc)
{
    int i;

    for (i = 0; i < desc->plots; i++) {
        if (desc->plot[i].parameters) {
            free(desc->plot[i].tlbs);
        }
    }
}
