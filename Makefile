#
# Copyright (C) 2023 Xiaomi Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

include $(APPDIR)/Make.defs

PROGNAME = $(CONFIG_CHARGERD_PROGNAME)
PRIORITY = $(CONFIG_CHARGERD_PRIORITY)
STACKSIZE = $(CONFIG_CHARGERD_STACKSIZE)

MAINSRC = charge_manager.c
CSRCS += charge_zone_user.c

CFLAGS += ${shell $(INCDIR) $(INCDIROPT) "$(CC)" $(APPDIR)/external/uORB}

include $(APPDIR)/Application.mk
