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

#include "charger_manager.h"
#include "charger_hwintf.h"
#include "charger_statemachine.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TEMP_VALUE_GAIN 10.0

/****************************************************************************
 * Private Types
 ****************************************************************************/

typedef int (*handle_event)(int fd);

struct event_handler {
    int fd;
    handle_event callback;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int healthd_events(int fd);
static int thermal_events(int fd);
static int state_events(int fd);
static int charger_dev_init(void);
static void charger_dev_unit(void);
static int charger_event_engine_init(void);
static void charger_event_engine_unit(void);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct charger_manager g_charger_manager = {
    .supply_fd = CHARGER_FD_INVAILD,
    .adapter_fd = CHARGER_FD_INVAILD,
    .charger_fd = { CHARGER_FD_INVAILD },
    .gauge_fd = CHARGER_FD_INVAILD,
    .temp_protect_lock = false,
    .env_timer_id = 0,
    .online = false,
    .epollfd = CHARGER_FD_INVAILD,
    .curr_charger = CHARGER_INDEX_INVAILD,
};

static struct event_handler handlers[EVENT_HANDLER_MAX] = {
    { .fd = CHARGER_FD_INVAILD, .callback = healthd_events },
    { .fd = CHARGER_FD_INVAILD, .callback = thermal_events },
    { .fd = CHARGER_FD_INVAILD, .callback = state_events },
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int check_temp_event(void)
{
    bool bot = false;
    bool sot = false;
    int tmin, tmax, smin, smax;
    charger_msg_t msg;
    int ret = 0;

    if (g_charger_manager.temp_protect_lock) {
        tmin = g_charger_manager.desc.temp_min_r;
        tmax = g_charger_manager.desc.temp_max_r;
        smin = g_charger_manager.desc.temp_skin_min_r;
        smax = g_charger_manager.desc.temp_skin_max_r;
    } else {
        tmin = g_charger_manager.desc.temp_min;
        tmax = g_charger_manager.desc.temp_max;
        smin = g_charger_manager.desc.temp_skin_min;
        smax = g_charger_manager.desc.temp_skin_max;
    }

    chargerinfo("battery tempture:%d skin tempture:%d\n",
        g_charger_manager.battery_temp, g_charger_manager.skin_temp);

    if (g_charger_manager.battery_temp >= tmax || g_charger_manager.battery_temp <= tmin) {
        bot = true;
    }

    if (g_charger_manager.skin_temp >= smax || g_charger_manager.skin_temp <= smin) {
        sot = true;
    }

    if ((bot || sot) && !g_charger_manager.temp_protect_lock) {
        msg.event = CHARGER_EVENT_OVERTEMP;
        ret = send_charger_msg(msg);
        if (ret < 0) {
            chargererr("check_temp_event send event %d failed\n", CHARGER_EVENT_OVERTEMP);
        } else {
            chargerinfo("send CHARGER_EVENT_OVERTEMP successed\n");
            g_charger_manager.temp_protect_lock = true;
        }
    } else if (g_charger_manager.temp_protect_lock && !bot && !sot) {
        msg.event = CHARGER_EVENT_OVERTEMP_RECOVERY;
        ret = send_charger_msg(msg);
        if (ret < 0) {
            chargererr("check_temp_event send event %d failed\n", CHARGER_EVENT_OVERTEMP_RECOVERY);
        } else {
            chargerinfo("send CHARGER_EVENT_OVERTEMP_RECOVERY successed\n");
            g_charger_manager.temp_protect_lock = false;
        }
    }

    return ret;
}

static int get_val(struct range_data* range, int rise_hys, int fall_hys,
    int current_index, int threshold, int* new_index, int* val)
{
    int i;

    *new_index = -EINVAL;

    /*
     * Return -ENODATA if the threshold is below the lowest range value.
     */

    if (range[0].low_threshold > threshold)
        return -ENODATA;

    /* Attempt to locate the matching index without hysteresis */

    for (i = 0; i < MAX_RANGES; i++) {
        if (!range[i].low_threshold && !range[i].high_threshold) {

            /* Exit loop if the table entry is invalid */

            break;
        }

        if (is_between(range[i].low_threshold,
                range[i].high_threshold, threshold)) {
            *val = range[i].value;
            *new_index = i;
            break;
        }
    }

    /*
     * If no match is found, the threshold exceeds the maximum range
     * (minimum range case was handled at the start). Clip to the maximum
     * allowed range, corresponding to the last valid entry in the array.
     */

    if (-EINVAL == *new_index) {
        if (0 == i) {

            /* Entire array contains invalid data */

            return -ENODATA;
        }

        *new_index = (i - 1);
        *val = range[*new_index].value;
    }

    /*
     * If there's no current index, return the new value found
     * since no hysteresis is applied for out-of-range transitions.
     */

    if (-EINVAL == current_index)
        return 0;

    /*
     * Apply hysteresis if near the current index.
     */

    if (current_index + 1 == *new_index) {
        if (threshold < (rise_hys + range[*new_index].low_threshold)) {

            /*
             * Remain at the current index as the threshold
             * is within the hysteresis range.
             */

            *val = range[current_index].value;
            *new_index = current_index;
        }
    } else if (current_index - 1 == *new_index) {
        if (range[*new_index].high_threshold - fall_hys < threshold) {

            /*
             * Remain at the current index as the threshold
             * does not fall below the hysteresis range.
             */

            *val = range[current_index].value;
            *new_index = current_index;
        }
    }

    return 0;
}

static int termination_voltage_update(void)
{
    int termination_voltage, last_vterm_index;
    unsigned int charger_state = BATTERY_UNKNOWN;
    int ret = CHARGER_OK;

    ret = get_charger_state(&g_charger_manager,
        CHARGER_ALGO_BUCK, &charger_state);
    if (ret < 0) {
        chargererr("get charger_state failed\n");
        return CHARGER_FAILED;
    } else if (charger_state != BATTERY_CHARGING) {
        return ret;
    }

    last_vterm_index = g_charger_manager.desc.temp_vterm.vterm_index;
    ret = get_val(g_charger_manager.desc.temp_vterm.ranges,
        g_charger_manager.desc.temp_vterm.rise_hys,
        g_charger_manager.desc.temp_vterm.fall_hys,
        g_charger_manager.desc.temp_vterm.vterm_index,
        g_charger_manager.battery_temp,
        &g_charger_manager.desc.temp_vterm.vterm_index,
        &termination_voltage);
    if (ret < 0) {
        chargererr("get termination_voltage failed\n");
        return CHARGER_FAILED;
    }

    if (g_charger_manager.desc.temp_vterm.vterm_index != last_vterm_index) {
        ret = set_charger_voltage(&g_charger_manager,
            CHARGER_ALGO_BUCK, termination_voltage);
        if (ret < 0) {
            chargererr("set termination_voltage failed\n");
            return CHARGER_FAILED;
        }
    }

    return ret;
}

static int healthd_events(int fd)
{
    int ret;
    int temp;
    struct battery_state battery_state_get;
    bool online;
    charger_msg_t msg;

    ret = orb_copy(ORB_ID(battery_state), fd, &battery_state_get);
    if (ret != OK) {
        chargererr("temp orb copy failed\n");
        return CHARGER_FAILED;
    }

    chargerinfo("healthd event state:%d level:%d online:%d"
                "temp:%d curr:%d vol:%d\n",
        battery_state_get.state,
        battery_state_get.level, battery_state_get.online,
        battery_state_get.temp, battery_state_get.curr,
        battery_state_get.voltage);

    online = battery_state_get.online;
    if (online != g_charger_manager.online) {
        msg.event = online ? CHARGER_EVENT_PLUGIN : CHARGER_EVENT_PLUGOUT;
        ret = send_charger_msg(msg);
        chargerassert_noreturn(ret < 0, "send plug event failed\n");
        g_charger_manager.online = online;
    }
    temp = battery_state_get.temp;
    if (temp != g_charger_manager.battery_temp) {
        g_charger_manager.battery_temp = temp;
        ret = check_temp_event();
        if (g_charger_manager.desc.temp_vterm.enable) {
            ret = termination_voltage_update();
        }
    }
    return ret;
}

static int thermal_events(int fd)
{
    struct device_temperature bt;
    int ret = 0;
    int temp = 0;

    ret = orb_copy(ORB_ID(device_temperature), fd, &bt);
    temp = bt.skin * TEMP_VALUE_GAIN;
    if (temp != g_charger_manager.skin_temp) {
        g_charger_manager.skin_temp = temp;
        ret = check_temp_event();
    }
    return ret;
}

static int state_events(int fd)
{
    charger_msg_t recive_msg;
    bool changed = false;

    if (mq_receive(fd, (char*)&recive_msg, sizeof(recive_msg), NULL) > 0) {
        do {
            charger_statemachine_state_run(&g_charger_manager, &recive_msg, &changed);
        } while (changed);
    }
    return 0;
}

static int register_event_handler(int fd, struct event_handler* handler)
{
    struct epoll_event ev;
    int ret;

    handler->fd = fd;
    ev.events = POLLIN;
    ev.data.ptr = handler;
    ret = epoll_ctl(g_charger_manager.epollfd, EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0) {
        chargererr("EPOLL_CTL_ADD  failed: %d\n", ret);
    }
    return ret;
}

static int register_healthd_events(void)
{
    int uorb_fd;

    uorb_fd = orb_subscribe(ORB_ID(battery_state));
    if (uorb_fd < 0) {
        chargererr("Uorb subscrib  failed: %d\n", uorb_fd);
        return CHARGER_FAILED;
    }

    return register_event_handler(uorb_fd, &handlers[EVENT_HANDLER_HEALTHD]);
}

static int register_thermal_events(void)
{
    int uorb_fd;

    uorb_fd = orb_subscribe(ORB_ID(device_temperature));
    if (uorb_fd < 0) {
        chargererr("Uorb subscrib  failed: %d\n", uorb_fd);
        return CHARGER_FAILED;
    }

    return register_event_handler(uorb_fd, &handlers[EVENT_HANDLER_THERMAL]);
}

static int register_state_events(void)
{
    struct mq_attr attr;
    mqd_t recive_mq;

    attr.mq_maxmsg = MQ_MSG_LOAD_MAX;
    attr.mq_msgsize = sizeof(charger_msg_t);
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;
    recive_mq = mq_open(MQ_MSG_NAME, O_RDWR | O_CREAT | O_NONBLOCK | O_CLOEXEC, 0644, &attr);
    if (recive_mq == (mqd_t)CHARGER_FD_INVAILD) {
        chargererr("err open mq;\r\n");
        return CHARGER_FAILED;
    }

    return register_event_handler(recive_mq, &handlers[EVENT_HANDLER_STATE]);
}

static int charger_event_engine_init(void)
{
    int epollfd;
    int ret;

    epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0) {
        chargererr("epoll_create1  failed: %d\n", epollfd);
        return CHARGER_FAILED;
    }
    g_charger_manager.epollfd = epollfd;
    ret = register_healthd_events();
    ret |= register_thermal_events();
    ret |= register_state_events();

