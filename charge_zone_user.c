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
#include <errno.h>
#include <fcntl.h>
#include <nuttx/arch.h>
#include <nuttx/config.h>
#include <nuttx/wqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <system/state.h>
#include <time.h>
#include <uORB/uORB.h>
#include <unistd.h>

#include "charge_configuration.h"
#include "charge_zone_user.h"

/****************************************************************************
 * Define
 ****************************************************************************/
const charge_plot_parameter_t charge_plot_table[] = {
    { -500, 0, 0, 0xffff, CHARHE_CHIP_TYPE_NULL, CHARGE_CURRENT_0_00C },
    { 1, 119, 2100, 3000, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_04C },
    { 1, 119, 3000, 4435, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },
    { 1, 119, 4435, 0xffff, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },

    { 120, 159, 2100, 3000, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_04C },
    { 120, 159, 3000, 3600, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },
    { 120, 159, 3600, 4140, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_1_00C },
    { 120, 159, 4140, 4435, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_70C },
    { 120, 159, 4435, 0xffff, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_70C },

    { 160, 449, 2100, 3000, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_04C },
    { 160, 449, 3000, PUMP_CONF_VOL_WORK_START, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },
    { 160, 449, PUMP_CONF_VOL_WORK_START, 4140, CHARHE_CHIP_TYPE_PUMP, CHARGE_CURRENT_2_00C },
    { 160, 449, 4140, 4435, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_1_50C },
    { 160, 449, 4435, 0xffff, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_1_50C },
    { 450, 0xffff, 0, 0xffff, CHARHE_CHIP_TYPE_NULL, CHARGE_CURRENT_0_00C }
};

const charge_plot_parameter_t charge_plot_table_noStand[] = {
    { -500, 0, 0, 0xffff, CHARHE_CHIP_TYPE_NULL, CHARGE_CURRENT_0_00C },
    { 1, 39, 2100, 0xffff, CHARHE_CHIP_TYPE_BUCK, 140 },
    { 40, 449, 2100, 0xffff, CHARHE_CHIP_TYPE_BUCK, 150 },
    { 450, 0xffff, 0, 0xffff, CHARHE_CHIP_TYPE_NULL, CHARGE_CURRENT_0_00C }
};

const charge_plot_parameter_t charge_plot_table_noStand_type3[] = {
    { -500, 0, 0, 0xffff, CHARHE_CHIP_TYPE_NULL, CHARGE_CURRENT_0_00C },
    { 1, 159, 2100, 3000, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_04C },
    { 1, 159, 3000, 4435, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },
    { 1, 159, 4435, 0xffff, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },

    { 160, 449, 2100, 3000, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_04C },
    { 160, 449, 3000, 3600, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_0_30C },
    { 160, 449, 3600, 4435, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_1_00C },
    { 160, 449, 4435, 0xffff, CHARHE_CHIP_TYPE_BUCK, CHARGE_CURRENT_1_00C },
    { 450, 0xffff, 0, 0xffff, CHARHE_CHIP_TYPE_NULL, CHARGE_CURRENT_0_00C }
};

/****************************************************************************
 * Static Define
 ****************************************************************************/
static int get_cur_voltage_adc(charge_zone_t* zone);
static int get_cur_charge_temperature(charge_zone_t* zone);
static int get_temperature_protect(charge_zone_t* zone);
static int get_temperature_protect_change(charge_zone_t* zone);
static int get_wpc_rx_state(charge_zone_t* zone);
static int get_wpc_rx_change(charge_zone_t* zone);
static int get_wpc_rx_voltage(charge_zone_t* zone, uint16_t* vol);
static int get_work_chip_change(charge_zone_t* zone);
static int get_cur_adapter(charge_zone_t* zone);
static int get_cur_batty_info(charge_zone_t* zone);
static int get_pump_adapter_insert_state(charge_zone_t* zone);
static int ctrol_chip_work(charge_zone_t* zone, int type, bool mode);
static int ctrol_chip_current(charge_zone_t* zone, int type, int current);
static int ctrol_wpc_sleep(charge_zone_t* zone, bool enable, int rx_sleep_state);
static int get_adapter_change(charge_zone_t* zone);
static int ctrol_wpc_rx_out(charge_zone_t* zone, uint16_t vol_base, uint16_t vol_step, bool* pump_status);
static int set_pump_adc(charge_zone_t* zone, bool enable);
static int get_pump_vbus_error_state(charge_zone_t* zone, bool* state);
static int get_pump_charge_en_state(charge_zone_t* zone, bool* pump_vbat_ovp_state, bool* pump_vus_ovp_state);
static int set_wireless_rx_vout_balance(charge_zone_t* zone, bool balance);
static int notify_rx_insert_out(charge_zone_t* zone);
static int quit_wpc_rx(charge_zone_t* zone);
static int get_wpc_vout_on_int_state(charge_zone_t* zone);
static int get_cutoff_condition(charge_zone_t* zone);
static int get_buck_vindpm_int_state(charge_zone_t* zone);
static int no_stand_soft_start_handle(charge_zone_t* zone, int work_chip_type, int work_cur);

