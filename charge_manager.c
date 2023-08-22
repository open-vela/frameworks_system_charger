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
#include "charge_configuration.h"
#include "charge_zone_user.h"
#include <debug.h>
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
#include <syslog.h>
#include <system/state.h>
#include <time.h>
#include <uORB/uORB.h>
#include <unistd.h>
#include <inttypes.h>
#include <mqueue.h>
#include <nuttx/power/battery_charger.h>
#include <nuttx/power/battery_gauge.h>
#include <nuttx/power/battery_ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef CONFIG_PM
#include <nuttx/power/pm.h>
#endif
/****************************************************************************
 * Define
 ****************************************************************************/
#define MQ_MSG_NAME "chargerD"
#define MQ_MSG_LOAD_MAX (10)
#define TASK_NAME "charger manager"
#define TASK_PRIORITY (100)
#define TASK_STACK_SIZE (2048)

#define PUMP_ADJUST_INTERVAL_MS 1000
#define BATTYINFO_PRINT_PER_SECS 60
#define MONITOR_INTERVAL_SECS 1
#define CUTOFF_RELEASE_INTERVAL_SECS (60 * 60) / MONITOR_INTERVAL_SECS

#ifdef CONFIG_FACTEST_CHARGE
#define MQ_FAC_BUCK_CHARGE_NAME "facBuckCharge"
#endif

#define DELAY_MS(x) (usleep(x * 1000L))
#define MS_TO_CLOCKT(mst) ((mst) * (CLOCKS_PER_SEC / 1000L))
#define CHARGE_WORKQ (LPWORK)
#define WATI_MAX_S 6

#if defined(CONFIG_PM)
PM_WAKELOCK_DECLARE_STATIC(g_pm_wakelock_charge, "charge", PM_IDLE_DOMAIN, PM_NORMAL);
#endif

/****************************************************************************
 * Static Define
 ****************************************************************************/
typedef struct {
    charge_mode_e mode;
    uint32_t time_gap;
} charge_msg_t;

const char* charge_device_path[CHARGE_DEVICE_MAX] = {
    "/dev/charge/batt_charger",
    "/dev/charge/charger_pump",
    "/dev/charge/wireless_rx",
    "/dev/charge/batt_gauge",
};

/****************************************************************************
 * Static Function
 ****************************************************************************/
static int send_charge_msg(charge_msg_t msg);
static int charge_service_thread(int argc, FAR char* argv[]);
static charge_service_t* get_instance(void) { return &charge_service_data; }

static int env_timer_cb(void)
{
    charge_msg_t msg;

#ifdef CONFIG_FACTEST_CHARGE
    if (get_instance()->fac_buck_charge_flag) {
        get_instance()->fac_buck_charge_count++;
        if (get_instance()->fac_buck_charge_count == 20) {
            syslog(LOG_DEBUG, "get_instance()->fac_buck_charge_count: time out\n");
            get_instance()->fac_buck_charge_count = 0;
            get_instance()->fac_buck_charge_flag = false;
        }
        goto end;
    }
#endif

    if (charge_service_data.mode == CHARGE_MODE_PUMP_WORK) {
        if (get_instance()->pump_switch_locked == false) {
            msg.mode = CHARGE_MODE_PUMP_WORK;
            send_charge_msg(msg);
        } else {
            chargedebug("pump_switch_locked\r\n");
        }
    } else if (get_instance()->monitor_lock == false) {
        msg.mode = CHARGE_MODE_MONITOR;
        send_charge_msg(msg);
    }

    if (get_instance()->cutoff_lock) {
        get_instance()->cutoff_timer_counter++;
        if (get_instance()->cutoff_timer_counter == CUTOFF_RELEASE_INTERVAL_SECS) {
            chargeinfo("cutoff release msg send\n");
            msg.mode = CHARGE_MODE_CUTOFF_RELEASE;
            send_charge_msg(msg);
            get_instance()->cutoff_timer_counter = 0;
            get_instance()->cutoff_lock = false;
        }
    }

#ifdef CONFIG_FACTEST_CHARGE
end:
#endif

    return 0;
}

static int env_timer_start(timer_t* timer)
{
    int ret;

    struct sigevent evp;
    struct itimerspec it;

    memset(&evp, 0, sizeof(struct sigevent));
    evp.sigev_value.sival_int = 122;
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_notify_function = (void*)env_timer_cb;

    it.it_interval.tv_sec = MONITOR_INTERVAL_SECS;
    it.it_interval.tv_nsec = 0;
    it.it_value.tv_sec = MONITOR_INTERVAL_SECS;
    it.it_value.tv_nsec = 0;

    if (*timer != 0) {
        chargeerr("timer has created\r\n");
        return 0;
    }
    ret = timer_create(CLOCK_REALTIME, &evp, timer);
    if (ret != 0) {
        chargeerr("timer_create fail: %d\r\n", ret);
        return -1;
    }

    ret = timer_settime(*timer, 0, &it, 0);

    if (ret != 0) {
        chargeerr("timer_start fail: %d\r\n", ret);
        return -1;
    }
    chargeinfo("timer_create success!\n");
    return 0;
}

