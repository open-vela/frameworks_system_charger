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

#include <nuttx/config.h>

#include <fcntl.h>
#include <inttypes.h>
#include <nuttx/power/battery_charger.h>
#include <nuttx/power/battery_gauge.h>
#include <nuttx/power/battery_ioctl.h>
#include <nuttx/power/battery_monitor.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <system/state.h>
#include <uORB/uORB.h>
#include <unistd.h>
#ifdef CONFIG_PM
#include <nuttx/power/pm.h>
#endif
#include "charger_manager.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum {
    CHARGERD_TEST_CMD_INVAILD = -1,
    BATTERY_TEST_CMD = 0,
    THERMAL_TEST_CMD,
    MSG_TEST_CMD,
    STRESS_BATTERY_TEST_CMD,
    STRESS_CHARGER_TEST_CMD,
    STRESS_ADAPTER_TEST_CMD,
    STRESS_SUPPLY_TEST_CMD,
    CHARGERD_TEST_MAX,
};

typedef int (*stress_func_t)(int fd);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void usage(void)
{
    printf("chargerdtool [arguments...] <command>\n");
    printf("\t[-h      ]  chargerdtool commands help\n");
    printf("\t[-t   val]  0:publish battery topic\n"
           "\t            1:publish thermal topic\n"
           "\t            2:send a chargerd msg\n"
           "\t            3:stress test a battery\n"
           "\t            4:stress test a charger\n"
           "\t            5:stress test a adapter\n"
           "\t            6:stress test a supply\n");
    printf(" command:\n");
    printf("\t <content> ex:\n"
           "\t state level temperature if cmd == 0\n"
           "\t temperature if cmd == 1\n"
           "\t msgtype if cmd == 2\n"
           "\t dev count interval(ms) if cmd == 3|4|5|6\n");
}

static void battery_topic_test(char* argv[])
{
    struct battery_state data;

    data.state = strtoul(argv[0], NULL, 10);
    data.level = strtoul(argv[1], NULL, 10);
    data.temp = strtoul(argv[2], NULL, 10);
    if (orb_publish_auto(ORB_ID(battery_state), NULL, &data, NULL) < 0) {
        printf("battery state publish err\n");
    }
}

static void thermal_topic_test(char* argv[])
{
    struct device_temperature data;

    data.skin = strtoul(argv[0], NULL, 10);
    if (orb_publish_auto(ORB_ID(device_temperature), NULL, &data, NULL) < 0) {
        printf("battery state publish err\n");
    }
}

static void chargerd_msg_test(char* argv[])
{
    charger_msg_t msg;
    int ret;

    msg.event = strtoul(argv[0], NULL, 10);
    ret = send_charger_msg(msg);
    if (ret < 0) {
        printf("send msg %d failed\n", msg.event);
    }
}

static int stress_battery_test(int fd)
{
    int ret;
    unsigned long test;

    ret = ioctl(fd, BATIOC_VOLTAGE, (unsigned long)((uintptr_t)&test));
    ret |= ioctl(fd, BATIOC_CAPACITY, (unsigned long)((uintptr_t)&test));
    ret |= ioctl(fd, BATIOC_TEMPERATURE, (unsigned long)((uintptr_t)&test));
    ret |= ioctl(fd, BATIOC_CURRENT, (unsigned long)((uintptr_t)&test));
    ret |= ioctl(fd, BATIOC_STATE, (unsigned long)((uintptr_t)&test));
    return ret;
}