charge_service_t charge_service_data = {
    .service_pid = 0,
    .rx_detect_threadid = 0,
    .mode = CHARGE_MODE_NULL,
    .time_gap = DEFAULT_TIME_GAP,
    .dev_fd = { -1 },
    .temperature_uorb_fd = -1,
    .temperature_protect_value = {
        TEMP_PROTECT_SEHLL_VALUE,
        TEMP_PROTECT_BATTY_VALUE_HI,
        TEMP_PROTECT_BATTY_VALUE_LO,
    },
    .gauge_inited = false,
    .pump_switch_locked = false,
    .temp_protect_lock = false,
    .pm_lock = false,
    .monitor_lock = false,
    .pump_open_fail_times = 0,
    .env_timer_id = 0,
    .cutoff_lock = false,
    .cutoff_timer_counter = 0,

#ifdef CONFIG_FACTEST_CHARGE
    .fac_buck_charge_flag = false,
    .fac_buck_charge_count = 0,
    .fac_charge_aging_stop_flag = false,
#endif

};

charge_zone_device_ops_t charge_zone_device_ops_use = {
    .get_wpc_rx_state = get_wpc_rx_state,
    .get_wpc_rx_change = get_wpc_rx_change,
    .get_cur_charge_temperature = get_cur_charge_temperature,
    .get_cur_batty_info = get_cur_batty_info,
    .get_temperature_protect = get_temperature_protect,
    .get_temperature_protect_change = get_temperature_protect_change,
    .get_cur_adapter = get_cur_adapter,
    .get_adapter_change = get_adapter_change,
    .quit_wpc_rx = quit_wpc_rx,
    .ctrol_wpc_sleep = ctrol_wpc_sleep,
    .ctrol_chip_work = ctrol_chip_work,
    .ctrol_chip_current = ctrol_chip_current,
    .get_work_chip_change = get_work_chip_change,
    .ctrol_wpc_rx_out = ctrol_wpc_rx_out,
    .set_pump_adc = set_pump_adc,
    .get_pump_vbus_error_state = get_pump_vbus_error_state,
    .get_pump_charge_en_state = get_pump_charge_en_state,
    .set_wireless_rx_vout_balance = set_wireless_rx_vout_balance,
    .get_cur_voltage_adc = get_cur_voltage_adc,
    .get_pump_adapter_insert_state = get_pump_adapter_insert_state,
    .notify_rx_insert_out = notify_rx_insert_out,
    .get_wpc_rx_voltage = get_wpc_rx_voltage,
    .get_wpc_vout_on_int_state = get_wpc_vout_on_int_state,
    .get_cutoff_condition = get_cutoff_condition,
    .get_buck_vindpm_int_state = get_buck_vindpm_int_state,
    .no_stand_soft_start_handle = no_stand_soft_start_handle,
};

charge_zone_t charge_zone_use = {
    .voltage = -1,
    .wpc_rx_change = false,
    .temperature_protect = 0,
    .temperature_protect_change = false,
    .adapter_change = false,
    .work_current = 0,
    .pump_work_state = PUMP_WORK_STATE_UNKNOWN,
    .batty_info = {
        .current = 0,
        .electricity = 0xff,
        .temperature = 110,
        .voltage = 2100,
    },
    .charge_temperature = {
        .shell_temp = 0,
    },
    .wpc_rx_state = WPC_RX_STATE_QUITE,
    .adapter = WPC_ADAPTER_STATE_NULL,
    .charge_chip_type = CHARHE_CHIP_TYPE_NULL,
    .ops_data = &charge_service_data,
    .ops = &charge_zone_device_ops_use,
};

/****************************************************************************
 * Static Function
 ****************************************************************************/
static int get_cur_voltage_adc(charge_zone_t* zone)
{
    return 0;
}

