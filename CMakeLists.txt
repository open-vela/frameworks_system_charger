#
# Copyright (C) 2021 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
if(CONFIG_CHARGERD)

  set(CSRCS charger_manager.c charger_statemachine.c charger_hwintf.c
            charger_algo.c charger_desc.c)

  set(INCDIR ${CMAKE_CURRENT_LIST_DIR}/include
             ${NUTTX_APPS_DIR}/netutils/cjson/cJSON)

  nuttx_add_application(
    NAME
    ${CONFIG_CHARGERD_PROGNAME}
    STACKSIZE
    ${CONFIG_CHARGERD_STACKSIZE}
    PRIORITY
    ${CONFIG_CHARGERD_PRIORITY}
    SRCS
    ${CSRCS}
    INCLUDE_DIRECTORIES
    ${INCDIR})

endif()