static int stress_charger_test(int fd)
{
    struct batio_operate_msg_s msg;
    int ret;
    static int en = 0;
    int current = 0;

    msg.operate_type = BATIO_OPRTN_CHARGE;
    msg.u32 = en;
    en = !en;

    ret = ioctl(fd, BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    ret |= ioctl(fd, BATIOC_CURRENT, (unsigned long)((uintptr_t)&current));
    return ret;
}

static int stress_adapter_test(int fd)
{
    struct batio_operate_msg_s msg;
    int ret;
    static int en = 0;
    unsigned long test;

    msg.operate_type = BATIO_OPRTN_CHARGE;
    msg.u32 = en;
    en = !en;

    ret = ioctl(fd, BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    ret |= ioctl(fd, BATIOC_GET_PROTOCOL, (unsigned long)((uintptr_t)&test));

    return ret;
}

static int stress_supply_test(int fd)
{
    int ret;
    int vol = 3000;

    ret = ioctl(fd, BATIOC_VOLTAGE, (unsigned long)((uintptr_t)&vol));
    ret |= ioctl(fd, BATIOC_GET_VOLTAGE, (unsigned long)((uintptr_t)&vol));
    return ret;
}

static void stress_dev_test(stress_func_t fn, char* argv[])
{
    int fd;
    unsigned long cnt;
    unsigned long fail = 0;
    int interval;

    fd = open(argv[0], O_RDONLY);
    if (fd < 0) {
        printf("open dev %s failed (%d)\n", argv[0], -errno);
        return;
    }
    cnt = strtoul(argv[1], NULL, 10);
    if (cnt <= 0) {
        printf("invaild cnt %s (%d)\n", argv[1], -errno);
        return;
    }
    interval = strtoul(argv[2], NULL, 10);
    if (cnt <= 0) {
        printf("invaild cnt %s (%d)\n", argv[1], -errno);
        return;
    }

    for (int c = 0; c < cnt; c++) {
        if (fn(fd) < 0) {
            fail++;
        }
        usleep(interval * 1000);
    }
    printf("test %ld times, fail %ld times\n", cnt, fail);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, char* argv[])
{
    struct pm_wakelock_s wakelock;
    int cmd = CHARGERD_TEST_CMD_INVAILD;
    stress_func_t fn = NULL;
    int ret;

    if (argc <= 1) {
        usage();
        return -EINVAL;
    }

    while ((ret = getopt(argc, argv, "t:h")) != EOF) {
        switch (ret) {
        case 't':
            cmd = strtoul(optarg, NULL, 10);
            break;
        case 'h':
        default:
            usage();
            return -EINVAL;
        }
    }

    if (optind >= argc) {
        usage();
        return -EINVAL;
    }

#ifdef CONFIG_PM
    pm_wakelock_init(&wakelock, "chargerdtool", PM_IDLE_DOMAIN, PM_NORMAL);
#endif
    switch (cmd) {
    case BATTERY_TEST_CMD:
        if (argc - optind < 3) {
            usage();
            ret = -EINVAL;
            break;
        }
        battery_topic_test(argv + optind);
        break;
    case THERMAL_TEST_CMD:
        if (argc - optind < 1) {
            usage();
            ret = -EINVAL;
            break;
        }
        thermal_topic_test(argv + optind);
        break;
    case MSG_TEST_CMD:
        if (argc - optind < 1) {
            usage();
            ret = -EINVAL;
            break;
        }
        chargerd_msg_test(argv + optind);
        break;
    case STRESS_BATTERY_TEST_CMD:
        fn = stress_battery_test;
        break;
    case STRESS_CHARGER_TEST_CMD:
        fn = stress_charger_test;
        break;
    case STRESS_ADAPTER_TEST_CMD:
        fn = stress_adapter_test;
        break;
    case STRESS_SUPPLY_TEST_CMD:
        fn = stress_supply_test;
        break;
    default:
        printf("unkown cmd\n");
        usage();
        ret = -EINVAL;
        break;
    }

    if (fn) {
        if (argc - optind < 3) {
            usage();
            ret = -EINVAL;
        } else {
#ifdef CONFIG_PM
            pm_wakelock_stay(&wakelock);
#endif
            stress_dev_test(fn, argv + optind);
#ifdef CONFIG_PM
            pm_wakelock_relax(&wakelock);
#endif
        }
    }

#ifdef CONFIG_PM
    pm_wakelock_uninit(&wakelock);
#endif
    return ret;
}