static int get_cur_charge_temperature(charge_zone_t* zone)
{
    int ret;
    int temperature_protect_old = 0;
    bool temp_updated = false;
    struct device_temperature device_temp;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    //get charge_temperature;
    if ((data == NULL) || (0 > data->temperature_uorb_fd)) {
        chargeerr("temperature_uorb_fd is null\n");
        return -1;
    }
    ret = orb_check(data->temperature_uorb_fd, &temp_updated);
    if (ret == ERROR) {
        chargeerr("orb check failed\n");
        return -1;
    }

    if (temp_updated) {
        ret = orb_copy(ORB_ID(device_temperature), data->temperature_uorb_fd, &device_temp);
        if (ret != OK) {
            chargeerr("temp orb copy failed\n");
            return -1;
        }
        zone->charge_temperature.shell_temp = device_temp.skin * TEMP_VALUE_GAIN;
    }

    chargedebug("zone->temperature_protect=%d\n", zone->temperature_protect);
    if ((zone->charge_temperature.shell_temp >= data->temperature_protect_value[CHARGE_TEMP_PROTECT_SKIN])) {
        zone->temperature_protect_change = (zone->temperature_protect == 0) ? true : false;
        zone->temperature_protect |= (1 << CHARGE_TEMP_PROTECT_SKIN);
    } else if (zone->charge_temperature.shell_temp <= (data->temperature_protect_value[CHARGE_TEMP_PROTECT_SKIN] - 1 * TEMP_VALUE_GAIN)) {
        temperature_protect_old = zone->temperature_protect;
        zone->temperature_protect &= ~(1 << CHARGE_TEMP_PROTECT_SKIN);
        zone->temperature_protect_change = (temperature_protect_old) && (zone->temperature_protect == 0) ? true : false;
    } else {
        zone->temperature_protect_change = false;
    }

    return 0;
}

static int get_temperature_protect(charge_zone_t* zone)
{
    return (zone->temperature_protect);
}

static int get_temperature_protect_change(charge_zone_t* zone)
{
    if (zone->temperature_protect_change) {
        chargeinfo("change! protect=%d stmp=%0.1f btmp=%0.1f\n", zone->temperature_protect, ((float)zone->charge_temperature.shell_temp) / TEMP_VALUE_GAIN, ((float)charge_zone_use.batty_info.temperature) / TEMP_VALUE_GAIN);
    }
    return (zone->temperature_protect_change);
}

static int get_wpc_rx_state(charge_zone_t* zone)
{
    return (zone->wpc_rx_state);
}

static int get_wpc_rx_change(charge_zone_t* zone)
{
    return (zone->wpc_rx_change);
}

static int get_wpc_rx_voltage(charge_zone_t* zone, uint16_t* vol)
{
    int ret = -1;
    uint16_t* rx_vout;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("wpc is not init \n");
        return -1;
    }

    rx_vout = vol;
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_VOLTAGE_INFO, (unsigned long)((uintptr_t)rx_vout));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_VOLTAGE_INFO) failed: %d\n", ret);
        return ret;
    }
    chargedebug("=======rx out vol=%d mv\n", *rx_vout);
    return 0;
}

static int get_work_chip_change(charge_zone_t* zone)
{
    uint16_t i = 0;
    int work_chip_type = 0;
    int work_current = 0;
    int voltage = 0;
    int temperature = 0;
    uint16_t arrayLen = 0;
    charge_plot_parameter_t* table = NULL;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if (data == NULL) {
        chargeerr("no service data \n");
        return -1;
    }

    if (zone->adapter == WPC_ADAPTER_STATE_YES) { // stand
        arrayLen = sizeof(charge_plot_table) / sizeof(charge_plot_table[0]);
        table = (charge_plot_parameter_t*)charge_plot_table;
    } else if (zone->adapter == WPC_ADAPTER_STATE_NO_TYPE3) { // no stand type 3
        arrayLen = sizeof(charge_plot_table_noStand_type3) / sizeof(charge_plot_table_noStand_type3[0]);
        table = (charge_plot_parameter_t*)charge_plot_table_noStand_type3;
    } else { // other no stand
        arrayLen = sizeof(charge_plot_table_noStand) / sizeof(charge_plot_table_noStand[0]);
        table = (charge_plot_parameter_t*)charge_plot_table_noStand;
    }

    voltage = zone->batty_info.voltage;
    temperature = zone->batty_info.temperature;

    for (i = 0; i < arrayLen; i++) {
        if (((temperature >= table[i].temp_range_min)
                && (temperature <= table[i].temp_range_max))
            && ((voltage >= table[i].vol_range_min)
                && (voltage <= table[i].vol_range_max))) {
            work_chip_type = table[i].work_chip_type;
            work_current = table[i].work_current;
            break;
        }
    }

    if (i == arrayLen) {
        chargeerr("can not find work chip type\n");
        work_chip_type = CHARHE_CHIP_TYPE_BUCK;
        work_current = CHARGE_CURRENT_0_30C;
        return 0;
    }
    if ((work_chip_type == zone->charge_chip_type) && (work_current == zone->work_current)) { //no change
        return -1;
    } else if ((zone->charge_chip_type == CHARHE_CHIP_TYPE_PUMP)
        && (zone->work_current == CHARGE_CURRENT_2_00C)
        && (work_chip_type == CHARHE_CHIP_TYPE_BUCK)
        && (work_current == CHARGE_CURRENT_0_30C)
        && (zone->batty_info.voltage > PUMP_CONF_VOL_PUMP_UP_LOCKED)) { //pump lock check for 3.45v
        return -1;
    } else if ((zone->charge_chip_type == CHARHE_CHIP_TYPE_BUCK)
        && (zone->work_current == CHARGE_CURRENT_1_50C)
        && (work_chip_type == CHARHE_CHIP_TYPE_PUMP)
        && (work_current == CHARGE_CURRENT_2_00C)
        && (zone->batty_info.voltage > PUMP_CONF_VOL_PUMP_DOWN_LOCKED)) { //pump lock check for 3.85v
        return -1;
    } else {
        zone->charge_chip_type = work_chip_type;
        zone->work_current = work_current;
        chargeinfo("change:chip_type = %d,work_current = %d adp=%d\n", work_chip_type, work_current, zone->adapter);
        return 0;
    }
}

