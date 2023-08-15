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

#ifndef __CHARGER_STATEMACHINE_H
#define __CHARGER_STATEMACHINE_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "charger_manager.h"

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

void charger_wakup(void);
void charger_sleep(void);
void charger_delay(unsigned int delay_ms);
int charger_timer_stop(timer_t* timer);
int charger_timer_start(timer_t* timer, time_t poll_interval);
void init_state_func_tables(struct charger_manager* manager);
int charger_statemachine_state_run(struct charger_manager* data,
    charger_msg_t* event, bool* changed);
#endif
