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

#define MAX_NUM_CHARS 100
#define MAX_NUM_PLOT_PARAMS 30

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
    struct charger_plot_parameter tmp_charge_plot_table[MAX_NUM_PLOT_PARAMS];
    char buf[MAX_NUM_CHARS];
    int plot_table_size = 0;

    FILE* f = fopen(CONFIG_CHARGER_CONFIGURATION_FILE_PATH, "r");
    if (!f) {
        chargererr("fopen failed\n");
        return CHARGER_FAILED;
    }

    desc->plots = -1;

    while (fgets(buf, MAX_NUM_CHARS, f)) {
        int ret = -1;

        int buf_len = strlen(buf);
        if (buf[buf_len - 1] == '\n')
            buf[buf_len - 1] = '\0';

        ret = strncmp(buf, "charger_supply=", sizeof("charger_supply=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("charger_supply=") - 1];
            strncpy(desc->charger_supply, tmp, MAX_BUF_LEN);
            continue;
        }
        ret = strncmp(buf, "charger_adapter=", sizeof("charger_adapter=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("charger_adapter=") - 1];
            strncpy(desc->charger_adapter, tmp, MAX_BUF_LEN);
            continue;
        }
        ret = strncmp(buf, "charger=", sizeof("charger=") - 1);
        if (ret == 0) {
            char* segmentation;
            char* tmp = &buf[sizeof("charger=") - 1];

            if ((segmentation = strstr(tmp, ";")) == NULL) {
                strncpy(desc->charger[0], tmp, MAX_BUF_LEN);
            } else {
                char* sub_str;
                int index = -1;
                sub_str = strtok(tmp, ";");
                while (sub_str != NULL) {
                    index++;
                    strncpy(desc->charger[index], sub_str, MAX_BUF_LEN);
                    sub_str = strtok(NULL, ";");
                }
            }
            continue;
        }
        ret = strncmp(buf, "fuel_gauge=", sizeof("fuel_gauge=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("fuel_gauge=") - 1];
            strncpy(desc->fuel_gauge, tmp, MAX_BUF_LEN);
            continue;
        }
        ret = strncmp(buf, "algo=", sizeof("algo=") - 1);
        if (ret == 0) {
            char* segmentation;
            char* tmp = &buf[sizeof("algo=") - 1];
            if ((segmentation = strstr(tmp, ";")) == NULL) {
                strncpy(desc->algo[0], tmp, MAX_BUF_LEN);
            } else {
                char* sub_str;
                int index = -1;

                sub_str = strtok(tmp, ";");
                while (sub_str != NULL) {
                    index++;
                    strncpy(desc->algo[index], sub_str, MAX_BUF_LEN);
                    sub_str = strtok(NULL, ";");
                }
            }
            continue;
        }

        ret = strncmp(buf, "chargers=", sizeof("chargers=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("chargers=") - 1];
            desc->chargers = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "polling_interval_ms=", sizeof("polling_interval_ms=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("polling_interval_ms=") - 1];
            desc->polling_interval_ms = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "fullbatt_capacity=", sizeof("fullbatt_capacity=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("fullbatt_capacity=") - 1];
            desc->fullbatt_capacity = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "fullbatt_current=", sizeof("fullbatt_current=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("fullbatt_current=") - 1];
            desc->fullbatt_current = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "fullbatt_duration_ms=", sizeof("fullbatt_duration_ms=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("fullbatt_duration_ms=") - 1];
            desc->fullbatt_duration_ms = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "fault_duration_ms=", sizeof("fault_duration_ms=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("fault_duration_ms=") - 1];
            desc->fault_duration_ms = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "temp_min=", sizeof("temp_min=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("temp_min=") - 1];
            desc->temp_min = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "temp_min_r=", sizeof("temp_min_r=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("temp_min_r=") - 1];
            desc->temp_min_r = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "temp_max=", sizeof("temp_max=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("temp_max=") - 1];
            desc->temp_max = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "temp_max_r=", sizeof("temp_max_r=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("temp_max_r=") - 1];
            desc->temp_max_r = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "temp_skin=", sizeof("temp_skin=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("temp_skin=") - 1];
            desc->temp_skin = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "temp_skin_r=", sizeof("temp_skin_r=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("temp_skin_r=") - 1];
            desc->temp_skin_r = atoi(tmp);
            continue;
        }
        ret = strncmp(buf, "enable_delay_ms=", sizeof("enable_delay_ms=") - 1);
        if (ret == 0) {
            char* tmp = &buf[sizeof("enable_delay_ms=") - 1];
            desc->enable_delay_ms = atoi(tmp);
            continue;
        }

        ret = strncmp(buf, "charger_fault_plot_table=", sizeof("charger_fault_plot_table=") - 1);
        if (ret == 0) {
            struct charger_plot_parameter tmp_param;
            int tmp_array[7];
            int tmp_index = 0;
            char* sub_str;
            char* tmp = &buf[sizeof("charger_fault_plot_table=") - 1];

            sub_str = strtok(tmp, ",");
            while (sub_str) {
                tmp_array[tmp_index] = atoi(sub_str);
                tmp_index++;
                sub_str = strtok(NULL, ",");
                if (tmp_index == 7) {
                    tmp_param.temp_range_min = tmp_array[0];
                    tmp_param.temp_range_max = tmp_array[1];
                    tmp_param.vol_range_min = tmp_array[2];
                    tmp_param.vol_range_max = tmp_array[3];
                    tmp_param.charger_index = tmp_array[4];
                    tmp_param.work_current = tmp_array[5];
                    tmp_param.supply_vol = tmp_array[6];
                    desc->fault = tmp_param;
                }
            }
            continue;
        }

        ret = strncmp(buf, "charger_plot_table", sizeof("charger_plot_table") - 1);
        if (ret == 0) {
            int mask = 0;
            char* tmp = &buf[sizeof("charge_plot_table") - 1];

            desc->plots++;
            while ((tmp = strstr(tmp, "_")) != NULL) {
                mask = mask | (1 << atoi(tmp + 1));
                tmp = tmp + 2;
            }
            desc->plot[desc->plots].mask = mask;
            continue;
        }

        ret = strncmp(buf, "{", sizeof("{") - 1);
        if (ret == 0) {
            if (strlen(buf) == 1)
                continue;
            else {
                struct charger_plot_parameter tmp_param;
                int tmp_array[7];
                int tmp_index = 0;
                char* sub_str;
                char* tmp = &buf[sizeof("{") - 1];

                sub_str = strtok(tmp, ",");
                while (sub_str) {
                    tmp_array[tmp_index] = atoi(sub_str);
                    tmp_index++;
                    sub_str = strtok(NULL, ",");
                    if (tmp_index == 7) {
                        tmp_param.temp_range_min = tmp_array[0];
                        tmp_param.temp_range_max = tmp_array[1];
                        tmp_param.vol_range_min = tmp_array[2];
                        tmp_param.vol_range_max = tmp_array[3];
                        tmp_param.charger_index = tmp_array[4];
                        tmp_param.work_current = tmp_array[5];
                        tmp_param.supply_vol = tmp_array[6];
                        tmp_charge_plot_table[plot_table_size] = tmp_param;
                        plot_table_size++;
                    }
                }
            }
        }

        ret = strncmp(buf, "};", sizeof("};") - 1);
        if (ret == 0) {
            struct charger_plot_parameter* tlbs = (struct charger_plot_parameter*)malloc(plot_table_size * sizeof(struct charger_plot_parameter));
            if (NULL == tlbs) {
                chargererr("alloc plot tables no memory\n");
                goto fail;
            }

            memcpy(tlbs, tmp_charge_plot_table, plot_table_size * sizeof(struct charger_plot_parameter));
            desc->plot[desc->plots].tlbs = tlbs;
            desc->plot[desc->plots].parameters = plot_table_size;
            plot_table_size = 0;
            memset(tmp_charge_plot_table, 0, sizeof(tmp_charge_plot_table));
            continue;
        }
    }

    fclose(f);
    return CHARGER_OK;

fail:
    for (int i = 0; i <= desc->plots; i++) {
        if (desc->plot[i].tlbs != NULL) {
            free(desc->plot[i].tlbs);
            desc->plot[i].tlbs = NULL;
        }
    }
    fclose(f);
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