static int get_cur_batty_info(charge_zone_t* zone)
{
    int ret;
    int temperature_protect_old = 0;
    static uint8_t init_counter = 0;
    unsigned long data_tmp = 0;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if (data == NULL) {
        chargeerr("data is null \n");
        return -1;
    }

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_GAUGE], BATIOC_ONLINE, (unsigned long)((uintptr_t) & (data->gauge_inited)));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", ret);
        return ret;
    }

    if (data->gauge_inited) {
#if 1
        ret = ioctl(data->dev_fd[CHARGE_DEVICE_GAUGE], BATIOC_CAPACITY, (unsigned long)((uintptr_t) & (data_tmp)));
        if (ret < 0) {
            chargeerr("ERROR: ioctl(BATIOC_CAPACITY) failed: %d\n", ret);
            return ret;
        }
        zone->batty_info.electricity = (uint16_t)data_tmp;
        chargedebug("batty_info.electricity=%d\n", zone->batty_info.electricity);
#endif
        ret = ioctl(data->dev_fd[CHARGE_DEVICE_GAUGE], BATIOC_VOLTAGE, (unsigned long)((uintptr_t) & (data_tmp)));
        if (ret < 0) {
            chargeerr("ERROR: ioctl(BATIOC_VOLTAGE) failed: %d\n", ret);
            return ret;
        }
        zone->batty_info.voltage = (uint16_t)data_tmp;

        ret = ioctl(data->dev_fd[CHARGE_DEVICE_GAUGE], BATIOC_CURRENT, (unsigned long)((uintptr_t) & (data_tmp)));
        if (ret < 0) {
            chargeerr("ERROR: ioctl(BATIOC_CURRENT) failed: %d\n", ret);
            return ret;
        }
        zone->batty_info.current = (int16_t)data_tmp;

        ret = ioctl(data->dev_fd[CHARGE_DEVICE_GAUGE], BATIOC_TEMPERATURE, (unsigned long)((uintptr_t) & (data_tmp)));
        if (ret < 0) {
            chargeerr("ERROR: ioctl(BATIOC_TEMPERATURE) failed: %d\n", ret);
            return ret;
        }
        zone->batty_info.temperature = (int16_t)data_tmp;
    } else {
        init_counter++;
        zone->batty_info.voltage = 2100;
        zone->batty_info.temperature = 110;
        if (init_counter == 10) {
            chargeerr("gauge init failed %d times\n", init_counter);
        }
    }

    chargedebug("zone->temperature_protect=%d\n", zone->temperature_protect);
    if ((zone->batty_info.temperature >= data->temperature_protect_value[CHARGE_TEMP_PROTECT_BATTY_HI])
        || (zone->batty_info.temperature <= data->temperature_protect_value[CHARGE_TEMP_PROTECT_BATTY_LO])) {
        chargeinfo("temperature protect! batmp=%d\n", zone->batty_info.temperature);
        zone->temperature_protect_change = (zone->temperature_protect == 0) ? true : false;
        zone->temperature_protect |= (1 << CHARGE_TEMP_PROTECT_BATTY_HI);
    } else if ((zone->batty_info.temperature <= (data->temperature_protect_value[CHARGE_TEMP_PROTECT_BATTY_HI] - 1 * TEMP_VALUE_GAIN))
        && (zone->batty_info.temperature >= (data->temperature_protect_value[CHARGE_TEMP_PROTECT_BATTY_LO] + 1 * TEMP_VALUE_GAIN))) {
        temperature_protect_old = zone->temperature_protect;
        zone->temperature_protect &= ~(1 << CHARGE_TEMP_PROTECT_BATTY_HI);
        zone->temperature_protect_change = (temperature_protect_old) && (zone->temperature_protect == 0) ? true : false;
    } else {
        zone->temperature_protect_change = false;
    }

    return 0;
}

