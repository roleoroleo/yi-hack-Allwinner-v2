#
#  This file is part of mqttv4 (https://github.com/TheCrypt0/mqttv4).
#  Copyright (c) 2018-2019 Davide Maggioni.
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 3.
# 
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#

CC=arm-openwrt-linux-gcc
USER_CFLAGS=-mcpu=cortex-a7 -mfpu=neon-vfpv4 -I/opt/yi/toolchain-sunxi-musl/toolchain/include -L/opt/yi/toolchain-sunxi-musl/toolchain/lib
USER_LDFLAGS=
AR=arm-openwrt-linux-ar
RANLIB=arm-openwrt-linux-ranlib
STRIP=arm-openwrt-linux-strip
