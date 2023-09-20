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

#ifdef CONFIG_CHARGERD_TXT_FILE
static int parse_charger_desc_config_by_txt(struct charger_desc* desc)
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
                char* saveptr = NULL;
                int index = -1;

                sub_str = strtok_r(tmp, ";", &saveptr);
                while (sub_str != NULL) {
                    index++;
                    strncpy(desc->charger[index], sub_str, MAX_BUF_LEN);
                    sub_str = strtok_r(NULL, ";", &saveptr);
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
                char* saveptr = NULL;
                int index = -1;

                sub_str = strtok_r(tmp, ";", &saveptr);
                while (sub_str != NULL) {
                    index++;
                    strncpy(desc->algo[index], sub_str, MAX_BUF_LEN);
                    sub_str = strtok_r(NULL, ";", &saveptr);
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
            char* saveptr = NULL;
            char* tmp = &buf[sizeof("charger_fault_plot_table=") - 1];

            sub_str = strtok_r(tmp, ",", &saveptr);
            while (sub_str) {
                tmp_array[tmp_index] = atoi(sub_str);
                tmp_index++;
                sub_str = strtok_r(NULL, ",", &saveptr);
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
                char* saveptr = NULL;
                char* tmp = &buf[sizeof("{") - 1];

                sub_str = strtok_r(tmp, ",", &saveptr);
                while (sub_str) {
                    tmp_array[tmp_index] = atoi(sub_str);
                    tmp_index++;
                    sub_str = strtok_r(NULL, ",", &saveptr);
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
#endif

#ifdef CONFIG_CHARGERD_JSON_FILE
static int parse_charger_desc_config_by_json(struct charger_desc* desc)
{
    long length;
    char* data;
    cJSON *root, *tmp_pointer, *charging_fault_arry, *charger_list;

    FILE* file = fopen(CONFIG_CHARGER_CONFIGURATION_FILE_PATH, "r");
    if (!file) {
        chargererr("Failed to open file %s\n", CONFIG_CHARGER_CONFIGURATION_FILE_PATH);
        return CHARGER_FAILED;
    }

    fseek(file, 0, SEEK_END);
    length = ftell(file);
    fseek(file, 0, SEEK_SET);
    data = (char*)malloc(length + 1);
    if (data == NULL) {
        chargererr("alloc data no memory\n");
        fclose(file);
        return CHARGER_FAILED;
    }
    fread(data, 1, length, file);
    fclose(file);
    data[length] = '\0';

    root = cJSON_Parse(data);
    if (!root) {
        chargererr("Failed to parse JSON file of chargerd\n");
        free(data);
        return CHARGER_FAILED;
    }

    tmp_pointer = cJSON_GetObjectItem(root, "charger_supply");
    if (tmp_pointer) {
        strncpy(desc->charger_supply, tmp_pointer->valuestring, MAX_BUF_LEN);
    }
    tmp_pointer = cJSON_GetObjectItem(root, "charger_adapter");
    if (tmp_pointer) {
        strncpy(desc->charger_adapter, tmp_pointer->valuestring, MAX_BUF_LEN);
    }
    tmp_pointer = cJSON_GetObjectItem(root, "charger");
    if (tmp_pointer) {
        if ((strstr(tmp_pointer->valuestring, ";")) == NULL) {
            strncpy(desc->charger[0], tmp_pointer->valuestring, MAX_BUF_LEN);
        } else {
            char* sub_str;
            char* saveptr = NULL;
            int index = -1;

            sub_str = strtok_r(tmp_pointer->valuestring, ";", &saveptr);
            while (sub_str != NULL) {
                index++;
                strncpy(desc->charger[index], sub_str, MAX_BUF_LEN);
                sub_str = strtok_r(NULL, ";", &saveptr);
            }
        }
    }
    tmp_pointer = cJSON_GetObjectItem(root, "fuel_gauge");
    if (tmp_pointer) {
        strncpy(desc->fuel_gauge, tmp_pointer->valuestring, MAX_BUF_LEN);
    }
    tmp_pointer = cJSON_GetObjectItem(root, "algo");
    if (tmp_pointer) {
        if ((strstr(tmp_pointer->valuestring, ";")) == NULL) {
            strncpy(desc->algo[0], tmp_pointer->valuestring, MAX_BUF_LEN);
        } else {
            char* sub_str;
            char* saveptr = NULL;
            int index = -1;

            sub_str = strtok_r(tmp_pointer->valuestring, ";", &saveptr);
            while (sub_str != NULL) {
                index++;
                strncpy(desc->algo[index], sub_str, MAX_BUF_LEN);
                sub_str = strtok_r(NULL, ";", &saveptr);
            }
        }
    }

    tmp_pointer = cJSON_GetObjectItem(root, "chargers");
    if (tmp_pointer) {
        desc->chargers = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "polling_interval_ms");
    if (tmp_pointer) {
        desc->polling_interval_ms = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "fullbatt_capacity");
    if (tmp_pointer) {
        desc->fullbatt_capacity = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "fullbatt_current");
    if (tmp_pointer) {
        desc->fullbatt_current = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "fullbatt_duration_ms");
    if (tmp_pointer) {
        desc->fullbatt_duration_ms = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "fault_duration_ms");
    if (tmp_pointer) {
        desc->fault_duration_ms = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "temp_min");
    if (tmp_pointer) {
        desc->temp_min = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "temp_min_r");
    if (tmp_pointer) {
        desc->temp_min_r = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "temp_max");
    if (tmp_pointer) {
        desc->temp_max = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "temp_max_r");
    if (tmp_pointer) {
        desc->temp_max_r = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "temp_skin");
    if (tmp_pointer) {
        desc->temp_skin = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "temp_skin_r");
    if (tmp_pointer) {
        desc->temp_skin_r = tmp_pointer->valueint;
    }
    tmp_pointer = cJSON_GetObjectItem(root, "enable_delay_ms");
    if (tmp_pointer) {
        desc->enable_delay_ms = tmp_pointer->valueint;
    }

    charging_fault_arry = cJSON_GetObjectItem(root, "charger_fault_plot_table");
    if (charging_fault_arry != NULL) {
        cJSON* parameter = charging_fault_arry->child;
        if (parameter != NULL) {
            cJSON *temp_range_min_p, *temp_range_max_p, *vol_range_min_p, *vol_range_max_p, *charger_index_p, *work_current_p, *supply_vol_p;
            temp_range_min_p = cJSON_GetObjectItem(parameter, "temp_range_min");
            temp_range_max_p = cJSON_GetObjectItem(parameter, "temp_range_max");
            vol_range_min_p = cJSON_GetObjectItem(parameter, "vol_range_min");
            vol_range_max_p = cJSON_GetObjectItem(parameter, "vol_range_max");
            charger_index_p = cJSON_GetObjectItem(parameter, "charger_index");
            work_current_p = cJSON_GetObjectItem(parameter, "work_current");
            supply_vol_p = cJSON_GetObjectItem(parameter, "supply_vol");
            if (temp_range_min_p && temp_range_max_p && vol_range_min_p && vol_range_max_p && charger_index_p && work_current_p && supply_vol_p) {
                desc->fault.temp_range_min = temp_range_min_p->valueint;
                desc->fault.temp_range_max = temp_range_max_p->valueint;
                desc->fault.vol_range_min = vol_range_min_p->valueint;
                desc->fault.vol_range_max = vol_range_max_p->valueint;
                desc->fault.charger_index = charger_index_p->valueint;
                desc->fault.work_current = work_current_p->valueint;
                desc->fault.supply_vol = supply_vol_p->valueint;
            } else {
                chargererr("an element of the charging plot table is incomplete\n");
            }
        }
    }

    charger_list = cJSON_GetObjectItem(root, "charger_plot_table_list");
    if (charger_list != NULL) {
        cJSON* charger_plot_table_index;

        desc->plots = 0;
        charger_plot_table_index = charger_list->child;
        while (charger_plot_table_index != NULL) {
            cJSON* name_p = cJSON_GetObjectItem(charger_plot_table_index, "name");

            if (name_p != NULL) {
                int element_num;
                struct charger_plot_parameter* tlbs;
                cJSON* charging_arry;
                const char* name = name_p->valuestring;

                element_num = cJSON_GetObjectItem(charger_plot_table_index, "element_num")->valueint;
                tlbs = (struct charger_plot_parameter*)malloc(element_num * sizeof(struct charger_plot_parameter));
                if (tlbs == NULL) {
                    chargererr("alloc plot tables no memory\n");
                    goto fail;
                }

                charging_arry = cJSON_GetObjectItem(root, name);
                if (charging_arry != NULL) {
                    int i = 0;
                    cJSON* parameter = charging_arry->child;
                    while (parameter != NULL) {
                        cJSON *temp_range_min_p, *temp_range_max_p, *vol_range_min_p, *vol_range_max_p, *charger_index_p, *work_current_p, *supply_vol_p;
                        temp_range_min_p = cJSON_GetObjectItem(parameter, "temp_range_min");
                        temp_range_max_p = cJSON_GetObjectItem(parameter, "temp_range_max");
                        vol_range_min_p = cJSON_GetObjectItem(parameter, "vol_range_min");
                        vol_range_max_p = cJSON_GetObjectItem(parameter, "vol_range_max");
                        charger_index_p = cJSON_GetObjectItem(parameter, "charger_index");
                        work_current_p = cJSON_GetObjectItem(parameter, "work_current");
                        supply_vol_p = cJSON_GetObjectItem(parameter, "supply_vol");
                        if (temp_range_min_p && temp_range_max_p && vol_range_min_p && vol_range_max_p && charger_index_p && work_current_p && supply_vol_p) {
                            tlbs[i].temp_range_min = temp_range_min_p->valueint;
                            tlbs[i].temp_range_max = temp_range_max_p->valueint;
                            tlbs[i].vol_range_min = vol_range_min_p->valueint;
                            tlbs[i].vol_range_max = vol_range_max_p->valueint;
                            tlbs[i].charger_index = charger_index_p->valueint;
                            tlbs[i].work_current = work_current_p->valueint;
                            tlbs[i].supply_vol = supply_vol_p->valueint;

                            i++;
                        } else {
                            chargererr("an element of the charging plot table is incomplete\n");
                        }
                        parameter = parameter->next;
                    }
                    desc->plot[desc->plots].tlbs = tlbs;
                    desc->plot[desc->plots].parameters = element_num;
                    desc->plot[desc->plots].mask = cJSON_GetObjectItem(charger_plot_table_index, "mask")->valueint;
                    desc->plots++;
                } else {
                    chargererr("The charging curve table named %s was not found.\n", name_p->valuestring);
                    free(tlbs);
                    tlbs = NULL;
                }
            }

            charger_plot_table_index = charger_plot_table_index->next;
        }
    }

    cJSON_Delete(root);
    free(data);
    return CHARGER_OK;

fail:
    for (int i = 0; i < desc->plots; i++) {
        if (desc->plot[i].tlbs != NULL) {
            free(desc->plot[i].tlbs);
            desc->plot[i].tlbs = NULL;
        }
    }
    cJSON_Delete(root);
    free(data);
    return CHARGER_FAILED;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int charger_desc_init(struct charger_desc* desc)
{
    int ret;

    memset(desc, 0, sizeof(struct charger_desc));

#ifdef CONFIG_CHARGERD_JSON_FILE
    ret = parse_charger_desc_config_by_json(desc);
#else
    ret = parse_charger_desc_config_by_txt(desc);
#endif

    if (ret < 0) {
        chargererr("failed to parse charging related parameters\n");
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