static int get_pump_adapter_insert_state(charge_zone_t* zone)
{
    int ret = 0;
    int pump_state;

    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_PUMP])) {
        chargeerr("pump is not init \n");
        return false;
    }
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_PUMP], BATIOC_STATE, (unsigned long)((uintptr_t)&pump_state));
    if (ret < 0) {
        chargeerr("ERROR: ioctl(BATIOC_STATE) failed: %d\n", ret);
        return false;
    }
    chargedebug("=====pump_state=%02X\n", pump_state);
    return ((pump_state & ADAPTER_INSERT_MASK) ? true : false);
}
static int ctrol_chip_work(charge_zone_t* zone, int type, bool mode)
{
    int ret = 0;
    bool pump_status = false;
    static bool chip_is_on[CHARHE_CHIP_TYPE_MAX] = { 0 };
    FAR struct batio_operate_msg_s msg;

    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_BUCK]) || (0 > data->dev_fd[CHARGE_DEVICE_PUMP])) {
        chargeerr("charge chip is not init \n");
        return -1;
    }
    chargedebug("----ctrol type=%d mode=%d\n", type, mode);
    switch (type) {
    case CHARHE_CHIP_TYPE_BUCK:
        if (mode) { //only one chip can work
            if (zone->ops->ctrol_chip_work(zone, CHARHE_CHIP_TYPE_PUMP, false)) {
                chargeerr("can not close pump\n");
                return -1;
            }
            //set rx out = 5v
            if (zone->ops->ctrol_wpc_rx_out(zone, 0,
                    (PUMP_CONF_VOUT_DEFAULT - PUMP_CONF_VOUT_OFFSET), &pump_status)
                < 0) {
                chargeerr("set rx out fail\n");
                return -1;
            }
        }

        if (mode == chip_is_on[type]) //check need open/close chip again
            //zone->charge_chip_type = type;
            return 0;
        msg.operate_type = BATIO_OPRTN_CHARGE;
        msg.u32 = mode;

        ret = ioctl(data->dev_fd[CHARGE_DEVICE_BUCK], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
        if (ret < 0) {
            chargeerr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", ret);
            return ret;
        }
        chip_is_on[type] = mode;
        break;

    case CHARHE_CHIP_TYPE_PUMP:
        if (mode) { //only one chip can work
            if (zone->ops->ctrol_chip_work(zone, CHARHE_CHIP_TYPE_BUCK, false)) {
                chargeerr("can not close pump\n");
                return -1;
            }
        }
        if (mode == chip_is_on[type])
            //zone->charge_chip_type = type;
            return 0;
        msg.operate_type = mode ? BATIO_OPRTN_SYSON : BATIO_OPRTN_SYSOFF;
        ret = ioctl(data->dev_fd[CHARGE_DEVICE_PUMP], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
        if (ret < 0) {
            chargeerr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", ret);
            return ret;
        }

        chip_is_on[type] = mode;
        break;

    case CHARHE_CHIP_TYPE_NULL: //close all
        if ((chip_is_on[CHARHE_CHIP_TYPE_BUCK] == false)
            && (chip_is_on[CHARHE_CHIP_TYPE_PUMP] == false)) {
            zone->charge_chip_type = type;
            return 0;
        }
        if (zone->ops->ctrol_chip_work(zone, CHARHE_CHIP_TYPE_BUCK, false)
            || zone->ops->ctrol_chip_work(zone, CHARHE_CHIP_TYPE_PUMP, false)) {
            chargeerr("close buck & pump failed\n");
            return -1;
        }
        chip_is_on[CHARHE_CHIP_TYPE_BUCK] = false;
        chip_is_on[CHARHE_CHIP_TYPE_PUMP] = false;
        zone->charge_chip_type = type;
        break;

    default:
        chargeerr("ERROR: chip type error: %d\n", type);
        return -1;
    }
    //zone->charge_chip_type = type;
    return 0;
}

static int set_pump_adc(charge_zone_t* zone, bool enable)
{
    int ret = 0;
    FAR struct batio_operate_msg_s msg_adc;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_PUMP])) {
        chargeerr("pump chip is not init \n");
        return -1;
    }
    msg_adc.operate_type = BATIO_OPRTN_EN_TERM;
    msg_adc.u32 = enable;

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_PUMP], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg_adc));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", ret);
        return ret;
    }

    return 0;
}

