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

#include "charger_statemachine.h"
#include "charger_hwintf.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

#if defined(CONFIG_PM)
PM_WAKELOCK_DECLARE_STATIC(g_pm_wakelock_chargerd, "chargerd_wakelock", PM_IDLE_DOMAIN, PM_NORMAL);
#endif

static bool delay_lock = false;
static bool pm_lock = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int charger_timer_cb(void)
{
    charger_msg_t msg;
    msg.event = CHARGER_EVENT_CHG_TIMEOUT;
    if (!delay_lock) {
        send_charger_msg(msg);
    }
    return 0;
}

static bool check_battery_full(struct charger_manager* manager)
{
    int capacity;
    int current;
    int ret = 0;
    static int cnt = 0;

    ret = get_battery_capacity(manager, &capacity);
    ret |= get_battery_current(manager, &current);
    if (ret < 0) {
        chargererr("can not get battery info , so cutoff\n");
        return true;
    }

    chargerdebug("capacity :%d current:%d\n", capacity, current);
    if (capacity == manager->desc.fullbatt_capacity && current >= 0 && current <= manager->desc.fullbatt_current) {

        /*three times of continuous,the condition is satisfying, avoid jitter */

        if (cnt++ > 3) {
            chargerdebug("battery is full\n");
            return true;
        }
        return false;
    }
    cnt = 0;
    return false;
}

static void clear_fullbatt_timer_cnt(struct charger_manager* manager)
{
    manager->fullbatt_timer_cnt = 0;
}

static bool update_fullbatt_timer(struct charger_manager* manager)
{
    uint64_t duration;

    manager->fullbatt_timer_cnt++;
    duration = manager->fullbatt_timer_cnt * manager->desc.polling_interval_ms;
    if (duration >= manager->desc.fullbatt_duration_ms) {
        return true;
    }
    return false;
}

static void clear_fault_timer_cnt(struct charger_manager* manager)
{
    manager->fault_timer_cnt = 0;
}

static bool update_fault_timer(struct charger_manager* manager)
{
    uint64_t duration;

    manager->fault_timer_cnt++;
    duration = manager->fault_timer_cnt * manager->desc.polling_interval_ms;
    if (duration >= manager->desc.fault_duration_ms) {
        return true;
    }
    return false;
}

static int update_charger_protocol(struct charger_manager* manager)
{
    int adapter_t = 0;
    int ret;

    if (is_adapter_exist()) {
        ret = get_adapter_type(manager, &adapter_t);
        chargerassert_return(ret < 0, "get adapter type failed\n");
    } else {
        if (manager->desc.chargers > 0) {
            ret = get_adapter_type_by_charger(manager, &adapter_t);
            chargerassert_return(ret < 0, "get adapter type by charger failed\n");
        }
    }
    chargerdebug("charger protocol is %d\n", adapter_t);
    manager->protocol = adapter_t;
    return CHARGER_OK;
}

static int charger_state_init(struct charger_manager* data, charger_msg_t* pevent)
{
    int ret;

    if (NULL == pevent) {
        charger_timer_stop(&data->env_timer_id);
        charger_sleep();
        return CHARGER_OK;
    }

    switch (pevent->event) {
    case CHARGER_EVENT_PLUGIN:
        if (charger_timer_start(&data->env_timer_id, data->desc.polling_interval_ms) < 0) {
            chargererr("creat timer failed;\n");
            return CHARGER_FAILED;
        }
        charger_wakup();
        ret = update_charger_protocol(data);
        if (data->temp_protect_lock) {
            data->nextstate = CHARGER_STATE_TEMP_PROTECT;
        } else if (ret < 0) {
            data->nextstate = CHARGER_STATE_FAULT;
        } else {
            data->nextstate = CHARGER_STATE_CHG;
        }
        break;
    default:
        chargerdebug("CHARGER_STATE_INIT ignore %d\n", pevent->event);
    }
    return CHARGER_OK;
}

