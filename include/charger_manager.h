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

#ifndef __CHARGER_MANAGER_H
#define __CHARGER_MANAGER_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "charger_algo.h"
#include "charger_desc.h"
#include <debug.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
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

#ifdef CONFIG_CHARGERD_PM
#include <nuttx/power/pm.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define CHARGER_INDEX_INVAILD -1
#define CHARGER_FD_INVAILD -1
#define CHARGER_ALGO_BUCK 0
#define LOG_TAG "[CHARGERD]"
#define MQ_MSG_NAME "charger_events"
#define MQ_MSG_LOAD_MAX (10)

#define CHARGER_DEBUG_LOG_EN 0

#if (CHARGER_DEBUG_LOG_EN == 1)
#define chargerdebug(format, ...) \
    syslog(LOG_DEBUG, LOG_TAG EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#else
#define chargerdebug(format, ...)
#endif

#define chargererr(format, ...) \
    syslog(LOG_ERR, LOG_TAG EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#define chargerinfo(format, ...) \
    syslog(LOG_INFO, LOG_TAG EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)
#define chargerwarn(format, ...) \
    syslog(LOG_WARNING, LOG_TAG EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__)

#define chargerassert_noreturn(condition, format, ...)                          \
    do {                                                                        \
        if (condition) {                                                        \
            syslog(LOG_ERR, LOG_TAG EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__); \
        }                                                                       \
    } while (0)

#define chargerassert_return(condition, format, ...)                            \
    do {                                                                        \
        if (condition) {                                                        \
            syslog(LOG_ERR, LOG_TAG EXTRA_FMT format EXTRA_ARG, ##__VA_ARGS__); \
            return -1;                                                          \
        }                                                                       \
    } while (0)

#define is_between(left, right, value)             \
    (((left) >= (right) && (left) >= (value)       \
         && (value) >= (right))                    \
        || ((left) <= (right) && (left) <= (value) \
            && (value) <= (right)))

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct charger_manager;

enum CHARGER_RET_CODE {
    CHARGER_FAILED = -1,
    CHARGER_OK = 0,
};

typedef enum {
    EVENT_HANDLER_HEALTHD,
    EVENT_HANDLER_THERMAL,
    EVENT_HANDLER_STATE,
    EVENT_HANDLER_MAX,
} event_hanlder_e;

typedef enum {
    CHARGER_EVENT_PLUGIN,
    CHARGER_EVENT_PLUGOUT,
    CHARGER_EVENT_CHG_TIMEOUT,
    CHARGER_EVENT_OVERTEMP,
    CHARGER_EVENT_OVERTEMP_RECOVERY,
} charger_event_e;

typedef struct {
    charger_event_e event;
    uint32_t time_gap;
} charger_msg_t;

typedef enum {
    CHARGER_STATE_INIT,
    CHARGER_STATE_CHG,
    CHARGER_STATE_TEMP_PROTECT,
    CHARGER_STATE_FULL,
    CHARGER_STATE_FAULT,
    CHARGER_STATE_MAX,
} charger_state_e;

typedef int (*state_func_t)(struct charger_manager* data, charger_msg_t* event);

/*manager*/
struct charger_manager {
    struct charger_desc desc;
    charger_state_e prestate;
    charger_state_e currstate;
    charger_state_e nextstate;
    bool online;
    int supply_fd;
    int adapter_fd;
    int charger_fd[MAX_CHARGERS];
    struct charger_algo algos[MAX_CHARGERS];
    int gauge_fd;
    int skin_temp;
    int battery_temp;
    bool temp_protect_lock;
    timer_t env_timer_id;
    state_func_t functables[CHARGER_STATE_MAX];
    int epollfd;
    uint64_t fullbatt_timer_cnt;
    uint64_t fault_timer_cnt;
    int curr_charger;
    int protocol;
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

bool is_adapter_exist(void);
bool is_supply_exist(void);
struct charger_plot_parameter* check_charger_plot(int temp, int vol, int type);
int update_battery_temperature(int temp);
int send_charger_msg(charger_msg_t msg);
#endif
