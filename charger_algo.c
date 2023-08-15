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

#include "charger_algo.h"
#include "charger_hwintf.h"
#include "charger_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define BIT(n) (1U << (n))
#define BUCK_ALGO_INIT_VOL 3000
#define MAX_ALGO_NUM 5

#define VBAT_OVP_MASK BIT(0)
#define IBAT_OCP_MASK BIT(1)
#define VBUS_OVP_MASK BIT(2)
#define IBUS_OCP_MASK BIT(3)
#define IBUS_UCP_MASK BIT(4)
#define ADAPTER_INSERT_MASK BIT(5)
#define VBAT_INSERT_MASK BIT(6)
#define ADC_DONE_MASK BIT(7)
#define VBUS_ERRORLO_STAT_MASK BIT(8)
#define VBUS_ERRORHI_STAT_MASK BIT(9)
#define CP_SWITCHING_STAT_MASK BIT(10)
#define CHG_EN_STAT_MASK BIT(11)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int buck_algo_start(struct charger_algo* algo);
static int buck_algo_update(struct charger_algo* algo, struct charger_plot_parameter* pa);
static int buck_algo_stop(struct charger_algo* algo);
static int pump_algo_start(struct charger_algo* algo);
static int pump_algo_update(struct charger_algo* algo, struct charger_plot_parameter* pa);
static int pump_algo_stop(struct charger_algo* algo);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct charger_algo_ops buck_algo = {
    .start = buck_algo_start,
    .update = buck_algo_update,
    .stop = buck_algo_stop,
};

static struct charger_algo_ops pump_algo = {
    .start = pump_algo_start,
    .update = pump_algo_update,
    .stop = pump_algo_stop,
};

