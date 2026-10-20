#!/bin/sh

# Force night mode by patching the day/night loop (auto_loop_func) of the
# running rmm process in memory. Nothing is written to flash: a reboot or a
# restart of rmm restores the original behaviour.
#
# The loop compares the image brightness (held in a register, r9 on the
# known builds) with two thresholds:
#   cmp rN, #79  / bgt -> brightness is high, don't count as dark
#   cmp rN, #560 / blt -> brightness is low, stay in night mode
# The patch replaces the first jump with a nop and makes the second one
# unconditional, so the camera always switches to (and stays in) night mode.
#
# Patch sites are looked up in a table of known rmm builds (keyed by md5).
# Unknown builds are searched for the instruction pattern above, assuming:
#   - ARM (not Thumb) code, little endian
#   - each branch immediately follows its cmp
#   - the thresholds are exactly 79 and 560 and both cmp use the same register
#   - "cmp rN, #560 / blt" appears exactly once, and exactly one
#     "cmp rN, #79 / bgt" on the same register precedes it by less than 4 KB
#     (same function)
# If any of these does not hold the build is reported as unsupported. Before
# writing, the words in memory are always checked against the expected
# original (or patched) instructions; on any mismatch nothing is written.
#
# Usage: force_night.sh on|off|status|supported|boot

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CAMERA_CONF_FILE="$YI_HACK_PREFIX/etc/camera.conf"
RMM=/home/app/rmm
SITES_CACHE=/tmp/force_night.sites

# Known rmm builds: "md5 off1 orig1 patch1 off2 orig2 patch2"
# Offsets are file offsets in /home/app/rmm, values are 32-bit little endian
# ARM instructions.
PATCHES="
4485b86c7aae2fd01dc1d3986be2f7e7 0x80e8 ca00003e e1a00000 0x8258 ba000047 ea000047
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

# Search rmm for the patch sites (see the assumptions above).
# Prints "off1 orig1 patch1 off2 orig2 patch2" or nothing.
search_sites()
{
    # Find every "cmp rN, #79" and "cmp rN, #560" (hexdump + grep is much
    # faster than awk on the whole file), then read the following word.
    LIST=""
    for C in $(hexdump -v -e '1/4 "%08x\n"' $RMM | grep -n -E '^e35[0-9a-f]0(04f|e23)$'); do
        N=${C%%:*}
        BR=$(dd if=$RMM bs=4 count=1 skip=$N 2>/dev/null | hexdump -v -e '1/4 "%08x"')
        LIST="$LIST$((N*4)) ${C#*:} $BR
"
    done
    # "cmp rN, #79" alone is common, so anchor on the unique "cmp rN, #560 / blt"
    # and take the only "cmp rN, #79 / bgt" on the same register shortly before it.
    set -- $(echo "$LIST" | awk '
        $2 ~ /0e23$/ && $3 ~ /^ba/ { n2++; o2 = $1; r2 = substr($2, 4, 1); b2 = $3 }
        $2 ~ /004f$/ && $3 ~ /^ca/ { n++; o[n] = $1; r[n] = substr($2, 4, 1); b[n] = $3 }
        END {
            if (n2 != 1)
                exit
            for (i = 1; i <= n; i++)
                if (r[i] == r2 && o[i] < o2 && o2 - o[i] < 4096) { m++; o1 = o[i]; b1 = b[i] }
            if (m == 1)
                print o1, b1, o2, b2
        }')
    [ $# -eq 4 ] || return 1
    printf "0x%x %s e1a00000 0x%x %s ea%s\n" $1 $2 $3 $4 ${4#ba}
}

# Sets OFF1 O1 P1 OFF2 O2 P2, returns 1 if the build is not supported
load_patch()
{
    if [ ! -f /tmp/rmm.md5 ]; then
        md5sum $RMM | cut -d ' ' -f1 > /tmp/rmm.md5
    fi
    MD5=$(cat /tmp/rmm.md5)
    PATCH=$(echo "$PATCHES" | grep "^$MD5 ")
    if [ -z "$PATCH" ]; then
        # The search takes a few seconds, cache the result until reboot
        if ! grep -q "^$MD5 " $SITES_CACHE 2>/dev/null; then
            SITES=$(search_sites)
            echo "$MD5 ${SITES:-none}" > $SITES_CACHE.$$
            mv $SITES_CACHE.$$ $SITES_CACHE
        fi
        PATCH=$(grep "^$MD5 0x" $SITES_CACHE)
    fi
    if [ -z "$PATCH" ]; then
        return 1
    fi
    set -- $PATCH
    OFF1=$2; O1=$3; P1=$4; OFF2=$5; O2=$6; P2=$7
    return 0
}

# Sets A1 A2, the addresses of the patch sites in the rmm process
load_addresses()
{
    set -- $(awk '$NF == "'$RMM'" && $2 ~ /^r-x/ { split($1, r, "-"); print r[1], $3; exit }' /proc/$PID/maps)
    if [ $# -ne 2 ]; then
        return 1
    fi
    A1=$((0x$1 + OFF1 - 0x$2))
    A2=$((0x$1 + OFF2 - 0x$2))
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
    if [ "$1" == "supported" ]; then
        echo "no"
    else
        echo "unsupported"
    fi
    exit 1
fi

if [ "$1" == "supported" ]; then
    echo "yes"
    exit 0
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

if ! load_addresses; then
    echo "unknown"
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
        echo "Usage: $0 on|off|status|supported|boot"
        exit 1
        ;;
esac

[ "$STATE" != "unknown" ]