static int env_timer_quit(timer_t* timer)
{
    int ret = 0;
    if (*timer != 0) {
        ret = timer_delete(*timer);
        if (ret != 0) {
            chargeerr("timer_delete fail: %d\r\n", ret);
            return -1;
        } else {
            *timer = 0;
            chargeinfo("timer_delete success!\n");
        }
    } else {
        chargeinfo("timer_create null!\n");
    }
    return 0;
}

static int charge_mode_init_cb(charge_service_t* cs)
{
    chargedebug("====================charge_zone_use.wpc_rx_state:%d\n", charge_zone_use.wpc_rx_state);
    for (uint8_t i = CHARGE_DEVICE_BUCK; i < CHARGE_DEVICE_MAX; i++) {
        cs->dev_fd[i] = open(charge_device_path[i], O_RDONLY);
        if (-1 == cs->dev_fd[i]) {
            chargeerr("%s:open %s failed;\r\n", __FUNCTION__, charge_device_path[i]);
            return -1;
        }
    }

    cs->temperature_uorb_fd = orb_subscribe(ORB_ID(device_temperature));
    if (cs->temperature_uorb_fd < 0) {
        chargeerr("Uorb subscribec failed: %d\n", cs->temperature_uorb_fd);
        return -1;
    }

    if (charge_zone_use.ops->set_pump_adc(&charge_zone_use, false)) {
        chargeerr("close pump adc failed\n");
        return -1;
    }

    chargeinfo("charge_mode_init success\n");
    return 0;
}

static void charge_mode_init_release(charge_service_t* cs)
{
    //release devide fd
    for (uint8_t i = CHARGE_DEVICE_BUCK; i < CHARGE_DEVICE_MAX; i++) {
        if (0 < cs->dev_fd[i]) {
            close(cs->dev_fd[i]);
        }
    }
    //release uorb subscribe
    if (cs->temperature_uorb_fd > 0) {
        orb_unsubscribe(cs->temperature_uorb_fd);
    }
}

static int send_charge_msg(charge_msg_t msg)
{
    mqd_t mq;
    struct mq_attr attr;
    int ret = 0;

    attr.mq_maxmsg = MQ_MSG_LOAD_MAX;
    attr.mq_msgsize = sizeof(charge_msg_t);
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;
    mq = mq_open(MQ_MSG_NAME, O_RDWR | O_CREAT | O_NONBLOCK, 0644, &attr);
    if (mq == (mqd_t)-1) {
        /* Unable to open message queue! */
        chargeerr("%s:open mq failed;\r\n", __FUNCTION__);
        return -1;
    }
    ret = mq_send(mq, (const char*)&msg, sizeof(msg), 1);
    chargedebug("%s: msg.mode = %d ret = %d;\r\n", __FUNCTION__, msg.mode, ret);
    mq_close(mq);
    return ret;
}

static int init_service(void)
{
    int ret = 0;
    charge_msg_t msgsend;

    if (0 >= get_instance()->service_pid) {
        msgsend.mode = CHARGE_MODE_INIT;
        send_charge_msg(msgsend);

        ret = task_create(TASK_NAME, TASK_PRIORITY, TASK_STACK_SIZE,
            charge_service_thread, NULL);
        if (ret <= 0) {
            chargeerr("%s:creat task failed;\r\n", __FUNCTION__);
            return -1;
        }
        get_instance()->service_pid = ret;
    }
    return 0;
}

static int batty_info_check(bool adapter_check)
{
    charge_msg_t msg;
    static uint16_t batty_info_print = 0;

    if (0 == charge_zone_use.ops->get_cur_batty_info(&charge_zone_use)) {
        if ((++batty_info_print) % BATTYINFO_PRINT_PER_SECS == 0) {
            chargeinfo("vol:%d cur:%d btmp:%0.1f chip:%s cus:%d stmp:%0.1f\r\n",
                charge_zone_use.batty_info.voltage,
                charge_zone_use.batty_info.current,
                ((float)charge_zone_use.batty_info.temperature) / TEMP_VALUE_GAIN,
                (charge_zone_use.charge_chip_type == 0) ? "Buck" : "Pump",
                charge_zone_use.work_current,
                ((float)charge_zone_use.charge_temperature.shell_temp) / TEMP_VALUE_GAIN);
        }
        //check adapter
        if (adapter_check && (false == get_instance()->temp_protect_lock)) {
            if (charge_zone_use.ops->get_cur_adapter(&charge_zone_use)) {
                chargeerr("get_cur_adapter failed\r\n");
            }
        }

        //check temperature protect state
        if (charge_zone_use.ops->get_temperature_protect_change(&charge_zone_use)) {
            msg.mode = (charge_zone_use.ops->get_temperature_protect(&charge_zone_use)) ? CHARGE_MODE_TEMP_PROTECT : CHARGE_MODE_TEMP_PROTECT_QUIT;
            chargedebug("=====%d--mode = %d\n", __LINE__, msg.mode);
            send_charge_msg(msg);
        }

        //check work chip change
        if (get_instance()->temp_protect_lock == false) {
            if (0 == charge_zone_use.ops->get_work_chip_change(&charge_zone_use)) {
                msg.mode = CHARGE_MODE_UPDATE;
                send_charge_msg(msg);
            }
        }
    }
    return 0;
}

