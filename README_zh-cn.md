# chargerd 概述

\[ [English](README.md) | 简体中文 \]

充电管理主要用来对电池充电过程的监控和管理，保障电池安全快速的充电。目前很多IoT产品都带有电池，并且充电电路和芯片具有一定的复用性，所以实现一个通用充电管理框架，将不同充电电路和芯片特性进行融合，增强软件复用性和可维护。

## chargerd配置应用
NuttX中chargerd应用需要做如下配置。
### chargerd配置
使能chargerd服务，需要打开如下配置：
```shell
# 使能chargerd服务
CONFIG_CHARGERD=y
# 如果从电量计获取的电池数据需要b16转换，则打开该配置
CONFIG_CHARGERD_HWINTF_CONVERSION=y
# chargerd配置文件默认路径, 如有改变，填写正确值
CONFIG_CHARGERD_CONFIGURATION_FILE_PATH=/etc/chargerd_parameters.json
```

### chargerd启动
在启动脚本rcS中添加chargerd服务的启动流程，如下：
```shell
#ifdef CONFIG_CHARGERD
chargerd &
#endif
```

### chargerd打包
在romfs打包脚本中加入chargerd的配置文件
```shell
ifeq ($(CONFIG_CHARGERD),y)
RCRAWS += etc/charger_parameters.json
endif
```

## chargerd配置文件
chargerd配置文件为json格式，chargerd启动时会读取chargerd配置文件，并根据配置文件的配置，初始化chargerd服务。

### chargerd配置文件参数
| 参数名 | 参数格式 | 备注说明 |
| --- | --- | --- |
| charger_supply | 参数值 | 调节charger芯片供电电压设备节点 |
| charger_adapter | 参数值 | 检测充电器拔插事件以及充电类型的设备节点 |
| charger | 参数值 | 充电芯片的设备节点，多个芯片使用';'分割 |
| fuel_gauge | 参数值 | 电池电量计的设备节点 |
| algo | 参数值 | 充电芯片采用的充电算法，多个芯片使用';'分割 |
| polling_interval_ms | 参数值 | 自检定时器轮询间隔 |
| fullbatt_capacity | 参数值 | 满充电量条件(%) |
| fullbatt_current | 参数值 | 满充电流条件(mA) |
| fullbatt_duration_ms | 参数值 | 满充断充后恢复时间(ms) |
| fault_duration_ms | 参数值 | 异常保护后恢复时间(ms) |
| temp_min | 参数值 | 低温保护阈值(0.1 Celsius) |
| temp_min_r | 参数值 | 低温保护恢复阈值(0.1 Celsius) |
| temp_max | 参数值 | 高温保护阈值(0.1 Celsius) |
| temp_max_r | 参数值 | 高温保护恢复阈值(0.1 Celsius) |
| temp_skin | 参数值 | thermal板壳温度超温阈值(0.1 Celsius) |
| temp_skin_r | 参数值 | thermal板壳温度超温恢复阈值(0.1 Celsius) |
| enable_delay_ms | 参数值 | adapter使能后正常访问delay时间(ms) |
| temp_rise_hys | 参数值 | 温度上升迟滞值(0.1 Celsius) |
| temp_fall_hys | 参数值 | 温度下降迟滞值(0.1 Celsius) |
| vol_rise_hys | 参数值 | 电压上升迟滞值(mV) |
| vol_fall_hys | 参数值 | 电压下降迟滞值(mV) |
| battery_default_param | 电池默认参数值，参见模版 | 无法通过ioctl获取电池信息时，将此参数配置的信息作为电池 |
| charger_fault_plot_table | 充电错误采用值，参见模版 | 充电出现错误时采用，充电参数依次表示为：{温度范围开始(0.1 Celsius)，温度范围结(0.1 Celsius)束，电压范围开始(mv)，电压范围结束(mV)，充电芯片索引(0开始，-1表示不充电)，充电电流(mA), 充电电压(mV)} |
| 充电曲线表 | 充电曲线表，参见模版 | 1. name：表示充电曲线表的名字；2. mask：表示支持的以1为基准的左移运算得到的值，例如1 << 3，则mask为8，表示充电protocol，对应内核数据结构battery_protocol_e; 3. element_num：表示该充电曲线表的元素数量。4. your_plot_name_xxx：自定义命名的充电曲线表的名字，需要注意的是，需要与上述name之后填充的name参数一致。5. 曲线表元素：都放在中括号之后，例如[-500,0,0,65535,-1,0,0]，当有多个元素时，类似的写多行。 |
| temperature_termination_voltage_table | 根据温度动态调整电压表 | 截止电压值根据温度动态调整的表：1. temp_vterm_enable：是否使能该功能；2. temp_rise_hys：温度上升迟滞值；3. temp_fall_hys：温度下降迟滞值；4. relation_table：温度范围与截止电压对应关系，比如 [-100,0,3000]，表示当温度在-10℃ ~ 0℃范围内，截止电压设置为3000mV。 |

### chargerd配置文件示例
charged配置案例：
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

## 代码位置
chargerd框架涉及相关代码。
```shell
# Driver
社区最新代码：nuttx/drivers/power/battery

# Healthd
frameworks/healthd/

# Chargerd
frameworks/charger

# uORB topic definition
frameworks/topics/include/system/state.h
```