static int ctrol_chip_current(charge_zone_t* zone, int type, int current)
{
    int ret = 0;
    int fd = -1;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_BUCK]) || (0 > data->dev_fd[CHARGE_DEVICE_PUMP])) {
        chargeerr("charge chip is not init \n");
        return -1;
    }

    if ((type != CHARHE_CHIP_TYPE_BUCK) && (type != CHARHE_CHIP_TYPE_PUMP)) {
        chargeerr("charge chip type error \n");
        return -1;
    }
    fd = (type == CHARHE_CHIP_TYPE_BUCK) ? data->dev_fd[CHARGE_DEVICE_BUCK] : data->dev_fd[CHARGE_DEVICE_PUMP];
    ret = ioctl(fd, BATIOC_CURRENT, (unsigned long)((uintptr_t)&current));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_CURRENT) failed: %d\n", ret);
        return ret;
    }
    return 0;
}

static int ctrol_wpc_sleep(charge_zone_t* zone, bool enable, int rx_sleep_state)
{
    int ret = 0;
    FAR struct batio_operate_msg_s msg;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("wpc is not init \n");
        return -1;
    }
    msg.operate_type = (enable) ? BATIO_OPRTN_SYSOFF : BATIO_OPRTN_SYSON;
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", ret);
        return ret;
    }

    msg.operate_type = BATIO_OPRTN_EN_TERM;
    msg.u8[0] = rx_sleep_state;

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", ret);
        return ret;
    }

    return 0;
}
static int get_cur_adapter(charge_zone_t* zone)
{
    int adapter = 0;
    int ret = 0;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("wpc is not init \n");
        return -1;
    }
#if 1
    if (zone->adapter == WPC_ADAPTER_STATE_YES) {
        zone->adapter_change = false;
        return 0;
    }
#endif
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_GET_PROTOCOL, (unsigned long)((uintptr_t)&adapter));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_GET_PROTOCOL) failed: %d\n", ret);
        return ret;
    }
    zone->adapter_change = (adapter == zone->adapter) ? false : true;
    if (zone->adapter_change) {
        chargeinfo("adapter change to %d\n", adapter);
    }
    zone->adapter = adapter;
    return 0;
}
static int get_adapter_change(charge_zone_t* zone)
{
    return (zone->adapter_change);
}
static int get_wpc_vout_on_int_state(charge_zone_t* zone)
{
    int ret = 0;
    bool vout_on_int_state;

    charge_service_t* data = (charge_service_t*)zone->ops_data;

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_ONLINE, (unsigned long)((uintptr_t)&vout_on_int_state));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", ret);
        return ret;
    }

    return vout_on_int_state;
}
static int get_buck_vindpm_int_state(charge_zone_t* zone)
{
    int ret = 0;
    int buck_vindpm_int_state;

    charge_service_t* data = (charge_service_t*)zone->ops_data;

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_BUCK], BATIOC_GET_PROTOCOL,
        (unsigned long)((uintptr_t)&buck_vindpm_int_state));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", ret);
        return ret;
    }

    return buck_vindpm_int_state;
}

