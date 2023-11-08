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

#define MAX_NUM_CHARS 100
#define MAX_NUM_PLOT_PARAMS 30

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int parse_charger_desc_config(struct charger_desc* desc)
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
            desc->chargers = 1;
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
            desc->chargers = index + 1;
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
            cJSON* name_ptr = cJSON_GetObjectItem(charger_plot_table_index, "name");

            if (name_ptr != NULL) {
                int element_num;
                struct charger_plot_parameter* tlbs;
                cJSON* charging_arry;
                const char* name = name_ptr->valuestring;

                element_num = cJSON_GetObjectItem(charger_plot_table_index, "element_num")->valueint;
                tlbs = (struct charger_plot_parameter*)malloc(element_num * sizeof(struct charger_plot_parameter));
                if (tlbs == NULL) {
                    chargererr("alloc plot tables no memory\n");
                    goto fail;
                }

                charging_arry = cJSON_GetObjectItem(charger_plot_table_index, name);
                if (charging_arry != NULL) {
                    if (!cJSON_IsArray(charging_arry)) {
                        chargererr("%s is not a json array\n", name);
                        continue;
                    }

                    for (int i = 0; i < cJSON_GetArraySize(charging_arry); i++) {
                        cJSON* parameter = cJSON_GetArrayItem(charging_arry, i);
                        if (!cJSON_IsArray(parameter)) {
                            chargererr("%s is not a json array\n", parameter->valuestring);
                            continue;
                        }

                        int tmp_array[7];
                        int item_size = cJSON_GetArraySize(parameter);
                        for (int j = 0; j < item_size; j++) {
                            cJSON* item = cJSON_GetArrayItem(parameter, j);
                            tmp_array[j] = item->valueint;
                            if (j == 6) {
                                tlbs[i].temp_range_min = tmp_array[0];
                                tlbs[i].temp_range_max = tmp_array[1];
                                tlbs[i].vol_range_min = tmp_array[2];
                                tlbs[i].vol_range_max = tmp_array[3];
                                tlbs[i].charger_index = tmp_array[4];
                                tlbs[i].work_current = tmp_array[5];
                                tlbs[i].supply_vol = tmp_array[6];
                            }
                        }
                    }

                    desc->plot[desc->plots].tlbs = tlbs;
                    desc->plot[desc->plots].parameters = element_num;
                    desc->plot[desc->plots].mask = cJSON_GetObjectItem(charger_plot_table_index, "mask")->valueint;
                    desc->plots++;
                } else {
                    chargererr("The charging curve table named %s was not found.\n", name_ptr->valuestring);
                    free(tlbs);
                    tlbs = NULL;
                }
            }

            charger_plot_table_index = charger_plot_table_index->next;
        }
    } else {
        chargererr("charger plot table not found!\n");
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

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int charger_desc_init(struct charger_desc* desc)
{
    int ret;

    memset(desc, 0, sizeof(struct charger_desc));
    ret = parse_charger_desc_config(desc);
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