static int charger_state_temp_protect(struct charger_manager* data, charger_msg_t* pevent)
{
    int seq;
    int temp = 0;
    int ret;

    if (NULL == pevent) {
        for (seq = 0; seq < data->desc.chargers; seq++) {
            ret = enable_charger(data, seq, false);
            chargerassert_noreturn(ret < 0, "disable charger %d failed\n", seq);
        }
        if (is_adapter_exist()) {
            ret = enable_adapter(data, false);
            chargerassert_noreturn(ret < 0, "disable adapter failed\n");
        }
        return CHARGER_OK;
    }

    switch (pevent->event) {
    case CHARGER_EVENT_OVERTEMP_RECOVERY:
        if (data->online) {
            data->nextstate = CHARGER_STATE_CHG;
        } else {
            data->nextstate = CHARGER_STATE_INIT;
        }
        if (is_adapter_exist()) {
            ret = enable_adapter(data, true);
            chargerassert_noreturn(ret < 0, "enable adapter failed\n");
        }
        break;
    case CHARGER_EVENT_CHG_TIMEOUT:
        ret = get_battery_temp(data, &temp);
        chargerassert_return(ret < 0, "get battery temp failed\n");
        ret = update_battery_temperature(temp);
        chargerassert_return(ret < 0, "update battery temperature failed\n");
        break;
    default:
        chargerdebug("CHARGER_STATE_TEMP_PROTECT ignore %d\n", pevent->event);
    }

    return CHARGER_OK;
}

static void charger_chg_proc_algostop(struct charger_manager* data)
{
    int curr_charger;
    struct charger_algo* algo = NULL;
    int ret;

    curr_charger = data->curr_charger;
    if (curr_charger != CHARGER_INDEX_INVAILD) {
        algo = &data->algos[curr_charger];
        ret = algo->ops->stop(algo);
        chargerassert_noreturn(ret < 0, "algo %d stop failed\n", algo->index);
        data->curr_charger = CHARGER_INDEX_INVAILD;
    }
    return;
}

static int charger_chg_proc_fault(struct charger_manager* data)
{
    charger_chg_proc_algostop(data);
    data->nextstate = CHARGER_STATE_FAULT;
    return CHARGER_FAILED;
}

static int charger_chg_proc_plot(struct charger_manager* data, struct charger_plot_parameter* pa)
{
    int* curr_charger;
    struct charger_algo* algo = NULL;
    int ret;

    curr_charger = &data->curr_charger;
    if (*curr_charger == CHARGER_INDEX_INVAILD) {
        *curr_charger = pa->charger_index;
        algo = &data->algos[*curr_charger];
        ret = algo->ops->start(algo);
        chargerassert_return(ret < 0, "algo %d start failed\n", algo->index);
    } else if (*curr_charger == pa->charger_index) {
        algo = &data->algos[*curr_charger];
        ret = algo->ops->update(algo, pa);
        chargerassert_return(ret < 0, "algo %d update failed\n", algo->index);
    } else {
        algo = &data->algos[*curr_charger];
        ret = algo->ops->stop(algo);
        chargerassert_return(ret < 0, "algo %d stop failed\n", algo->index);
        *curr_charger = pa->charger_index;
        algo = &data->algos[*curr_charger];
        ret = algo->ops->start(algo);
        chargerassert_return(ret < 0, "algo %d start failed\n", algo->index);
        ret = algo->ops->update(algo, pa);
        chargerassert_return(ret < 0, "algo %d update failed\n", algo->index);
    }
    return CHARGER_OK;
}

static int charger_chg_proc(struct charger_manager* data)
{
    int temp = 0;
    int vol = 0;
    struct charger_plot_parameter* pa = NULL;

    if (check_battery_full(data)) {
        charger_chg_proc_algostop(data);
        data->nextstate = CHARGER_STATE_FULL;
        return CHARGER_OK;
    }

    if (update_charger_protocol(data) < 0) {
        chargererr("update_charger_protocol failed\n");
        goto fault;
    }

    if (get_battery_temp(data, &temp) < 0) {
        chargererr("get battery temperature failed\n");
        goto fault;
    }

    if (update_battery_temperature(temp) < 0) {
        chargererr("update battery temperature failed\n");
        goto fault;
    }

    if (get_battery_voltage(data, &vol) < 0) {
        chargererr("get battery voltage failed\n");
        goto fault;
    }

    pa = check_charger_plot(temp, vol, data->protocol);
    if (NULL == pa || pa->charger_index == CHARGER_INDEX_INVAILD) {
        chargerwarn("temp:%d vol:%d not find the plot_parameter"
                    " or charger_index is invaild\n",
            temp, vol);
        charger_chg_proc_algostop(data);
        return CHARGER_OK;
    }

    if (charger_chg_proc_plot(data, pa) < 0) {
        chargererr("charger chg proc plot failed\n");
        goto fault;
    }
    return CHARGER_OK;
fault:
    return charger_chg_proc_fault(data);
}

