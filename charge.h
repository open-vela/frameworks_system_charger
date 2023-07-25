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

#ifndef __CHARGE_H__
#define __CHARGE_H__

#include <assert.h>
#include <ctype.h>
#include <debug.h>
#include <errno.h>
#include <limits.h>
#include <nuttx/compiler.h>
#include <nuttx/config.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* charge work chip define */
typedef enum {
    CHARHE_CHIP_TYPE_NULL = -1,
    CHARHE_CHIP_TYPE_BUCK = 0,
    CHARHE_CHIP_TYPE_PUMP,
    CHARHE_CHIP_TYPE_MAX
} charge_chip_type_e;

/* WPC RX state define */
typedef enum {
    WPC_RX_STATE_NULL = -1,
    WPC_RX_STATE_QUITE = 0,
    WPC_RX_STATE_ENTER,
    WPC_RX_STATE_MAX
} wpc_rx_state_e;

/* WPC RX state define */
typedef enum {
    WPC_ADAPTER_STATE_NULL = -1,
    WPC_ADAPTER_STATE_NO_TYPE1 = 0,
    WPC_ADAPTER_STATE_NO_TYPE2,
    WPC_ADAPTER_STATE_NO_TYPE3,
    WPC_ADAPTER_STATE_YES,
    WPC_ADAPTER_STATE_MAX
} wpc_adapter_state_e;

/* batty infomation */
typedef struct batty_info {
    uint16_t voltage;
    int16_t current;
    uint16_t electricity;
    int16_t temperature;
} batty_info_t;

struct charge_zone_device_ops;

/* temperature charge used */
typedef struct charge_temperature {
    float shell_temp;
} charge_temperature_t;

typedef struct charge_zone {
    /* data */
    int voltage;
    bool wpc_rx_change;
    int temperature_protect;
    bool temperature_protect_change;
    int work_current;
    bool adapter_change;
    int pump_work_state;
    bool safetimer_en;
    bool shell_temp_ready;

    batty_info_t batty_info;
    charge_temperature_t charge_temperature;
    wpc_rx_state_e wpc_rx_state;
    wpc_adapter_state_e adapter;
    charge_chip_type_e charge_chip_type;

    /*ops*/
    struct charge_zone_device_ops* ops;
    void* ops_data; //to do
} charge_zone_t;
/* charge zone device operations */
typedef struct charge_zone_device_ops {
    int (*get_cur_voltage_adc)(charge_zone_t*);
    int (*get_cur_charge_temperature)(charge_zone_t*);
    int (*get_temperature_protect)(charge_zone_t*);
    int (*get_temperature_protect_change)(charge_zone_t*);
    int (*get_wpc_rx_state)(charge_zone_t*);
    int (*get_wpc_rx_change)(charge_zone_t*);
    int (*get_wpc_rx_voltage)(charge_zone_t*, uint16_t*);
    int (*get_work_chip_change)(charge_zone_t*);
    int (*get_cur_adapter)(charge_zone_t*);
    int (*get_adapter_change)(charge_zone_t*);
    int (*get_cur_batty_info)(charge_zone_t*);
    int (*get_pump_adapter_insert_state)(charge_zone_t*);
    /* charge chip operations */
    int (*ctrol_chip_work)(charge_zone_t*, int, bool);
    int (*ctrol_chip_current)(charge_zone_t*, int, int);
    int (*ctrol_wpc_sleep)(charge_zone_t*, bool, int);
    int (*ctrol_wpc_rx_out)(charge_zone_t*, uint16_t, uint16_t, bool*);
    int (*set_pump_adc)(charge_zone_t*, bool);
    int (*get_pump_vbus_error_state)(charge_zone_t*, bool*);
    int (*get_pump_charge_en_state)(charge_zone_t*, bool*, bool*);
    int (*set_wireless_rx_vout_balance)(charge_zone_t*, bool);
    int (*notify_rx_insert_out)(charge_zone_t*);
    int (*quit_wpc_rx)(charge_zone_t*);
    int (*quit_charge)(charge_zone_t*);
    int (*get_wpc_vout_on_int_state)(charge_zone_t*);
    int (*get_cutoff_condition)(charge_zone_t*);
    int (*get_buck_vindpm_int_state)(charge_zone_t*);
    int (*no_stand_soft_start_handle)(charge_zone_t*, int, int);
    int (*ctrol_buck_hiz)(charge_zone_t*, bool);
    int (*ctrol_buck_safetimer)(charge_zone_t*, bool);
} charge_zone_device_ops_t;

#endif /* __CHARGE_H__ */