static struct charger_algo_class algo_tlbs[MAX_ALGO_NUM] = {
    { "buck", &buck_algo },
    { "pump", &pump_algo },
    { NULL, NULL },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int is_pa_changed(struct charger_plot_parameter* spa, struct charger_plot_parameter* dpa)
{
    return memcmp(spa, dpa, sizeof(struct charger_plot_parameter));
}

static int buck_algo_start(struct charger_algo* algo)
{
    int ret;

    chargerinfo("buck algo start\n");
    if (is_supply_exist()) {
        ret = set_supply_voltage(algo->cm, BUCK_ALGO_INIT_VOL);
        if (ret < 0) {
            chargererr("set supply voltage %d failed\n", BUCK_ALGO_INIT_VOL);
            return CHARGER_FAILED;
        }
    }
    ret = enable_charger(algo->cm, algo->index, true);
    if (ret < 0) {
        chargererr("enable charger %d failed\n", algo->index);
        return CHARGER_FAILED;
    }
    memset(&algo->sp, 0, sizeof(struct charger_plot_parameter));
    return CHARGER_OK;
}

static int buck_algo_update(struct charger_algo* algo, struct charger_plot_parameter* pa)
{
    int ret = CHARGER_OK;

    if (pa && is_pa_changed(&algo->sp, pa)) {
        chargerinfo("buck_algo_update t_min:%d t_max:%d v_min:%d v_max:%d index:%d"
                    "current:%d supply_vol:%d\n",
            pa->temp_range_min, pa->temp_range_max,
            pa->vol_range_min, pa->vol_range_max, pa->charger_index,
            pa->work_current, pa->supply_vol);

        ret = set_charger_current(algo->cm, pa->charger_index, pa->work_current);
        if (ret < 0) {
            chargererr("enable charger %d failed\n", algo->index);
            return CHARGER_FAILED;
        }

        if (pa->supply_vol > 0) {
            if (is_supply_exist()) {
                ret = set_supply_voltage(algo->cm, pa->supply_vol);
                if (ret < 0) {
                    chargererr("set supply %d failed\n", pa->supply_vol);
                    return CHARGER_FAILED;
                }
            }
        }
        memcpy(&algo->sp, pa, sizeof(struct charger_plot_parameter));
    }
    return CHARGER_OK;
}

static int buck_algo_stop(struct charger_algo* algo)
{
    int ret;

    chargerinfo("buck algo stop\n");
    ret = enable_charger(algo->cm, algo->index, false);
    if (ret < 0) {
        chargererr("disable charger %d failed\n", algo->index);
        return CHARGER_FAILED;
    }

    if (is_supply_exist()) {
        ret = set_supply_voltage(algo->cm, BUCK_ALGO_INIT_VOL);
        if (ret < 0) {
            chargererr("set supply %d failed\n", BUCK_ALGO_INIT_VOL);
            return CHARGER_FAILED;
        }
    }

    return CHARGER_OK;
}

static int pump_algo_start(struct charger_algo* algo)
{
    int voltage = 0;
    int current = 0;
    int vbase = 0;
    int rx_vout = 0;
    unsigned int errstate = 0;
    unsigned int enstate = 0;
    int psvc = 0;
    int ret = 0;

    chargerinfo("pump algo start\n");
    ret = get_battery_voltage(algo->cm, &voltage);
    ret |= get_battery_current(algo->cm, &current);
    if (ret < 0) {
        chargererr("get battery info failed\n");
        return CHARGER_FAILED;
    }
    vbase = voltage - current * 0.25;
    do {
        rx_vout = vbase * 1.91 + PUMP_CONF_VOUT_OFFSET + (PUMP_CONF_STARTUP_VOLTAGE + PUMP_CONF_STARTUP_VOLTAGE_OFFSET * psvc);
        if (rx_vout > PUMP_CONF_VOUT_MAX) {
            chargererr("rx_vout = %d over %d\n", rx_vout, PUMP_CONF_VOUT_MAX);
            return CHARGER_FAILED;
        }
        ret = set_supply_voltage(algo->cm, rx_vout);
        usleep(1000 * 100);
        ret |= get_charger_state(algo->cm, algo->index, &errstate);
        if (ret < 0) {
            chargererr("get charger error state failed\n");
            return CHARGER_FAILED;
        }
        psvc++;
    } while (errstate & (VBUS_ERRORLO_STAT_MASK | VBUS_ERRORHI_STAT_MASK));
    ret = enable_charger(algo->cm, algo->index, true);
    usleep(1000 * 500);
    ret |= get_charger_state(algo->cm, algo->index, &enstate);
    enstate = enstate & CHG_EN_STAT_MASK;
    if (ret < 0 || !enstate) {
        enable_charger(algo->cm, algo->index, false);
        return CHARGER_FAILED;
    }
    memset(&algo->sp, 0, sizeof(struct charger_plot_parameter));
    return CHARGER_OK;
}

static int pump_algo_update(struct charger_algo* algo, struct charger_plot_parameter* pa)
{
    unsigned int state = 0;
    unsigned int ovp;
    unsigned int enstate;
    int current = 0;
    int vol = 0;
    int ret = 0;

    if (pa) {
        ret = get_charger_state(algo->cm, algo->index, &state);
        enstate = state & CHG_EN_STAT_MASK;
        ovp = state & (VBAT_OVP_MASK | VBUS_OVP_MASK);
        if (ret < 0 || !enstate || ovp) {
            return CHARGER_FAILED;
        }

        if (is_pa_changed(&algo->sp, pa)) {
            chargerinfo("pump_algo_update t_min:%d t_max:%d v_min:%d v_max:%d index:%d"
                        "current:%d supply_vol:%d\n",
                pa->temp_range_min, pa->temp_range_max,
                pa->vol_range_min, pa->vol_range_max, pa->charger_index,
                pa->work_current, pa->supply_vol);

            memcpy(&algo->sp, pa, sizeof(struct charger_plot_parameter));
            return set_charger_current(algo->cm, algo->index, pa->work_current);
        } else {
            ret = get_battery_current(algo->cm, &current);
            chargerassert_return(ret < 0, "pump_algo_update get battery current failed\n");
        }

        if (current < (pa->work_current - PUMP_CONF_COUT_STEP_DEC)) {
            if (get_supply_voltage(algo->cm, &vol) < 0) {
                return CHARGER_FAILED;
            }
            vol += PUMP_CONF_VOUT_STEP_INC;
            if (vol > PUMP_CONF_VOUT_MAX) {
                vol = PUMP_CONF_VOUT_MAX;
            }
            if (set_supply_voltage(algo->cm, vol) < 0) {
                return CHARGER_FAILED;
            }
        }
        if (current > (pa->work_current + PUMP_CONF_COUT_STEP_INC)) {
            if (get_supply_voltage(algo->cm, &vol) < 0) {
                return CHARGER_FAILED;
            }
            vol -= PUMP_CONF_VOUT_STEP_DEC;
            if (set_supply_voltage(algo->cm, vol) < 0) {
                return CHARGER_FAILED;
            }
        }
    }
    return CHARGER_OK;
}

static int pump_algo_stop(struct charger_algo* algo)
{
    int ret;

    chargerinfo("pump algo stop\n");
    ret = enable_charger(algo->cm, algo->index, false);
    if (ret < 0) {
        chargererr("disable charger %d failed\n", algo->index);
        return CHARGER_FAILED;
    }

    if (is_supply_exist()) {
        ret = set_supply_voltage(algo->cm, PUMP_CONF_STARTUP_VOLTAGE);
        if (ret < 0) {
            chargererr("set supply %d failed\n", PUMP_CONF_STARTUP_VOLTAGE);
            return CHARGER_FAILED;
        }
    }

    return CHARGER_OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

struct charger_algo_ops* get_algo_ops(char* id)
{
    int i = 0;
    struct charger_algo_class* algo = NULL;

    for (i = 0; i < MAX_ALGO_NUM; i++) {
        algo = &algo_tlbs[i];
        if (NULL != algo->algo_id && 0 == strcmp(id, algo->algo_id)) {
            return algo->ops;
        }
    }
    return NULL;
}