static int no_stand_soft_start_handle(charge_zone_t* zone, int work_chip_type, int work_cur)
{
    int buck_vindpm_int_state = 0;
    int set_work_cur;
    uint8_t buck_vindpm_int_count = 0;

    //clear interrput flag
    buck_vindpm_int_state = get_buck_vindpm_int_state(zone);
    ctrol_chip_current(zone, work_chip_type, work_cur);
    ctrol_chip_work(zone, work_chip_type, true);
    usleep((BUCK_VINDPM_DELAY_MS)*2);
    buck_vindpm_int_state = get_buck_vindpm_int_state(zone);
    if ((buck_vindpm_int_state & 0x0200) >> 9) {
        chargeinfo("set cur:50ma\n");
        work_cur = NO_STAND_CURRENT_50_MA;
        ctrol_chip_current(zone, work_chip_type, work_cur);
    }
    if (!(buck_vindpm_int_state & 0x01)) {
        chargeinfo("set cur:200ma\n");
        work_cur = NO_STAND_CURRENT_200_MA;
        ctrol_chip_current(zone, work_chip_type, work_cur);
        usleep(BUCK_VINDPM_DELAY_MS);
        buck_vindpm_int_state = get_buck_vindpm_int_state(zone);
        if (!(buck_vindpm_int_state & 0x01)) {
            chargeinfo("set cur:300ma\n");
            work_cur = NO_STAND_CURRENT_300_MA;
            ctrol_chip_current(zone, work_chip_type, work_cur);
            usleep(BUCK_VINDPM_DELAY_MS);
            buck_vindpm_int_state = get_buck_vindpm_int_state(zone);
            if (!(buck_vindpm_int_state & 0x01)) {
                chargeinfo("set cur:400ma\n");
                work_cur = NO_STAND_CURRENT_400_MA;
                ctrol_chip_current(zone, work_chip_type, work_cur);
                usleep(BUCK_VINDPM_DELAY_MS);
                buck_vindpm_int_state = get_buck_vindpm_int_state(zone);
                if (!(buck_vindpm_int_state & 0x01)) {
                    chargeinfo("set cur:500ma\n");
                    work_cur = NO_STAND_CURRENT_500_MA;
                    ctrol_chip_current(zone, work_chip_type, work_cur);
                    usleep(BUCK_VINDPM_DELAY_MS);
                    buck_vindpm_int_state = get_buck_vindpm_int_state(zone);
                    if (!(buck_vindpm_int_state & 0x01)) {
                    } else {
                        buck_vindpm_int_count = (buck_vindpm_int_state >> 1);
                        if (buck_vindpm_int_count >= BUCK_VINDPM_INT_VALUE) {
                            chargeinfo("set back cur:400ma\n");
                            work_cur = NO_STAND_CURRENT_400_MA;
                            ctrol_chip_current(zone, work_chip_type, work_cur);
                        }
                    }
                } else {
                    buck_vindpm_int_count = (buck_vindpm_int_state >> 1);
                    if (buck_vindpm_int_count >= BUCK_VINDPM_INT_VALUE) {
                        chargeinfo("set back cur:300ma\n");
                        work_cur = NO_STAND_CURRENT_300_MA;
                        ctrol_chip_current(zone, work_chip_type, work_cur);
                    }
                }
            } else {
                buck_vindpm_int_count = (buck_vindpm_int_state >> 1);
                if (buck_vindpm_int_count >= BUCK_VINDPM_INT_VALUE) {
                    chargeinfo("set back cur:200ma\n");
                    work_cur = NO_STAND_CURRENT_200_MA;
                    ctrol_chip_current(zone, work_chip_type, work_cur);
                }
            }
        } else {
            buck_vindpm_int_count = (buck_vindpm_int_state >> 1);
            if (buck_vindpm_int_count >= BUCK_VINDPM_INT_VALUE) {
                chargeinfo("set back cur:100ma\n");
                work_cur = NO_STAND_CURRENT_100_MA;
                ctrol_chip_current(zone, work_chip_type, work_cur);
            }
        }
    }
    set_work_cur = work_cur;
    return set_work_cur;
}

static int ctrol_wpc_rx_out(charge_zone_t* zone, uint16_t vol_base, uint16_t vol_step, bool* pump_status)
{
    int ret = 0;
    int rx_vout = 0;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("wpc is not init \n");
        return -1;
    }
    rx_vout = vol_base * 1.91 + PUMP_CONF_VOUT_OFFSET + vol_step;
    chargedebug("vol_base = %d vol_step = %d rx_vout=%d\n", vol_base, vol_step, rx_vout);
    if (rx_vout > PUMP_CONF_VOUT_MAX) {
        chargeerr("rx_vout = %d over %d\n", rx_vout, PUMP_CONF_VOUT_MAX);
        *pump_status = true;
    }
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_VOLTAGE, (unsigned long)((uintptr_t)&rx_vout));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_VOLTAGE) failed: %d\n", ret);
        return ret;
    }
#if 0
    usleep(500000);
    ret = zone->ops->get_wpc_rx_voltage(zone, (uint16_t *)&rx_vout);
    if (ret < 0) {
        chargeerr("get_wpc_rx_voltage failed: %d\n", ret);
        return ret;
    }
    chargedebug("=======rx out vol=%d mv\n", rx_vout);