static int charger_state_chg(struct charger_manager* data, charger_msg_t* pevent)
{
    if (NULL == pevent) {
        return charger_chg_proc(data);
    }

    switch (pevent->event) {
    case CHARGER_EVENT_PLUGOUT:
        charger_chg_proc_algostop(data);
        data->nextstate = CHARGER_STATE_INIT;
        break;
    case CHARGER_EVENT_CHG_TIMEOUT:
        return charger_chg_proc(data);
    case CHARGER_EVENT_OVERTEMP:
        charger_chg_proc_algostop(data);
        data->nextstate = CHARGER_STATE_TEMP_PROTECT;
        break;
    default:
        chargerdebug("CHARGER_STATE_CHG ignore %d\n", pevent->event);
    }
    return CHARGER_OK;
}

static int charger_state_full(struct charger_manager* data, charger_msg_t* pevent)
{
    int ret = 0;

    if (NULL == pevent) {
        if (is_adapter_exist()) {
            ret = enable_adapter(data, false);
            chargerassert_noreturn(ret < 0, "disable adapter failed\n");
        }
        clear_fullbatt_timer_cnt(data);
        return CHARGER_OK;
    }

    switch (pevent->event) {
    case CHARGER_EVENT_CHG_TIMEOUT:
        if (update_fullbatt_timer(data)) {
            if (data->online) {
                data->nextstate = CHARGER_STATE_CHG;
            } else {
                data->nextstate = CHARGER_STATE_INIT;
            }
            if (is_adapter_exist()) {
                ret = enable_adapter(data, true);
                chargerassert_noreturn(ret < 0, "enable adapter failed\n");
            }
        }
        break;
    case CHARGER_EVENT_OVERTEMP:
        data->nextstate = CHARGER_STATE_TEMP_PROTECT;
        break;
    case CHARGER_EVENT_PLUGOUT:
        if (is_adapter_exist()) {
            ret = enable_adapter(data, true);
            chargerassert_noreturn(ret < 0, "enable adapter failed\n");
        }
        data->nextstate = CHARGER_STATE_INIT;
        break;
    default:
        chargerdebug("CHARGER_STATE_FULL ignore %d\n", pevent->event);
    }

    return CHARGER_OK;
}

static int check_fault_plot(struct charger_manager* data)
{
    int temp = 0;
    int vol = 0;
    int ret = 0;
    struct charger_plot_parameter* pa = NULL;
    struct charger_algo* algo = NULL;

    if (data->desc.fault.charger_index == CHARGER_INDEX_INVAILD) {
        return CHARGER_FAILED;
    }

    ret = get_battery_temp(data, &temp);
    ret |= get_battery_voltage(data, &vol);
    chargerassert_return(ret < 0, "get battery info failed\n");
    pa = &data->desc.fault;
    if (temp >= pa->temp_range_min && temp <= pa->temp_range_max
        && vol >= pa->vol_range_min && vol <= pa->vol_range_max) {
        algo = &data->algos[data->desc.fault.charger_index];
        data->curr_charger = data->desc.fault.charger_index;
        ret = algo->ops->start(algo);
        ret |= algo->ops->update(algo, &data->desc.fault);
        if (ret < 0) {
            charger_chg_proc_algostop(data);
        }
        return ret;
    }
    return CHARGER_FAILED;
}

static int charger_fault_proc(struct charger_manager* data)
{
    int seq = 0;
    int ret;

    if (check_fault_plot(data) < 0) {
        for (seq = 0; seq < data->desc.chargers; seq++) {
            ret = enable_charger(data, seq, false);
            chargerassert_noreturn(ret < 0, "disable charger %d failed\n", seq);
        }
        if (is_adapter_exist()) {
            ret = enable_adapter(data, false);
            chargerassert_noreturn(ret < 0, "disable adapter failed\n");
        }
    }
    return CHARGER_OK;
}

static void charger_fault_escape(struct charger_manager* data)
{
    int ret;

    charger_chg_proc_algostop(data);
    if (is_adapter_exist()) {
        ret = enable_adapter(data, true);
        chargerassert_noreturn(ret < 0, "enable adapter failed\n");
    }
    return;
}

