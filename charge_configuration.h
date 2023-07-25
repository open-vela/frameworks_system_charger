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

#ifndef __INCLUDE_CHARGE_CONFIGURATION_H
#define __INCLUDE_CHARGE_CONFIGURATION_H

#ifndef BIT
#if defined(_ASMLANGUAGE)
#define BIT(n) (1 << (n))
#else
#define BIT(n) (1U << (n))
#endif
#endif

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

#endif /* __INCLUDE_CHARGE_CONFIGURATION_H */
