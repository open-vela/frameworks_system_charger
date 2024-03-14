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

#include "charger_hwintf.h"
#include "charger_statemachine.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: enable_adapter()
 *
 * Description:
 *   enable/disbale the adapter
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   enable - true:enable  false:disable
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int enable_adapter(struct charger_manager* manager, bool enable)
{
    int ret;
    struct batio_operate_msg_s msg;

    if (manager->adapter_fd < 0) {
        chargererr("Error: adapter not exsit\n");
        return CHARGER_FAILED;
    }
    msg.operate_type = (enable) ? BATIO_OPRTN_SYSON : BATIO_OPRTN_SYSOFF;
    ret = ioctl(manager->adapter_fd, BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", errno);
        return CHARGER_FAILED;
    }

    if (manager->desc.enable_delay_ms > 0 && enable) {
        charger_delay(manager->desc.enable_delay_ms);
    }
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_adapter_type()
 *
 * Description:
 *   get the adapter charger protocol type
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   type - pointer to save protocol type value
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_adapter_type(struct charger_manager* manager, int* type)
{
    int adapter = 0;
    int ret;

    if (manager->adapter_fd < 0) {
        chargererr("Error: adapter not exsit\n");
        return CHARGER_FAILED;
    }
    ret = ioctl(manager->adapter_fd, BATIOC_GET_PROTOCOL, (unsigned long)((uintptr_t)&adapter));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_GET_PROTOCOL) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    *type = adapter;
    return CHARGER_OK;
}