static int wpc_enter_cb(void)
{

#ifdef CONFIG_FACTEST_CHARGE
    if (get_instance()->fac_charge_aging_stop_flag) {
        return -1;
    };
#endif

    if (env_timer_start(&(get_instance()->env_timer_id)) < 0) {
        chargeerr("creat timer failed;\r\n");
        return -1;
    }
    return 0;
}

static int wpc_quit_cb(void)
{
    int ret = -1;
    //stop monitor work

#ifdef CONFIG_FACTEST_CHARGE
    if (get_instance()->fac_charge_aging_stop_flag) {
        return -1;
    }
#endif
    ret = env_timer_quit(&(get_instance()->env_timer_id));
    if (ret != 0) {
        chargeerr("timer cancel failed. ret=%d.\n", ret);
        return -1;
    }
    if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, CHARHE_CHIP_TYPE_NULL, true)) {
        chargeerr("close buck & pump failed;\r\n");
        return -1;
    }
    if (charge_zone_use.ops->ctrol_wpc_sleep(&charge_zone_use, false, WPC_SLEEP_NOT)) {
        chargeerr("set sleep low failed\r\n");
        return -1;
    }
    if (charge_zone_use.ops->notify_rx_insert_out(&charge_zone_use)) {
        chargeerr("notify_rx_insert_out failed\r\n");
        return -1;
    }
    charge_zone_use.ops->quit_wpc_rx(&charge_zone_use);
    get_instance()->pump_switch_locked = false;
    get_instance()->pump_open_fail_times = 0;
    get_instance()->temp_protect_lock = false;
    get_instance()->monitor_lock = false;
    get_instance()->cutoff_timer_counter = 0;
    get_instance()->cutoff_lock = false;

    return 0;
}

static int temp_protect_cb(void)
{
    get_instance()->temp_protect_lock = true;
    if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, CHARHE_CHIP_TYPE_NULL, true)) {
        chargeerr("close all charge chip failed;\r\n");
    }
#if 0
    uint16_t vxout = 0;
    if(charge_zone_use.ops->get_wpc_rx_voltage(&charge_zone_use, &vxout)){
        chargeerr("get_wpc_rx_voltage failed;\r\n");
    }
    chargedebug("---temp_protect 1 rx out = %d\n", vxout);
#endif
    if (charge_zone_use.ops->ctrol_wpc_sleep(&charge_zone_use, true, WPC_SLEEP_ENTER)) {
        chargeerr("ctrol_wpc_sleep on failed;\r\n");
    }
#if 0
    vxout = 0;
    if(charge_zone_use.ops->get_wpc_rx_voltage(&charge_zone_use, &vxout)){
        chargeerr("get_wpc_rx_voltage failed;\r\n");
    }
    chargedebug("---temp_protect 2 rx out = %d\n", vxout);
#endif
    return 0;
}

