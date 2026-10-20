#!/bin/sh

# Force night mode by patching the day/night loop (auto_loop_func) of the
# running rmm process in memory. Nothing is written to flash: a reboot or a
# restart of rmm restores the original behaviour.
#
# The loop compares the image brightness with two thresholds:
#   cmp r9, #79  / bgt -> brightness is high, don't count as dark
#   cmp r9, #560 / blt -> brightness is low, stay in night mode
# The patch replaces the first jump with a nop and makes the second one
# unconditional, so the camera always switches to (and stays in) night mode.
#
# Usage: force_night.sh on|off|status|boot

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CAMERA_CONF_FILE="$YI_HACK_PREFIX/etc/camera.conf"
RMM=/home/app/rmm

# Supported rmm builds: "md5 addr1 orig1 patch1 addr2 orig2 patch2"
# Values are 32-bit little endian ARM instructions.
PATCHES="
4485b86c7aae2fd01dc1d3986be2f7e7 0x180e8 ca00003e e1a00000 0x18258 ba000047 ea000047
"

get_camera_config()
{
    grep -w $1 $CAMERA_CONF_FILE | cut -d "=" -f2-
}

set_camera_config()
{
    if grep -q "^$1=" $CAMERA_CONF_FILE; then
        sed -i "s/^\($1\s*=\s*\).*$/\1$2/" $CAMERA_CONF_FILE
    else
        echo "$1=$2" >> $CAMERA_CONF_FILE
    fi
}

rmm_pid()
{
    ps | grep "[.]/rmm" | awk 'NR==1 {print $1}'
}

# read_word PID ADDR -> prints the 32-bit word as 8 hex digits
read_word()
{
    dd if=/proc/$1/mem bs=4 count=1 skip=$(($2/4)) 2>/dev/null | hexdump -v -e '1/4 "%08x"'
}

# write_word PID ADDR HEX
write_word()
{
    W=$3
    printf "\x${W:6:2}\x${W:4:2}\x${W:2:2}\x${W:0:2}" | dd of=/proc/$1/mem bs=4 count=1 seek=$(($2/4)) conv=notrunc 2>/dev/null
}

load_patch()
{
    if [ ! -f /tmp/rmm.md5 ]; then
        md5sum $RMM | cut -d ' ' -f1 > /tmp/rmm.md5
    fi
    MD5=$(cat /tmp/rmm.md5)
    PATCH=$(echo "$PATCHES" | grep "^$MD5 ")
    if [ -z "$PATCH" ]; then
        return 1
    fi
    set -- $PATCH
    A1=$2; O1=$3; P1=$4; A2=$5; O2=$6; P2=$7
    return 0
}

# Prints on, off or unknown
patch_state()
{
    W1=$(read_word $PID $A1)
    W2=$(read_word $PID $A2)
    if [ "$W1" == "$P1" ] && [ "$W2" == "$P2" ]; then
        echo "on"
    elif [ "$W1" == "$O1" ] && [ "$W2" == "$O2" ]; then
        echo "off"
    else
        echo "unknown"
    fi
}

# Toggle the IR setting so rmm leaves night mode immediately when the
# patch is removed (with IR off rmm forces day mode).
kick_day_mode()
{
    if [ "$(get_camera_config IR)" != "no" ]; then
        ipc_cmd -i off
        sleep 3
        ipc_cmd -i on
    fi
}

export PATH=$PATH:$YI_HACK_PREFIX/bin:$YI_HACK_PREFIX/usr/bin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:$YI_HACK_PREFIX/lib

if ! load_patch; then
    echo "unsupported"
    exit 1
fi

if [ "$1" == "boot" ]; then
    # Wait for rmm, then apply the saved state if it must persist
    N=0
    while [ -z "$(rmm_pid)" ] && [ $N -lt 60 ]; do
        sleep 1
        N=$((N+1))
    done
    if [ "$(get_camera_config FORCE_NIGHT_PERSIST)" == "yes" ] && [ "$(get_camera_config FORCE_NIGHT)" == "yes" ]; then
        set -- on
    else
        set_camera_config FORCE_NIGHT no
        exit 0
    fi
fi

PID=$(rmm_pid)
if [ -z "$PID" ]; then
    echo "rmm not running"
    exit 1
fi

STATE=$(patch_state)

case "$1" in
    on)
        if [ "$STATE" == "off" ]; then
            write_word $PID $A1 $P1
            write_word $PID $A2 $P2
            STATE=$(patch_state)
        fi
        if [ "$STATE" == "on" ]; then
            set_camera_config FORCE_NIGHT yes
        fi
        echo $STATE
        ;;
    off)
        if [ "$STATE" == "on" ]; then
            write_word $PID $A1 $O1
            write_word $PID $A2 $O2
            STATE=$(patch_state)
            if [ "$STATE" == "off" ]; then
                kick_day_mode
            fi
        fi
        if [ "$STATE" == "off" ]; then
            set_camera_config FORCE_NIGHT no
        fi
        echo $STATE
        ;;
    status)
        echo $STATE
        ;;
    *)
        echo "Usage: $0 on|off|status|boot"
        exit 1
        ;;
esac

[ "$STATE" != "unknown" ]
