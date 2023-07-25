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

#ifndef __CHARGE_HANDLE_H
#define __CHARGE_HANDLE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <inttypes.h>
#include <mqueue.h>
#include <nuttx/power/battery_charger.h>
#include <nuttx/power/battery_gauge.h>
#include <nuttx/power/battery_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CHARGE_CURRENT_0_04C 20
#define CHARGE_CURRENT_0_06C 30
#define CHARGE_CURRENT_0_30C 145
#define CHARGE_CURRENT_0_61C 300
#define CHARGE_CURRENT_0_70C 340
#define CHARGE_CURRENT_1_00C 486
#define CHARGE_CURRENT_1_50C 729
#define CHARGE_CURRENT_2_00C 920

#define BAT_TEMP_0C 0
#define BAT_TEMP_1C 10
#define BAT_TEMP_4C 40
#define BAT_TEMP_11C 110
#define BAT_TEMP_16C 160
#define BAT_TEMP_44C 440
#define BAT_TEMP_45C 450

#define SHELL_TEMP_34C 34
#define SKIN_TEMP_39C 39
#define SKIN_TEMP_40C 40
#define SKIN_TEMP_42C 42
#define SKIN_TEMP_43C 43

#define TMPF_TEMP_39C 39
#define TMPF_TEMP_40C 40

#define NUM_CHR_LEVEL 16
#define PUMP_STARTUP_VOLTAGE 300
#define PUMP_VOLTAGE_STEP 25
#define PUMP_VOLTAGE_SETP_100 100
#define PUMP_CURRENT_LOW_STEP 25
#define PUMP_CURRENT_HIGH_STEP 100
#define PUMP_REAJUST_CURRENT 200
#define PUMP_STARTUP_VOLTAGE_OFFSET 25
#define CHARGE_VOLTAGE_CC 3000
#define CHARGE_VOLTAGE_UNPOWERD 3650
#define CHARGE_VOLTAGE_CV 4140
#define CHARGE_REAJUST_VOLTAGE 3840

#define PUMP_VBUS_ERRORLO_STAT_SHIFT 8
#define PUMP_VBUS_ERRORHI_STAT_SHIFT 9
#define PUMP_VBAT_INSERT_SHIFT 6
#define PUMP_VBAT_OVP_SHIFT 0
#define PUMP_VBUS_OVP_SHIFT 2

#define BAT_VOLTAGE_4150 4150

#define DELAY_1200MS 1200000

enum chrge_stage_e {
    CHAGER_TYPE_UNKNOWN = 0,
    TEMP_CHARGE_TRICKLE,
    LOW_TEMP_STEP2_CHARGE_CC_1,
    LOW_TEMP_STEP2_CHARGE_CC_2,
    LOW_TEMP_STEP1_CHARGE_CC_1,
    LOW_TEMP_STEP1_CHARGE_CC_2,
    LOW_TEMP_STEP0_CHARGE_NO_POWERED,
    LOW_TEMP_STEP0_CHARGE_CC_1,
    LOW_TEMP_STEP0_CHARGE_CC_2,
    LOW_TEMP_STEP0_CHARGE_CC_3,
    NORMAL_TEMP_CHARGE_NO_POWERED,
    NORMAL_TEMP_CHARGE_CC_1,
    NORMAL_TEMP_CHARGE_CC_2,
    NORMAL_TEMP_CHARGE_CC_3,
    TEMP_CHARGE_STOP,
};

enum pump_work_state_e {
    PUMP_WORK_STATE_UNKNOWN,
    PUMP_WORK_STATE_START,
    PUMP_WORK_STATE_SET,
    PUMP_WORK_STATE_STOP,
    PUMP_WORK_STATE_EXP,
};

enum chip_type_type_e {
    CHIP_WORK_TYPE_UNKNOWN,
    CHIP_WORK_TYPE_CHARGER_PRE,
    CHIP_WORK_TYPE_PUMP,
    CHIP_WORK_TYPE_CHARGER,
    CHIP_WORK_TYPE_CHARGER_LOW_1,
    CHIP_WORK_TYPE_CHARGER_LOW_2,
    CHIP_WORK_TYPE_STOP,
};

enum bat_charge_temp_state_e {
    BAT_CHARGE_TEMP_UNKNOWN,
    BAT_CHARGE_TEMP_STOP,
    BAT_CHARGE_TEMP_RECOVER,
};

enum charge_temp_state_e {
    CHARGE_TEMP_UNKNOWN,
    CHARGE_TEMP_STOP,
    CHARGE_TEMP_RECOVER,
};

struct batt_charge_range_s {
    int16_t temp_threshold_l;
    int16_t temp_threshold_h;
    uint16_t vol_threshold_l;
    uint16_t vol_threshold_h;
    uint8_t chr_type;
};

struct batt_info_s {
    uint16_t voltage;
    int16_t current;
    int16_t temp;
};

struct batt_chrge_config_s {
    uint8_t charge_phase;
    int16_t charge_cur_val;
};

struct temp_info_s {
    float shell_temp;
    float skin_temp;
    int tmpf;
};

#ifdef CONFIG_FACTEST_CHARGE

struct mq_info_s {
    mqd_t mq_id;
    char* pbuf;
    int size;
};

#endif

#endif