#endif
    return 0;
}
static int get_pump_vbus_error_state(charge_zone_t* zone, bool* state)
{
    int ret = 0;
    int pump_state;
    bool vbus_errorhi_state;
    bool vbus_errorlo_state;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_PUMP])) {
        chargeerr("pump is not init \n");
        return -1;
    }

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_PUMP], BATIOC_STATE, (unsigned long)((uintptr_t)&pump_state));
    if (ret < 0) {
        chargeerr("ERROR: ioctl(BATIOC_STATE) failed: %d\n", ret);
        return ret;
    }

    vbus_errorlo_state = (pump_state & VBUS_ERRORLO_STAT_MASK) >> PUMP_VBUS_ERRORLO_STAT_SHIFT;
    vbus_errorhi_state = (pump_state & VBUS_ERRORHI_STAT_MASK) >> PUMP_VBUS_ERRORHI_STAT_SHIFT;

    if (vbus_errorlo_state || vbus_errorhi_state) {
        *state = false;
    } else {
        *state = true;
    }
    return ret;
}
static int get_pump_charge_en_state(charge_zone_t* zone, bool* pump_vbat_ovp_state, bool* pump_vus_ovp_state)
{
    int ret = 0;
    int pump_state;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_PUMP])) {
        chargeerr("pump is not init \n");
        return -1;
    }
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_PUMP], BATIOC_STATE, (unsigned long)((uintptr_t)&pump_state));
    if (ret < 0) {
        chargeerr("ERROR: ioctl(BATIOC_STATE) failed: %d\n", ret);
        return ret;
    }
    chargedebug("pump_state = 0x%X\n", pump_state);
    *pump_vbat_ovp_state = (pump_state & VBAT_OVP_MASK) >> PUMP_VBAT_OVP_SHIFT;
    *pump_vus_ovp_state = (pump_state & VBUS_OVP_MASK) >> PUMP_VBUS_OVP_SHIFT;

    chargedebug("pump  charge en value is %d\n", ((pump_state & CHG_EN_STAT_MASK) ? 1 : 0));
    return (pump_state & CHG_EN_STAT_MASK) ? 1 : 0;
}
static int set_wireless_rx_vout_balance(charge_zone_t* zone, bool balance)
{
    int ret;
    int rx_vout;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("wpc is not init \n");
        return -1;
    }

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_GET_VOLTAGE, (unsigned long)((uintptr_t)&rx_vout));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_GET_VOLTAGE) failed: %d\n", ret);
        return ret;
    }

    if (balance) {
        rx_vout += PUMP_CONF_VOUT_STEP_INC;
    } else {
        rx_vout -= PUMP_CONF_VOUT_STEP_DEC;
    }

    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_VOLTAGE, (unsigned long)((uintptr_t)&rx_vout));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_VOLTAGE) failed: %d\n", ret);
        return ret;
    }
#if 0
    uint16_t rx_out_real;
    usleep(500000);
    ret = zone->ops->get_wpc_rx_voltage(zone, &rx_out_real);
    if (ret < 0) {
        chargeerr("get_wpc_rx_voltage failed: %d\n", ret);
        return ret;
    }
    chargedebug("=======rx set=%d out=%d mv\n", rx_vout, rx_out_real);
#endif
    return ret;
}
static int get_cutoff_condition(charge_zone_t* zone)
{
    static int cutoff_counter = 0;
    if (NULL == zone) {
        return -1;
    }

    if ((zone->batty_info.electricity == CUTOFF_ELECTRICITY_PERCENT) && (zone->batty_info.current == CUTOFF_CURRENT)) {
        cutoff_counter++;
    } else {
        cutoff_counter = 0;
    }

    if (cutoff_counter == 3) {
        cutoff_counter = 0;
        return 0;
    }

    return 1;
}

static int notify_rx_insert_out(charge_zone_t* zone)
{
    int ret = 0;
    FAR struct batio_operate_msg_s msg;
    charge_service_t* data = (charge_service_t*)zone->ops_data;

    if ((data == NULL) || (0 > data->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("wpc is not init \n");
        return -1;
    }
    msg.operate_type = BATIO_OPRTN_CHARGE;
    ret = ioctl(data->dev_fd[CHARGE_DEVICE_WPC], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    if (ret < 0) {
        chargeerr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", ret);
        return ret;
    }
    return ret;
}
static int quit_wpc_rx(charge_zone_t* zone)
{
    zone->temperature_protect = 0;
    zone->temperature_protect_change = false;
    zone->adapter = WPC_ADAPTER_STATE_NULL;
    zone->adapter_change = false;
    zone->wpc_rx_change = false;
    zone->charge_chip_type = CHARHE_CHIP_TYPE_NULL;
    zone->work_current = 0;
    zone->pump_work_state = PUMP_WORK_STATE_UNKNOWN;
    return 0;
}
