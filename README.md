# Overview of chargerd

\[ English | [简体中文](README_zh-cn.md) \]

Charging management is mainly used for monitoring and managing the battery charging process, ensuring safe and fast charging of the battery. Currently, many IoT products come with batteries, and the charging circuits and chips have a certain degree of reusability. Therefore, implementing a universal charging management framework that integrates the characteristics of different charging circuits and chips enhances software reusability and maintainability.

## Configuration Application of chargerd
The chargerd application in NuttX requires the following configuration.

### Configuration for chargerd
To enable the chargerd service, the following configurations need to be set:
```shell
# Enable the chargerd service
CONFIG_CHARGERD=y
# If battery data from the fuel gauge requires b16 conversion, enable this configuration
CONFIG_CHARGERD_HWINTF_CONVERSION=y
# Default path for the chargerd configuration file, fill in the correct value if changed
CONFIG_CHARGERD_CONFIGURATION_FILE_PATH=/etc/chargerd_parameters.json
```

### Starting chargerd
Add the startup process for the chargerd service in the startup script rcS as follows:
```shell
#ifdef CONFIG_CHARGERD
chargerd &
#endif
```

### Packaging chargerd
Include the configuration file for chargerd in the ROMFS packaging script:
```shell
ifeq ($(CONFIG_CHARGERD),y)
RCRAWS += etc/charger_parameters.json
endif
```

## Configuration File for chargerd
The chargerd configuration file is in JSON format. When chargerd starts, it reads the configuration file and initializes the chargerd service according to the configuration.

### Parameters in Configuration File
| Parameter Name | Parameter Format | Description |
| --- | --- | --- |
| charger_supply | Parameter Value | Adjusts the power supply voltage device node for the charger chip |
| charger_adapter | Parameter Value | Device node for detecting charger plug/unplug events and charging types |
| charger | Parameter Value | Device node for the charging chip, multiple chips separated by ';' |
| fuel_gauge | Parameter Value | Device node for the battery fuel gauge |
| algo | Parameter Value | Charging algorithm used by the charging chip, multiple chips separated by ';' |
| polling_interval_ms | Parameter Value | Self-check timer polling interval |
| fullbatt_capacity | Parameter Value | Full charge condition (%) |
| fullbatt_current | Parameter Value | 	Full charge current condition (mA) |
| fullbatt_duration_ms | Parameter Value | Recovery time after full charge cutoff (ms) |
| fault_duration_ms | Parameter Value | Recovery time after fault protection (ms) |
| temp_min | Parameter Value | Low temperature protection threshold (0.1 Celsius) |
| temp_min_r | Parameter Value | Low temperature protection recovery threshold (0.1 Celsius) |
| temp_max | Parameter Value | High temperature protection threshold (0.1 Celsius) |
| temp_max_r | Parameter Value | High temperature protection recovery threshold (0.1 Celsius) |
| temp_skin | Parameter Value | Thermal shell temperature overheat threshold (0.1 Celsius) |
| temp_skin_r | Parameter Value | Thermal shell temperature overheat recovery threshold (0.1 Celsius) |
| enable_delay_ms | Parameter Value | Delay time (ms) for normal access after adapter is enabled |
| temp_rise_hys | Parameter Value | Temperature rise hysteresis value (0.1 Celsius) |
| temp_fall_hys | Parameter Value | Temperature fall hysteresis value (0.1 Celsius) |
| vol_rise_hys | Parameter Value | Voltage rise hysteresis value (mV) |
| vol_fall_hys | Parameter Value | Voltage fall hysteresis value (mV) |
| battery_default_param | Default battery parameters, see template | Information configured as battery when unable to obtain battery info via ioctl |
| charger_fault_plot_table | Charging error values, see template | Used when charging errors occur; parameters represent: {temperature range start (0.1 Celsius), temperature range end (0.1 Celsius), voltage range start (mV), voltage range end (mV), charger index (0 start, -1 indicates no charging), charging current (mA), charging voltage (mV)} |
| Charging Curve Table | Charging curve table, see template | 1. name: name of the charging curve table; 2. mask: value obtained from left shift operation based on 1, e.g., 1 << 3, then mask is 8, indicating charging protocol corresponding to kernel data structure battery_protocol_e; 3. element_num: number of elements in the charging curve table; 4. your_plot_name_xxx: custom name for the charging curve table, must match the name parameter filled after the above name; 5. Curve table elements: placed after brackets, e.g., [-500,0,0,65535,-1,0,0], if there are multiple elements, write them in similar lines. |
| temperature_termination_voltage_table | 	Voltage table adjusted dynamically according to temperature | Cut-off voltage values adjusted dynamically based on temperature: 1. temp_vterm_enable: whether to enable this function; 2. temp_rise_hys: temperature rise hysteresis value; 3. temp_fall_hys: temperature fall hysteresis value; 4. relation_table: relationship between temperature range and cut-off voltage, e.g., [-100,0,3000], indicates that when the temperature is in the range of -10℃ ~ 0℃, the cut-off voltage is set to 3000mV. |