static int temp_protect_quit_cb(void)
{
    charge_msg_t msg;
    int i;

    if (charge_zone_use.ops->ctrol_wpc_sleep(&charge_zone_use, false, WPC_SLEEP_QUIT)) {
        chargeerr("ctrol_wpc_sleep off failed;\r\n");
    }

    for (i = 0; i < WATI_MAX_S; i++) {
        sleep(1);
        if ((charge_zone_use.ops->get_wpc_vout_on_int_state(&charge_zone_use)) || (charge_zone_use.wpc_rx_state == WPC_RX_STATE_QUITE)) {
            chargedebug("rx vout int ok or wpc rx state quit\r\n");
            break;
        }
    }

    if ((i == WATI_MAX_S) && (charge_zone_use.wpc_rx_state != WPC_RX_STATE_QUITE)) {
        chargeinfo("wait rx vout on int timeout when temp protect quit\r\n");
        msg.mode = CHARGE_MODE_WPC_QUIT;
        send_charge_msg(msg);
    }

#if 1
    uint16_t vxout = 0;
    if (charge_zone_use.ops->get_wpc_rx_voltage(&charge_zone_use, &vxout)) {
        chargeerr("get_wpc_rx_voltage failed;\r\n");
    }
    chargeinfo("---temp_protect quit 1 rx out = %dmv\n", vxout);
#endif
    get_instance()->pump_open_fail_times = 0;
    get_instance()->pump_switch_locked = false;
    get_instance()->temp_protect_lock = false;
    get_instance()->monitor_lock = false;
#if 0
    vxout = 0;
    if(charge_zone_use.ops->get_wpc_rx_voltage(&charge_zone_use, &vxout)){
        chargeerr("get_wpc_rx_voltage failed;\r\n");
    }
    chargedebug("---temp_protect quit 2 rx out = %d\n", vxout);
#endif
    //msg.mode = CHARGE_MODE_MONITOR;
    //send_charge_msg(msg);
    return 0;
}
static int pump_work_cb(void)
{
    charge_msg_t msg;
    bool pump_status = false;
    static bool pump_exp_flag = false;
    bool pump_vbat_ovp_state = false;
    bool pump_vus_ovp_state = false;
    bool pump_vbus_error_state = false;
    int ret = 0;
    static int pump_startup_voltage_count = 0;
    static int rx_open_fail_times = 0;
    uint8_t k = 0;
    static uint16_t batty_info_print = 0;

    if ((0 > get_instance()->dev_fd[CHARGE_DEVICE_PUMP]) || (0 > get_instance()->dev_fd[CHARGE_DEVICE_WPC])) {
        chargeerr("pump or wpc is not init \n");
        return -1;
    }
    //temperature check
    if (0 == charge_zone_use.ops->get_cur_charge_temperature(&charge_zone_use)) {
        chargedebug("charge_zone_use.charge_temperature.shell_temp=%f\r\n", charge_zone_use.charge_temperature.shell_temp);
        //check temperature protect state
        if (charge_zone_use.ops->get_temperature_protect_change(&charge_zone_use)) {
            msg.mode = (charge_zone_use.ops->get_temperature_protect(&charge_zone_use)) ? CHARGE_MODE_TEMP_PROTECT : CHARGE_MODE_TEMP_PROTECT_QUIT;
            rx_open_fail_times = 0;
            chargedebug("=====%d--mode = %d\n", __LINE__, msg.mode);
            send_charge_msg(msg);
            goto end;
        }
    }

    switch (charge_zone_use.pump_work_state) {
    case PUMP_WORK_STATE_START: {
        chargedebug("pump_state is %d\n", charge_zone_use.pump_work_state);
        chargedebug("batty_info.voltage= %d, batty_info.current= %d\n", charge_zone_use.batty_info.voltage, charge_zone_use.batty_info.current);
        uint16_t vbase = charge_zone_use.batty_info.voltage - charge_zone_use.batty_info.current * 0.25;
        chargeinfo("vbase = %d\n", vbase);
        ret = charge_zone_use.ops->ctrol_wpc_rx_out(&charge_zone_use, vbase,
            (PUMP_CONF_STARTUP_VOLTAGE + PUMP_CONF_STARTUP_VOLTAGE_OFFSET * pump_startup_voltage_count), &pump_status);
        if (ret < 0) {
            ++rx_open_fail_times;
            chargeerr("pump rx open fail %d\n", rx_open_fail_times);
            if (rx_open_fail_times >= 3) {
                charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                pump_exp_flag = true;
                goto change_state;
            } else {
                goto change_state;
            }
        }

        if (pump_status) {
            chargeerr("pump status is %d\n", pump_status);
            charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
            pump_exp_flag = true;
            goto change_state;
        }

        DELAY_MS(100);
        charge_zone_use.ops->get_pump_vbus_error_state(&charge_zone_use, &pump_vbus_error_state);
        if (pump_vbus_error_state == true) {
#if 0
                if(charge_zone_use.ops->set_pump_adc(&charge_zone_use, true)){
                    chargeerr("open pump adc failed\n");
                    charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                    pump_exp_flag = true;
                    goto change_state;
                }
#endif
            for (k = 0; k < 3; k++) {
                if (charge_zone_use.ops->get_pump_adapter_insert_state(&charge_zone_use)) {
                    break;
                } else {
                    DELAY_MS(1000);
                    chargeinfo("===============get adapter_insert times=%d\n", k);
                }
            }
            if (k == 3) {
                chargeinfo("===============vbat_insert false\n");
                charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                pump_exp_flag = true;
                goto change_state;
            }
            chargeinfo("open pump\n");
            if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, charge_zone_use.charge_chip_type, true)) {
                chargeerr("en pump fail\n");
                charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                pump_exp_flag = true;
                goto change_state;
            }
            DELAY_MS(500);
            if (charge_zone_use.ops->get_pump_charge_en_state(&charge_zone_use, &pump_vbat_ovp_state, &pump_vus_ovp_state) == true) {
                chargeinfo("-------open pump successed!!!\n");

                DELAY_MS(100);
                chargeinfo("the current should be %dmA\n", charge_zone_use.work_current);
                charge_zone_use.ops->ctrol_chip_current(&charge_zone_use, charge_zone_use.charge_chip_type, charge_zone_use.work_current); //to do
                get_instance()->pump_open_fail_times = 0;
                pump_startup_voltage_count = 0;
                charge_zone_use.pump_work_state = PUMP_WORK_STATE_SET;
                goto change_state;
            } else {
                if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, charge_zone_use.charge_chip_type, false)) {
                    chargeerr("close chip-%d failed;\r\n", charge_zone_use.charge_chip_type);
                    charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                    pump_exp_flag = true;
                    goto change_state;
                }
                if (pump_vus_ovp_state || pump_vbat_ovp_state) {
                    chargeinfo("pump vat ovp falg true\n");
                    charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                    pump_exp_flag = true;
                    goto change_state;
                } else {
                    chargeinfo("===========pump open fail========\n");
                    charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                    pump_exp_flag = true;
                    goto change_state;
                }
            }
        } else {
            chargeinfo("pump_startup_voltage_count is %d\n", pump_startup_voltage_count);
            pump_startup_voltage_count++;
        }
    } break;

    case PUMP_WORK_STATE_SET: {
        chargedebug("pump_state is %d\n", charge_zone_use.pump_work_state);
        if (charge_zone_use.ops->get_pump_charge_en_state(&charge_zone_use, &pump_vbat_ovp_state, &pump_vus_ovp_state) == false) {
            chargeerr("Set: get pump charge en false\n");
            if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, charge_zone_use.charge_chip_type, false)) {
                chargeerr("close pump fail\n");
                charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
                pump_exp_flag = true;
                goto change_state;
            }
            charge_zone_use.pump_work_state = PUMP_WORK_STATE_START;
            goto change_state;
        }

        if (pump_vus_ovp_state || pump_vbat_ovp_state) {
            chargeerr("pump vat ovp falg true\n");
            charge_zone_use.pump_work_state = PUMP_WORK_STATE_EXP;
            pump_exp_flag = true;
            goto change_state;
        }

        if (charge_zone_use.batty_info.current < (charge_zone_use.work_current - PUMP_CONF_COUT_STEP_DEC)) {
            ret = charge_zone_use.ops->set_wireless_rx_vout_balance(&charge_zone_use, true);
            if (ret < 0) {
                goto change_state;
            }
        }

        if (charge_zone_use.batty_info.current > (charge_zone_use.work_current + PUMP_CONF_COUT_STEP_INC)) {
            ret = charge_zone_use.ops->set_wireless_rx_vout_balance(&charge_zone_use, false);
            if (ret < 0) {
                goto change_state;
            }
        }

        //check batty info
        if (0 == charge_zone_use.ops->get_cur_batty_info(&charge_zone_use)) {
            if ((++batty_info_print) % BATTYINFO_PRINT_PER_SECS == 0) {
                chargeinfo("vol:%d cur:%d btmp:%0.1f chip:%s cus:%d stmp:%0.1f\r\n",
                    charge_zone_use.batty_info.voltage,
                    charge_zone_use.batty_info.current,
                    ((float)charge_zone_use.batty_info.temperature) / TEMP_VALUE_GAIN,
                    (charge_zone_use.charge_chip_type == 0) ? "Buck" : "Pump",
                    charge_zone_use.work_current,
                    ((float)charge_zone_use.charge_temperature.shell_temp) / TEMP_VALUE_GAIN);
            }

            //check temperature protect state
            if (charge_zone_use.ops->get_temperature_protect_change(&charge_zone_use)) {
                msg.mode = (charge_zone_use.ops->get_temperature_protect(&charge_zone_use)) ? CHARGE_MODE_TEMP_PROTECT : CHARGE_MODE_TEMP_PROTECT_QUIT;
                rx_open_fail_times = 0;
                chargedebug("=====%d--mode = %d\n", __LINE__, msg.mode);
                send_charge_msg(msg);
                goto end;
            }
            //check work chip change
            if (get_instance()->temp_protect_lock == false) {
                if (0 == charge_zone_use.ops->get_work_chip_change(&charge_zone_use)) {
                    rx_open_fail_times = 0;
                    msg.mode = CHARGE_MODE_UPDATE;
                    get_instance()->pump_open_fail_times = 0;
                    send_charge_msg(msg);
                    goto end;
                }
            }
        }

    } break;

    case PUMP_WORK_STATE_EXP: {
        chargeinfo("pump_state is %d\n", charge_zone_use.pump_work_state);
        rx_open_fail_times = 0;
        get_instance()->pump_open_fail_times++;
        if (get_instance()->pump_open_fail_times < 3) {
            chargedebug("============--%d pump_open_fail_times=%d\n", __LINE__, get_instance()->pump_open_fail_times);
            pump_startup_voltage_count = 0;
            charge_zone_use.pump_work_state = PUMP_WORK_STATE_START;
            goto change_state;
        } else {
            if (pump_exp_flag) {
                chargedebug("============--%d pump_open_fail_times=%d\n", __LINE__, get_instance()->pump_open_fail_times);
#if 0
                    if(charge_zone_use.ops->set_pump_adc(&charge_zone_use, false)){
                        chargeerr("close pump adc failed\n");
                    }
#endif
                if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, CHARHE_CHIP_TYPE_NULL, true)) {
                    chargeerr("close all charge chip failed;\r\n");
                }
                pump_exp_flag = false;
                msg.mode = CHARGE_MODE_MONITOR;
                send_charge_msg(msg);
                goto change_state;
            }
        }
    }
    change_state:
        break;

    default:
        break;
    }
