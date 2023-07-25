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

#ifndef __CHARGE_ZONE_USER_H
#define __CHARGE_ZONE_USER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <inttypes.h>
#ifdef CONFIG_MIWEAR_DRIVERS_CHARGE
#include <nuttx/power/cw2218.h>
#include <nuttx/power/da9168.h>
#include <nuttx/power/sc8551.h>
#include <nuttx/power/stwlc38.h>
#endif
#include "charge.h"
#include <debug.h>
#include <mqueue.h>
#include <nuttx/power/battery_charger.h>
#include <nuttx/power/battery_gauge.h>
#include <nuttx/power/battery_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Define
 ****************************************************************************/
#define DEFAULT_TIME_GAP (2000) // default m
#define TEMP_VALUE_GAIN 10.0
#define TEMP_PROTECT_SEHLL_VALUE 400 // TEMP_VALUE_GAIN:1 ℃
#define TEMP_PROTECT_BATTY_VALUE_HI 450 // TEMP_VALUE_GAIN:1 ℃
#define TEMP_PROTECT_BATTY_VALUE_LO 0 // ℃
#define TEMP_SHELL_RESUME_INTERVAL 1 // ℃
#define TEMP_BATTY_RESUME_INTERVAL_HI 1 // ℃
#define TEMP_BATTY_RESUME_INTERVAL_LO 1 // ℃

#define PUMP_CONF_VOUT_RATIO (1.91f)
#define PUMP_VBUS_ERRORLO_STAT_SHIFT 8
#define PUMP_VBUS_ERRORHI_STAT_SHIFT 9
#define PUMP_VBAT_INSERT_SHIFT 6
#define PUMP_VBAT_OVP_SHIFT 0
#define PUMP_VBUS_OVP_SHIFT 2

#define CUTOFF_ELECTRICITY_PERCENT 100
#define CUTOFF_CURRENT 0

#define BUCK_VINDPM_INT_FLAG (1 << 0)
#define BUCK_VINDPM_INT_COUNT (0x0f << 2)
#define BUCK_VINDPM_DELAY_MS 100000
#define BUCK_VINDPM_INT_VALUE 2

#define CHARGE_DEBUG_LOG_EN 0

#if (CHARGE_DEBUG_LOG_EN == 1)
#define chargedebug(format, ...) \
    syslog(LOG_DEBUG, EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#else
#define chargedebug(format, ...)
#endif

#define chargeerr(format, ...) \
    syslog(LOG_ERR, EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#define chargeinfo(format, ...) \
    syslog(LOG_INFO, EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#define chargewarn(format, ...) \
    syslog(LOG_WARNING, EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)

typedef enum {
    CHARGE_MODE_NULL,
    CHARGE_MODE_INIT,
    CHARGE_MODE_WPC_ENTER,
    CHARGE_MODE_WPC_QUIT,
    CHARGE_MODE_TEMP_PROTECT,
    CHARGE_MODE_TEMP_PROTECT_QUIT,
    CHARGE_MODE_PUMP_WORK,
    CHARGE_MODE_CUTOFF,
    CHARGE_MODE_CUTOFF_RELEASE,
    CHARGE_MODE_UPDATE,
    CHARGE_MODE_MONITOR,

#ifdef CONFIG_FACTEST_CHARGE
    CHARGE_MODE_FAC_AGING_START = 15,
    CHARGE_MODE_FAC_AGING_STOP,
    CHARGE_MODE_FAC_BUCK_CHARGE,
    CHARGE_MODE_FAC_BUCK_CHARGE_ACK,
#endif

    CHARGE_MODE_MAX,
} charge_mode_e;

typedef enum {
    CHARGE_DEVICE_BUCK,
    CHARGE_DEVICE_PUMP,
    CHARGE_DEVICE_WPC,
    CHARGE_DEVICE_GAUGE,
    CHARGE_DEVICE_MAX,
} charge_device_e;

typedef enum {
    CHARGE_TEMP_PROTECT_SKIN,
    CHARGE_TEMP_PROTECT_BATTY_HI,
    CHARGE_TEMP_PROTECT_BATTY_LO,
    CHARGE_TEMP_PROTECT_MAX,
} charge_temp_protect_e;

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

typedef enum {
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
} pump_config_e; //1C=486mA

typedef enum {
    PUMP_WORK_STATE_UNKNOWN,
    PUMP_WORK_STATE_START,
    PUMP_WORK_STATE_SET,
    PUMP_WORK_STATE_STOP,
    PUMP_WORK_STATE_EXP,
} pump_work_state_e;

typedef enum {
    WPC_SLEEP_NOT,
    WPC_SLEEP_ENTER,
    WPC_SLEEP_QUIT,
    WPC_SLEEP_MAX,
} wpc_sleep_state_e;

typedef enum {
    NO_STAND_CURRENT_50_MA = 50,
    NO_STAND_CURRENT_100_MA = 100,
    NO_STAND_CURRENT_200_MA = 200,
    NO_STAND_CURRENT_300_MA = 300,
    NO_STAND_CURRENT_400_MA = 400,
    NO_STAND_CURRENT_500_MA = 500,
} no_stand_soft_start_cur_e;

typedef struct charge_plot_parameter {
    int temp_range_min;
    int temp_range_max;
    int vol_range_min;
    int vol_range_max;
    int work_chip_type;
    int work_current;
} charge_plot_parameter_t;

typedef struct {
    int service_pid;
    struct work_s works;
    pthread_t rx_detect_threadid;
    charge_mode_e mode;
    uint32_t time_gap;
    int dev_fd[CHARGE_DEVICE_MAX];
    int temperature_uorb_fd;
    int temperature_protect_value[CHARGE_TEMP_PROTECT_MAX];
    bool gauge_inited;
    bool pump_switch_locked;
    bool temp_protect_lock;
    bool pm_lock;
    bool monitor_lock;
    uint8_t pump_open_fail_times;
    timer_t env_timer_id;
    bool cutoff_lock;
    uint64_t cutoff_timer_counter;

#ifdef CONFIG_FACTEST_CHARGE
    bool fac_buck_charge_flag;
    int fac_buck_charge_count;
    bool fac_charge_aging_stop_flag;
#endif

} charge_service_t;

extern charge_zone_t charge_zone_use;
extern charge_service_t charge_service_data;
#endif