/****************************************************************************
 * Name: set_supply_voltage()
 *
 * Description:
 *   set the charger chip supply voltage
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   vol - voltage value (mV)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int set_supply_voltage(struct charger_manager* manager, int vol)
{
    int ret;

    if (manager->supply_fd < 0) {
        chargererr("Error: adapter not exsit\n");
        return CHARGER_FAILED;
    }
    chargerdebug("set supply voltage:%d\n", vol);
    ret = ioctl(manager->supply_fd, BATIOC_VOLTAGE, (unsigned long)((uintptr_t)&vol));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_VOLTAGE) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_supply_voltage
 *
 * Description:
 *   get the charger chip supply voltage
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   vol - pointer to save supply voltage value (mV)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_supply_voltage(struct charger_manager* manager, int* vol)
{
    int ret;

    ret = ioctl(manager->supply_fd, BATIOC_GET_VOLTAGE, (unsigned long)((uintptr_t)vol));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_VOLTAGE) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    return CHARGER_OK;
}

/****************************************************************************
 * Name: enable_charger
 *
 * Description:
 *   enable/disbale the charger
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   enable - true:enable  false:disable
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int enable_charger(struct charger_manager* manager, int seq, bool enable)
{
    int ret;
    int index;
    struct batio_operate_msg_s msg;

    if (seq >= manager->desc.chargers && manager->charger_fd[seq] < 0) {
        chargererr("Error: charger not exsit\n");
        return CHARGER_FAILED;
    }

    if (enable) {
        for (index = 0; index < manager->desc.chargers; index++) {
            if (seq != index) {
                ret = enable_charger(manager, index, false);
                if (ret < 0) {
                    chargererr("Error: disable charger %d failed (%d)\n", index, ret);
                    return CHARGER_FAILED;
                }
            }
        }
    }

    msg.operate_type = BATIO_OPRTN_CHARGE;
    msg.u32 = enable ? 1 : 0;

    ret = ioctl(manager->charger_fd[seq], BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    return CHARGER_OK;
}

/****************************************************************************
 * Name: set_charger_current
 *
 * Description:
 *   set the charger max current
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   seq - the index of the charger
 *   current - the max charge current(mA)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int set_charger_current(struct charger_manager* manager, int seq, int current)
{
    int ret;

    if (seq >= manager->desc.chargers && manager->charger_fd[seq] < 0) {
        chargererr("Error: charger not exsit\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->charger_fd[seq], BATIOC_CURRENT, (unsigned long)((uintptr_t)&current));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_CURRENT) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_charger_state
 *
 * Description:
 *   get the charger state
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   seq - the index of the charger
 *   state - save the charger state
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_charger_state(struct charger_manager* manager, int seq, unsigned int* state)
{
    int ret;
    unsigned int charger_state;

    if (seq >= manager->desc.chargers && manager->charger_fd[seq] < 0) {
        chargererr("Error: charger not exsit\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->charger_fd[seq], BATIOC_STATE, (unsigned long)((uintptr_t)&charger_state));
    if (ret < 0) {
        chargererr("ERROR: ioctl(BATIOC_STATE) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    chargerdebug("charger_state = 0x%X\n", charger_state);
    *state = charger_state;
    return CHARGER_OK;
}

int get_adapter_type_by_charger(struct charger_manager* manager, int* type)
{
    int adapter = 0;
    int ret;

    if (manager->charger_fd[0] < 0) {
        chargererr("Error: charger not exsit\n");
        return CHARGER_FAILED;
    }
    ret = ioctl(manager->charger_fd[0], BATIOC_GET_PROTOCOL, (unsigned long)((uintptr_t)&adapter));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_GET_PROTOCOL) failed: %d\n", errno);
        return CHARGER_FAILED;
    }
    *type = adapter;
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_battery_voltage
 *
 * Description:
 *   get the battery voltage
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   voltage - the pointer to save battery voltage(mV)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_battery_voltage(struct charger_manager* manager, int* voltage)
{
    int ret;
    bool gauge_inited;
    b16_t vol = 0;

    if (voltage == NULL) {
        chargererr("Error: voltage is invaild\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->gauge_fd, BATIOC_ONLINE, (unsigned long)((uintptr_t)(&gauge_inited)));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", errno);
        goto battery_default;
    }

    if (gauge_inited) {
        ret = ioctl(manager->gauge_fd, BATIOC_VOLTAGE, (unsigned long)((uintptr_t)(&vol)));
        if (ret < 0) {
            chargererr("ERROR: ioctl(BATIOC_VOLTAGE) failed: %d\n", errno);
            goto battery_default;
        }

#ifdef CONFIG_CHARGERD_HWINTF_CONVERSION
        *voltage = b16tof(vol) * 1000;
#else
        *voltage = vol;
#endif
    } else {
        chargererr("gauge has not been initialized successfully\n");
        goto battery_default;
    }

    return CHARGER_OK;

battery_default:
    *voltage = manager->desc.default_param.vol;
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_battery_capacity
 *
 * Description:
 *   get the battery capacity
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   capacity - the pointer to save battery capacity(%)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_battery_capacity(struct charger_manager* manager, int* capacity)
{
    int ret = 0;
    bool gauge_inited;
    b16_t cap = 0;

    if (capacity == NULL) {
        chargererr("Error: capacity is invaild\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->gauge_fd, BATIOC_ONLINE, (unsigned long)((uintptr_t)(&gauge_inited)));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", errno);
        goto battery_default;
    }

    if (gauge_inited) {
        ret = ioctl(manager->gauge_fd, BATIOC_CAPACITY, (unsigned long)((uintptr_t)(&cap)));
        if (ret < 0) {
            chargererr("ERROR: ioctl(BATIOC_CAPACITY) failed: %d\n", errno);
            goto battery_default;
        }

#ifdef CONFIG_CHARGERD_HWINTF_CONVERSION
        *capacity = b16toi(cap);
#else
        *capacity = cap;
#endif
    } else {
        chargererr("gauge has not been initialized successfully\n");
        goto battery_default;
    }

    return CHARGER_OK;

battery_default:
    *capacity = manager->desc.default_param.capacity;
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_battery_temp
 *
 * Description:
 *   get the battery temperature
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   val - the pointer to save battery temperature(0.1 Celsius)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_battery_temp(struct charger_manager* manager, int* val)
{
    int ret = 0;
    bool gauge_inited;
    b8_t temp = 0;

    if (val == NULL) {
        chargererr("Error: val is invaild\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->gauge_fd, BATIOC_ONLINE, (unsigned long)((uintptr_t)(&gauge_inited)));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", errno);
        goto battery_default;
    }

    if (gauge_inited) {
        ret = ioctl(manager->gauge_fd, BATIOC_TEMPERATURE, (unsigned long)((uintptr_t)(&temp)));
        if (ret < 0) {
            chargererr("ERROR: ioctl(BATIOC_TEMPERATURE) failed: %d\n", errno);
            goto battery_default;
        }

#ifdef CONFIG_CHARGERD_HWINTF_CONVERSION
        *val = b8tof(temp) * 10;
#else
        *val = temp;
#endif
    } else {
        chargererr("gauge has not been initialized successfully\n");
        goto battery_default;
    }

    return CHARGER_OK;

battery_default:
    *val = manager->desc.default_param.temp;
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_battery_current
 *
 * Description:
 *   get the battery current
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   cur - the pointer to save battery current(mA)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_battery_current(struct charger_manager* manager, int* cur)
{
    int ret = 0;
    bool gauge_inited;
    b16_t current = 0;

    if (cur == NULL) {
        chargererr("Error:cur is invaild\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->gauge_fd, BATIOC_ONLINE, (unsigned long)((uintptr_t)(&gauge_inited)));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", errno);
        goto battery_default;
    }

    if (gauge_inited) {
        ret = ioctl(manager->gauge_fd, BATIOC_CURRENT, (unsigned long)((uintptr_t)(&current)));
        if (ret < 0) {
            chargererr("ERROR: ioctl(BATIOC_CURRENT) failed: %d\n", errno);
            goto battery_default;
        }

#ifdef CONFIG_CHARGERD_HWINTF_CONVERSION
        *cur = b16toi(current);
#else
        *cur = current;
#endif
    } else {
        chargererr("gauge has not been initialized successfully\n");
        goto battery_default;
    }

    return CHARGER_OK;

battery_default:
    *cur = manager->desc.default_param.current;
    return CHARGER_OK;
}

/****************************************************************************
 * Name: get_battery_status
 *
 * Description:
 *   get the battery status
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   state - the pointer to save battery status(enum battery_status_e)
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int get_battery_status(struct charger_manager* manager, enum battery_status_e* state)
{
    int ret;
    bool gauge_inited;

    if (state == NULL) {
        chargererr("Error: state is invaild\n");
        return CHARGER_FAILED;
    }

    ret = ioctl(manager->gauge_fd, BATIOC_ONLINE, (unsigned long)((uintptr_t)(&gauge_inited)));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_ONLINE) failed: %d\n", errno);
        return CHARGER_FAILED;
    }

    if (gauge_inited) {
        ret = ioctl(manager->gauge_fd, BATIOC_STATE, (unsigned long)((uintptr_t)state));
        if (ret < 0) {
            chargererr("ERROR: ioctl(BATIOC_STATE) failed: %d\n", errno);
        }
    } else {
        chargererr("gauge has not been initialized successfully\n");
        return CHARGER_FAILED;
    }

    return ret;
}

/****************************************************************************
 * Name: set_battery_vbus_state
 *
 * Description:
 *   set vbus state to battery driver
 *
 * Input Parameters:
 *   manager - the struct charger_manager instance
 *   enable - the vbus state
 *
 * Returned Value:
 *    Zero on success or a negated errno value on failure.
 ****************************************************************************/

int set_battery_vbus_state(struct charger_manager* manager, bool enable)
{
    int ret;
    struct batio_operate_msg_s msg;

    msg.operate_type = BATIO_OPRTN_VBUS_STATE;
    msg.u32 = enable;

    ret = ioctl(manager->gauge_fd, BATIOC_OPERATE, (unsigned long)((uintptr_t)&msg));
    if (ret < 0) {
        chargererr("Error: ioctl(BATIOC_OPERATE) failed: %d\n", errno);
    }

    return ret;
}