    if (ret < 0) {
        charger_event_engine_unit();
    }
    return ret;
}

static void charger_event_engine_unit(void)
{
    int i;

    for (i = 0; i < EVENT_HANDLER_MAX; i++) {
        if (CHARGER_FD_INVAILD != handlers[i].fd) {
            close(handlers[i].fd);
            handlers[i].fd = CHARGER_FD_INVAILD;
        }
    }
    if (g_charger_manager.epollfd != CHARGER_FD_INVAILD) {
        close(g_charger_manager.epollfd);
        g_charger_manager.epollfd = CHARGER_FD_INVAILD;
    }
    return;
}

static int charger_algo_init(int index, char* id)
{
    struct charger_algo* algo = NULL;
    struct charger_algo_ops* ops = NULL;

    algo = &(g_charger_manager.algos[index]);

    algo->index = index;
    algo->cm = &g_charger_manager;
    algo->priv = NULL;
    ops = get_algo_ops(id);
    if (NULL == ops) {
        chargererr("get_algo_ops %s not find\n", id);
        return CHARGER_FAILED;
    }
    algo->ops = ops;
    return CHARGER_OK;
}

static int charger_algos_init(void)
{
    int chargers;
    int i;
    int ret = CHARGER_FAILED;

    chargers = g_charger_manager.desc.chargers;

    for (i = 0; i < chargers; i++) {
        ret = charger_algo_init(i, g_charger_manager.desc.algo[i]);
        if (ret < 0) {
            break;
        }
    }
    return ret;
}

static int charger_dev_open(const char* dev)
{
    int fd;

    fd = open(dev, O_RDONLY);
    if (fd < 0) {
        chargererr("open dev %s failed (%d)\n", dev, -errno);
    }
    return fd;
}

static void charger_dev_close(int fd)
{
    if (fd != CHARGER_FD_INVAILD) {
        close(fd);
    }
    return;
}

static int charger_dev_init(void)
{
    int fd;
    const char* dev;
    int i;

    dev = g_charger_manager.desc.charger_supply;
    if (dev[0]) {
        if ((fd = charger_dev_open(dev)) < 0) {
            goto fail;
        }
        g_charger_manager.supply_fd = fd;
    }
    dev = g_charger_manager.desc.charger_adapter;
    if (dev[0]) {
        if ((fd = charger_dev_open(dev)) < 0) {
            goto fail;
        }
        g_charger_manager.adapter_fd = fd;
    }
    dev = g_charger_manager.desc.fuel_gauge;
    if (dev[0]) {
        if ((fd = charger_dev_open(dev)) < 0) {
            goto fail;
        }
        g_charger_manager.gauge_fd = fd;
    }

    for (i = 0; i < g_charger_manager.desc.chargers; i++) {
        dev = g_charger_manager.desc.charger[i];
        if (dev[0]) {
            if ((fd = charger_dev_open(dev)) < 0) {
                goto fail;
            }
            g_charger_manager.charger_fd[i] = fd;
        }
    }
    return CHARGER_OK;
fail:
    charger_dev_unit();
    return CHARGER_FAILED;
}

static void charger_dev_unit(void)
{
    int i;

    charger_dev_close(g_charger_manager.supply_fd);
    g_charger_manager.supply_fd = CHARGER_FD_INVAILD;
    charger_dev_close(g_charger_manager.adapter_fd);
    g_charger_manager.adapter_fd = CHARGER_FD_INVAILD;
    charger_dev_close(g_charger_manager.gauge_fd);
    g_charger_manager.gauge_fd = CHARGER_FD_INVAILD;
    for (i = 0; i < g_charger_manager.desc.chargers; i++) {
        charger_dev_close(g_charger_manager.charger_fd[i]);
        g_charger_manager.charger_fd[i] = CHARGER_FD_INVAILD;
    }
    return;
}

static int charger_manager_init(void)
{
    int ret;

    ret = charger_desc_init(&g_charger_manager.desc);
    if (ret < 0) {
        chargererr("charger_desc_init failed\n");
        goto desc_fail;
    }
    ret = charger_dev_init();
    if (ret < 0) {
        chargererr("charger_dev_init failed\n");
        goto dev_fail;
    }
    ret = charger_algos_init();
    if (ret < 0) {
        chargererr("charger_algos_init failed\n");
        goto algo_fail;
    }
    if (charger_event_engine_init() < 0) {
        chargererr("charger_event_engine_init failed\n");
        goto algo_fail;
    }
    g_charger_manager.currstate = CHARGER_STATE_INIT;
    g_charger_manager.nextstate = CHARGER_STATE_INIT;
    init_state_func_tables(&g_charger_manager);
    if (is_adapter_exist()) {
        ret = enable_adapter(&g_charger_manager, true);
        if (ret < 0) {
            goto fail;
        }
    }

    return CHARGER_OK;
fail:
    charger_event_engine_unit();
algo_fail:
    charger_dev_unit();
dev_fail:
    charger_desc_unit(&g_charger_manager.desc);
desc_fail:
    return CHARGER_FAILED;
}

static void charger_manager_unit(void)
{
    if (charger_timer_stop(&g_charger_manager.env_timer_id) != 0) {
        chargererr("timer cancel failed.\n");
    }
    charger_event_engine_unit();
    charger_dev_unit();
    charger_desc_unit(&g_charger_manager.desc);
}

static void charger_event_engine_start(void)
{
    int nfds = -1;
    struct epoll_event* pevs = NULL;
    struct event_handler* ep = NULL;
    int ret;

    pevs = zalloc(sizeof(struct epoll_event) * EVENT_HANDLER_MAX);
    if (NULL == pevs) {
        chargererr("malloc epoll_events failed\n");
        return;
    }

    while (1) {
        nfds = epoll_wait(g_charger_manager.epollfd, pevs, EVENT_HANDLER_MAX, -1);
        for (uint8_t i = 0; i < nfds; i++) {
            if ((pevs[i].events & POLLIN) && (pevs[i].data.ptr != NULL)) {
                ep = (struct event_handler*)pevs[i].data.ptr;
                ret = ep->callback(ep->fd);
                if (ret < 0) {
                    chargererr("event_handler %d callback ret:%d \n", ep->fd, ret);
                }
            }
        }
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

bool is_adapter_exist(void)
{
    if (g_charger_manager.adapter_fd < 0) {
        return false;
    }
    return true;
}

bool is_supply_exist(void)
{
    if (g_charger_manager.supply_fd < 0) {
        return false;
    }
    return true;
}

struct charger_plot_parameter* check_charger_plot(int temp, int vol, int type)
{
    static struct charger_plot_parameter* last_pa = NULL;
    struct charger_plot_parameter* pa = NULL;
    struct charger_plot* plot = NULL;
    int i = 0;

    chargerdebug("temp:%d vol:%d type:%d\n", temp, vol, type);

    for (i = 0; i < g_charger_manager.desc.plots; i++) {
        plot = &g_charger_manager.desc.plot[i];
        if (plot->mask & (1 << type)) {
            break;
        }
    }
    if (i >= g_charger_manager.desc.plots) {
        chargererr("there is no plot match type %d\n", type);
        return NULL;
    }
    for (i = 0; i < plot->parameters; i++) {
        pa = plot->tlbs + i;
        if (temp >= pa->temp_range_min && temp <= pa->temp_range_max
            && vol >= pa->vol_range_min && vol <= pa->vol_range_max) {
            if (last_pa != NULL && pa != last_pa) {
                if (pa->temp_range_min != last_pa->temp_range_min && pa->temp_range_max != last_pa->temp_range_max) {
                    if (pa->temp_range_min > last_pa->temp_range_min) {
                        if (temp < pa->temp_range_min + g_charger_manager.desc.temp_rise_hys) {
                            pa = last_pa;
                        }
                    } else {
                        if (temp > pa->temp_range_max - g_charger_manager.desc.temp_fall_hys) {
                            pa = last_pa;
                        }
                    }
                } else if (pa->vol_range_min != last_pa->vol_range_min && pa->vol_range_max != last_pa->vol_range_max) {
                    if (pa->vol_range_min > last_pa->vol_range_min) {
                        if (vol < pa->vol_range_min + g_charger_manager.desc.vol_rise_hys) {
                            pa = last_pa;
                        }
                    } else {
                        if (vol > pa->vol_range_max - g_charger_manager.desc.vol_fall_hys) {
                            pa = last_pa;
                        }
                    }
                }
            }
            last_pa = pa;
            return pa;
        }
    }
    return NULL;
}

int update_battery_temperature(int temp)
{
    if (temp != g_charger_manager.battery_temp) {
        g_charger_manager.battery_temp = temp;
        return check_temp_event();
    }
    return CHARGER_OK;
}

int send_charger_msg(charger_msg_t msg)
{
    mqd_t mq;
    struct mq_attr attr;
    int ret;

    attr.mq_maxmsg = MQ_MSG_LOAD_MAX;
    attr.mq_msgsize = sizeof(charger_msg_t);
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;
    mq = mq_open(MQ_MSG_NAME, O_RDWR | O_CREAT | O_NONBLOCK | O_CLOEXEC, 0644, &attr);
    if (mq == (mqd_t)CHARGER_FD_INVAILD) {
        chargererr("%s:open mq failed;\r\n", __FUNCTION__);
        return CHARGER_FAILED;
    }
    ret = mq_send(mq, (const char*)&msg, sizeof(msg), 1);
    mq_close(mq);
    return ret;
}

int main(int argc, FAR char* argv[])
{
    int ret;

    chargerinfo("in chargerd main!\r\n");
    ret = charger_manager_init();
    if (ret < 0) {
        return ret;
    }

    charger_event_engine_start();
    charger_manager_unit();
    return CHARGER_FAILED;
}