end:
    return 0;
}
static int update_cb(void)
{
    charge_msg_t msg;
    int16_t buck_set_cur_val = 0;
    int16_t no_stand_soft_start_current = 0;
    chargedebug("charge_chip_type=%d work_current=%d pump_open_fail_times=%d\r\n",
        charge_zone_use.charge_chip_type,
        charge_zone_use.work_current,
        get_instance()->pump_open_fail_times);

#if 0
    uint16_t vxout = 0;
    if(charge_zone_use.ops->get_wpc_rx_voltage(&charge_zone_use, &vxout)){
        chargeerr("get_wpc_rx_voltage failed;\r\n");
    }
    chargedebug("---up_date rx out = %d\n", vxout);
#endif
    if (charge_zone_use.charge_chip_type == CHARHE_CHIP_TYPE_PUMP) {
        if (get_instance()->pump_open_fail_times >= 3) {
            charge_zone_use.charge_chip_type = CHARHE_CHIP_TYPE_BUCK;
            charge_zone_use.work_current = CHARGE_CURRENT_2_00C;
            if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, charge_zone_use.charge_chip_type, true) || charge_zone_use.ops->ctrol_chip_current(&charge_zone_use, charge_zone_use.charge_chip_type, charge_zone_use.work_current)) {
                chargeerr("set buck failed;\r\n");
                if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, CHARHE_CHIP_TYPE_NULL, true)) { //force close all chip
                    chargeerr("close all charge chip failed;\r\n");
                }
            }
        } else {
            get_instance()->pump_open_fail_times = 0;
            charge_zone_use.pump_work_state = PUMP_WORK_STATE_START;
            msg.mode = CHARGE_MODE_PUMP_WORK;
            send_charge_msg(msg);
        }
    } else {
        if ((charge_zone_use.adapter == WPC_ADAPTER_STATE_NO_TYPE3) && (charge_zone_use.batty_info.voltage >= 3600)) {
            no_stand_soft_start_current = charge_zone_use.ops->no_stand_soft_start_handle(&charge_zone_use,
                charge_zone_use.charge_chip_type, NO_STAND_CURRENT_100_MA);
            chargeinfo("no stand soft set current: %dma\n", no_stand_soft_start_current);
            buck_set_cur_val = no_stand_soft_start_current;
        } else {
            buck_set_cur_val = charge_zone_use.work_current;
        }
        if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, charge_zone_use.charge_chip_type, true) || charge_zone_use.ops->ctrol_chip_current(&charge_zone_use, charge_zone_use.charge_chip_type, buck_set_cur_val)) {
            chargeerr("set buck failed;\r\n");
            if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, CHARHE_CHIP_TYPE_NULL, true)) { //force close all chip
                chargeerr("close all charge chip failed;\r\n");
            }
        }
    }

    return 0;
}