### Example of the chargerd Configuration File
Example configuration for chargerd:
```json
{
    "charger_supply" : "/dev/charge/wireless_rx",
    "charger_adapter" : "/dev/charge/wireless_rx",
    "charger" : "/dev/charge/batt_charger;/dev/charge/charger_pump",
    "fuel_gauge" : "/dev/charge/batt_gauge",
    "algo" : "buck;pump",

    "polling_interval_ms" : 1000,
    "fullbatt_capacity" : 100,
    "fullbatt_current" : 0,
    "fullbatt_duration_ms" : 180000,
    "fault_duration_ms" : 60000,
    "temp_min" : 10,
    "temp_min_r" : 20,
    "temp_max" : 450,
    "temp_max_r" : 440,
    "temp_skin" : 400,
    "temp_skin_r" : 390,
    "temp_rise_hys" : 10,
    "temp_fall_hys" : 10,
    "vol_rise_hys" : 10,
    "vol_fall_hys" : 10,
    "enable_delay_ms" : 3000,
    "battery_default_param" : [
        {
            "capacity" : 0,
            "current" : 0,
            "temp" : 110,
            "vol" : 2100
        }
    ],

    "charger_fault_plot_table" : [
        {
            "temp_range_min" : 160,
            "temp_range_max" : 449,
            "vol_range_min" : 2100,
            "vol_range_max" : 65535,
            "charger_index" : 0,
            "work_current" : 145,
            "supply_vol" : 5500
        }
    ],

    "charger_plot_table_list" : [
        {
            "name" : "g_charger_plot_table",
            "mask" : 8,
            "element_num" : 15,
            "g_charger_plot_table" : [
                [-500,0,0,65535,-1,0,0],
                [1,119,2100,3000,0,20,5500],
                [1,119,3000,4435,0,145,5500],
                [1,119,4435,65535,0,145,5500],
                [120,159,2100,3000,0,20,5500],
                [120,159,3000,3600,0,145,5500],
                [120,159,3600,4140,0,486,5500],
                [120,159,4140,4435,0,340,5500],
                [120,159,4435,65535,0,340,5500],
                [160,449,2100,3000,0,20,5500],
                [160,449,3000,3650,0,145,5500],
                [160,449,3650,4140,1,920,0],
                [160,449,4140,4435,0,729,5500],
                [160,449,4435,65535,0,729,5500],
                [450,65535,0,65535,-1,0,0]
            ]
        },
        {
            "name" : "g_charger_plot_table_noStand_type3",
            "mask" : 4,
            "element_num" : 9,
            "g_charger_plot_table_noStand_type3" : [
                [-500,0,0,65535,-1,0,0],
                [1,159,2100,3000,0,20,5500],
                [1,159,3000,4435,0,145,5500],
                [1,159,4435,65535,0,145,5500],
                [160,449,2100,3000,0,20,5500],
                [160,449,3000,3600,0,145,5500],
                [160,449,3600,4435,0,486,5500],
                [160,449,4435,65535,0,486,5500],
                [450,65535,0,65535,-1,0,0]
            ]
        },
        {
            "name" : "g_charger_plot_table_noStand",
            "mask" : 3,
            "element_num" : 4,
            "g_charger_plot_table_noStand" : [
                [-500,0,0,65535,-1,0,0],
                [1,39,2100,65535,0,140,5500],
                [40,449,2100,65535,0,150,5500],
                [450,65535,0,65535,-1,0,0]
            ]
        }
    ]

    "temperature_termination_voltage_table" : [
        {
            "temp_vterm_enable" : 1,
            "temp_rise_hys" : 20,
            "temp_fall_hys" : 20,
            "relation_table" : [
                [-100,0,3000],
                [1,100,3000],
                [101,150,4000],
                [151,450,4400],
                [451,600,4000]
            ]
        }
    ]
}
```

## Code Locations
The chargerd framework involves related code:
```shell
# Driver
Latest community code：nuttx/drivers/power/battery

# Healthd
frameworks/healthd/

# Chargerd
frameworks/charger

# uORB topic definition
frameworks/topics/include/system/state.h
```