static int charger_state_fault(struct charger_manager* data, charger_msg_t* pevent)
{

    if (NULL == pevent) {
        clear_fault_timer_cnt(data);
        return charger_fault_proc(data);
    }

    switch (pevent->event) {
    case CHARGER_EVENT_CHG_TIMEOUT:
        if (check_battery_full(data)) {
            data->nextstate = CHARGER_STATE_FULL;
        } else if (update_fault_timer(data)) {
            if (data->online) {
                data->nextstate = CHARGER_STATE_CHG;
            } else {
                data->nextstate = CHARGER_STATE_INIT;
            }
        }
        break;
    case CHARGER_EVENT_OVERTEMP:
        data->nextstate = CHARGER_STATE_TEMP_PROTECT;
        break;
    case CHARGER_EVENT_PLUGOUT:
        data->nextstate = CHARGER_STATE_INIT;
        break;
    default:
        chargerdebug("CHARGER_STATE_FAULT ignore %d\n", pevent->event);
    }
    if (data->nextstate != CHARGER_STATE_FAULT) {
        charger_fault_escape(data);
    }
    return CHARGER_OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void charger_wakup(void)
{
#if defined(CONFIG_PM)
    if (pm_lock == false) {
        pm_wakelock_stay(&g_pm_wakelock_chargerd);
        pm_lock = true;
        chargerwarn("[charger] power state: stay\n");
    }
#endif
}

void charger_sleep(void)
{
#if defined(CONFIG_PM)
    if (pm_lock == true) {
        pm_wakelock_relax(&g_pm_wakelock_chargerd);
        pm_lock = false;
        chargerwarn("[charger] power state: relax\n");
    }
#endif
}

void charger_delay(unsigned int delay_ms)
{
    delay_lock = true;
    usleep(1000 * delay_ms);
    delay_lock = false;
}

int charger_timer_stop(timer_t* timer)
{
    int ret;

    if (*timer != 0) {
        ret = timer_delete(*timer);
        if (ret != 0) {
            chargererr("timer_delete fail: %d\r\n", ret);
            return -1;
        } else {
            *timer = 0;
            chargerinfo("timer_delete success!\n");
        }
    } else {
        chargerinfo("timer_create null!\n");
    }
    return 0;
}

int charger_timer_start(timer_t* timer, time_t poll_interval)
{
    int ret;
    struct sigevent evp;
    struct itimerspec it;

    memset(&evp, 0, sizeof(struct sigevent));
    evp.sigev_value.sival_int = 122;
    evp.sigev_notify = SIGEV_THREAD;
    evp.sigev_notify_function = (void*)charger_timer_cb;

    it.it_interval.tv_sec = poll_interval / 1000;
    it.it_interval.tv_nsec = (poll_interval % 1000) * 1000;
    it.it_value.tv_sec = poll_interval / 1000;
    it.it_value.tv_nsec = (poll_interval % 1000) * 1000;

    if (*timer != 0) {
        chargererr("timer has created\r\n");
        return 0;
    }
    ret = timer_create(CLOCK_REALTIME, &evp, timer);
    if (ret != 0) {
        chargererr("timer_create fail: %d\r\n", ret);
        return -1;
    }

    ret = timer_settime(*timer, 0, &it, 0);

    if (ret != 0) {
        chargererr("timer_start fail: %d\r\n", ret);
        return -1;
    }
    chargerinfo("timer_create success!\n");
    return 0;
}

void init_state_func_tables(struct charger_manager* manager)
{
    manager->functables[CHARGER_STATE_INIT] = charger_state_init;
    manager->functables[CHARGER_STATE_CHG] = charger_state_chg;
    manager->functables[CHARGER_STATE_TEMP_PROTECT] = charger_state_temp_protect;
    manager->functables[CHARGER_STATE_FULL] = charger_state_full;
    manager->functables[CHARGER_STATE_FAULT] = charger_state_fault;
    return;
}

int charger_statemachine_state_run(struct charger_manager* data,
    charger_msg_t* event, bool* changed)
{
    int ret = 0;

    chargerdebug("current state %d\n", data->currstate);

    DEBUGASSERT(data->functables[data->currstate]);
    if (*changed) {
        *changed = false;
        ret = data->functables[data->currstate](data, NULL);
        if (ret < 0) {
            chargererr("enter state %d failed\n", data->currstate);
        }
    } else {
        ret = data->functables[data->currstate](data, event);
        if (ret < 0) {
            chargererr("state %d handle event %d failed\n", data->currstate, event->event);
        }
    }

    if (data->currstate != data->nextstate) {
        *changed = true;
        chargerinfo("change state %d to %d\n", data->currstate, data->nextstate);
        data->prestate = data->currstate;
        data->currstate = data->nextstate;
    }
    return ret;
}