static int monitor_cb(void)
{
    charge_msg_t msg;

#ifdef CONFIG_FACTEST_CHARGE
    if (get_instance()->fac_charge_aging_stop_flag) {
        return -1;
    }
#endif

    //temperature check
    if (0 == charge_zone_use.ops->get_cur_charge_temperature(&charge_zone_use)) {
        chargedebug("charge_zone_use.charge_temperature.shell_temp=%f\r\n", charge_zone_use.charge_temperature.shell_temp);
        //check temperature protect state
        if (charge_zone_use.ops->get_temperature_protect_change(&charge_zone_use)) {
            msg.mode = (charge_zone_use.ops->get_temperature_protect(&charge_zone_use)) ? CHARGE_MODE_TEMP_PROTECT : CHARGE_MODE_TEMP_PROTECT_QUIT;
            chargedebug("=====%d--mode = %d\n", __LINE__, msg.mode);
            send_charge_msg(msg);
        }
    }
    if ((charge_zone_use.ops->get_temperature_protect(&charge_zone_use) & (1 << CHARGE_TEMP_PROTECT_SKIN)) == 0) {
        //check batty info
        batty_info_check(true);
    }

    if (charge_zone_use.ops->get_cutoff_condition(&charge_zone_use) == 0) {
        msg.mode = CHARGE_MODE_CUTOFF;
        chargeinfo("send CHARGE_MODE_CUTOFF\n");
        send_charge_msg(msg);
    }

    return 0;
}

static int cutoff_cb(void)
{
    if (false == get_instance()->cutoff_lock) {
        chargeinfo("cutoff exec\n");
        temp_protect_cb();
        get_instance()->cutoff_timer_counter = 0;
        get_instance()->cutoff_lock = true;
    }

    return 0;
}

static int cutoff_release_cb(void)
{
    chargeinfo("cutoff_release exec\n");
    temp_protect_quit_cb();

    return 0;
}

#ifdef CONFIG_FACTEST_CHARGE
static int fac_aging_stop_cb(void)
{
    int ret = 0;

    if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, CHARHE_CHIP_TYPE_NULL, true)) {
        chargeerr("close all charge chip failed;\r\n");
    }

    if (charge_zone_use.ops->ctrol_wpc_sleep(&charge_zone_use, true)) {
        chargeerr("ctrol_wpc_sleep on failed;\r\n");
    }

    get_instance()->fac_charge_aging_stop_flag = true;

    return ret;
}

static int fac_aging_start_cb(void)
{
    int ret = 0;
    get_instance()->fac_charge_aging_stop_flag = false;
    if (charge_zone_use.ops->ctrol_wpc_sleep(&charge_zone_use, false)) {
        chargeerr("ctrol_wpc_sleep off failed;\r\n");
    }

    return ret;
}

static int fac_send_charge_msg(charge_msg_t msg)
{
    mqd_t mq;
    struct mq_attr attr;
    int ret = 0;

    attr.mq_maxmsg = MQ_MSG_LOAD_MAX;
    attr.mq_msgsize = sizeof(charge_msg_t);
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;
    mq = mq_open(MQ_FAC_BUCK_CHARGE_NAME, O_RDWR | O_CREAT, 0644, &attr);
    if (mq == (mqd_t)-1) {
        chargeerr("%s:open mq failed;\r\n", __FUNCTION__);
        return -1;
    }
    ret = mq_send(mq, (const char*)&msg, sizeof(msg), 1);
    chargedebug("%s: msg.mode = %d ret = %d;\r\n", __FUNCTION__, msg.mode, ret);
    mq_close(mq);
    return ret;
}

static int fac_buck_charge_cb(void)
{
    int ret = 0;
    charge_msg_t msg;

    get_instance()->fac_buck_charge_flag = true;

    if (charge_zone_use.ops->ctrol_wpc_sleep(&charge_zone_use, false)) {
        chargeerr("set sleep low failed\r\n");
        return -1;
    }

    charge_zone_use.charge_chip_type = CHARHE_CHIP_TYPE_BUCK;
    charge_zone_use.work_current = 300;

    if (charge_zone_use.ops->ctrol_chip_current(&charge_zone_use, charge_zone_use.charge_chip_type,
            charge_zone_use.work_current)) {
        chargeerr("set buck current falid;\r\n");
        return -1;
    }

    if (charge_zone_use.ops->ctrol_chip_work(&charge_zone_use, charge_zone_use.charge_chip_type, true)) {
        chargeerr("close buck failed;\r\n");
        return -1;
    }

    syslog(LOG_DEBUG, "send CHARGE_MODE_FAC_BUCK_CHARGE_ACK\n");
    msg.mode = CHARGE_MODE_FAC_BUCK_CHARGE_ACK;
    fac_send_charge_msg(msg);

    return ret;
}
#endif

static int rx_detect_thread(void* arg)
{
    charge_msg_t msg;
    int rx_state = -1;
    int uorb_fd = -1;
    int ret = -1;
    int nfds = -1;
    int epollfd = -1;
    struct epoll_event ev;
    struct epoll_event events[12];
    //bool update = false;
    struct battery_state battery_state_get;

    uorb_fd = orb_subscribe(ORB_ID(battery_state));
    if (uorb_fd < 0) {
        chargeerr("Uorb subscrib  failed: %d\n", uorb_fd);
        return -1;
    }
    epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0) {
        chargeerr("epoll_create1  failed: %d\n", epollfd);
        return -1;
    }

    ev.events = POLLIN;
    ev.data.fd = uorb_fd;
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, uorb_fd, &ev);
    if (ret < 0) {
        ret = -errno;
        chargeerr("EPOLL_CTL_ADD  failed: %d\n", ret);
        return ret;
    }
    chargedebug("rx_state get by uorb epoll\r\n");
    while (1) {
        nfds = epoll_wait(epollfd, events, 12, -1);
        for (uint8_t i = 0; i < nfds; i++) {
            if ((events[i].events & POLLIN) && (events[i].data.fd == uorb_fd)) {
                ret = orb_copy(ORB_ID(battery_state), uorb_fd, &battery_state_get);
                if (ret != OK) {
                    chargeerr("temp orb copy failed\n");
                    break;
                }
                rx_state = battery_state_get.state;
                chargedebug("[%d]==****===rx_state=%d charge_zone_use.wpc_rx_state=%d\n", __LINE__, rx_state, charge_zone_use.wpc_rx_state);
                if (((rx_state == WPC_RX_STATE_ENTER) || (rx_state == WPC_RX_STATE_QUITE)) && (rx_state != charge_zone_use.wpc_rx_state)) {
                    chargeinfo("rx_state change to %d\r\n", rx_state);
                    msg.mode = (rx_state == WPC_RX_STATE_ENTER) ? CHARGE_MODE_WPC_ENTER : CHARGE_MODE_WPC_QUIT;
                    send_charge_msg(msg);
                    charge_zone_use.wpc_rx_state = rx_state;
                }
            }
        }
    }

    return 0;
}

static int charge_service_thread(int argc, FAR char* argv[])
{

    charge_msg_t recive_msg;
    struct mq_attr attr;
    mqd_t recive_mq = 0;
    int ret = -1;

    chargeinfo("charge service thread start;\r\n");
    /************  mq  *******************/
    attr.mq_maxmsg = MQ_MSG_LOAD_MAX;
    attr.mq_msgsize = sizeof(charge_msg_t);
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;
    recive_mq = mq_open(MQ_MSG_NAME, O_RDWR | O_CREAT, 0644, &attr);
    if (recive_mq == (mqd_t)-1) {
        /* Unable to open message queue! */
        chargeerr("err open mq;\r\n");
        return -1;
    }
    for (;;) {
        mq_receive(recive_mq, (char*)&recive_msg, sizeof(recive_msg), NULL);
        charge_service_data.mode = recive_msg.mode;
        switch (charge_service_data.mode) {
        case CHARGE_MODE_INIT:
            chargedebug("mode=CHARGE_MODE_INIT\r\n");
            if (charge_mode_init_cb(get_instance())) {
                charge_mode_init_release(get_instance());
                chargeerr("charge init mode failed;\r\n");
                goto end;
            }
            /*init rx state check pthread*/
            ret = pthread_create(&(charge_service_data.rx_detect_threadid), NULL, (void*)rx_detect_thread, NULL);
            if (ret != 0) {
                chargeerr("create pthread faild\n");
                goto end;
            }
            break;
        case CHARGE_MODE_WPC_ENTER:
#if defined(CONFIG_PM)
            if (get_instance()->pm_lock == false) {
                pm_wakelock_stay(&g_pm_wakelock_charge);
                get_instance()->pm_lock = true;
                syslog(LOG_WARNING, "[charge] power state: stay\n");
            }
#endif
            chargedebug("mode=CHARGE_MODE_WPC_ENTER\r\n");
            wpc_enter_cb();
            break;
        case CHARGE_MODE_WPC_QUIT:
            chargedebug("mode=CHARGE_MODE_WPC_QUIT\r\n");
            wpc_quit_cb();
#if defined(CONFIG_PM)
            if (get_instance()->pm_lock == true) {
                pm_wakelock_relax(&g_pm_wakelock_charge);
                get_instance()->pm_lock = false;
                syslog(LOG_WARNING, "[charge] power state: relax\n");
            }
#endif
            break;
        case CHARGE_MODE_TEMP_PROTECT:
            chargedebug("mode=CHARGE_MODE_TEMP_PROTECT\r\n");
            temp_protect_cb();
            break;
        case CHARGE_MODE_TEMP_PROTECT_QUIT:
            chargedebug("mode=CHARGE_MODE_TEMP_PROTECT_QUIT\r\n");
            temp_protect_quit_cb();
            break;
        case CHARGE_MODE_PUMP_WORK:
            chargedebug("mode=CHARGE_MODE_PUMP_WORK\r\n");
            if (charge_zone_use.wpc_rx_state == WPC_RX_STATE_ENTER) {
                get_instance()->pump_switch_locked = true;
                pump_work_cb();
                get_instance()->pump_switch_locked = false;
            }
            break;
        case CHARGE_MODE_UPDATE:
            chargedebug("mode=CHARGE_MODE_UPDATE\r\n");
            update_cb();
            break;
        case CHARGE_MODE_MONITOR:
            chargedebug("mode=CHARGE_MODE_MONITOR\r\n");
            if (charge_zone_use.wpc_rx_state == WPC_RX_STATE_ENTER) {
                get_instance()->monitor_lock = true;
                monitor_cb();
                get_instance()->monitor_lock = false;
            }
            break;

        case CHARGE_MODE_CUTOFF:
            cutoff_cb();
            break;
        case CHARGE_MODE_CUTOFF_RELEASE:
            cutoff_release_cb();
            break;

#ifdef CONFIG_FACTEST_CHARGE
        case CHARGE_MODE_FAC_AGING_START:
            chargedebug("mode=CHARGE_MODE_FAC_AGING_START\r\n");
            fac_aging_start_cb();
            break;

        case CHARGE_MODE_FAC_AGING_STOP:
            chargedebug("mode=CHARGE_MODE_FAC_AGING_STOP\r\n");
            fac_aging_stop_cb();
            break;

        case CHARGE_MODE_FAC_BUCK_CHARGE:
            chargedebug("mode=CHARGE_MODE_FAC_BUCK\r\n");
            fac_buck_charge_cb();
            break;
#endif

        default:
            chargeinfo("unsupport cmd; mode=%d\r\n", recive_msg.mode);
            break;
        }
    }

end:
    mq_close(recive_mq);
    mq_unlink(MQ_MSG_NAME);
    if ((get_instance()->rx_detect_threadid != 0) && pthread_cancel(get_instance()->rx_detect_threadid)) {
        chargeerr("pthread cancle faild\n");
    }
    if (env_timer_quit(&(get_instance()->env_timer_id)) != 0) {
        chargeerr("timer cancel failed. ret=%d.\n", ret);
    }
    get_instance()->service_pid = 0;
    get_instance()->rx_detect_threadid = 0;
    chargeinfo("charge service thread stop;\r\n");
    return 0;
}

/****************************************************************************
 * Main
 ****************************************************************************/
int main(int argc, FAR char* argv[])
{
    chargeinfo("in charge_service main!\r\n");
    init_service(); // init service data & thread
    return 0;
}
