#!/bin/sh

#
# Ethernet-only mode monitor.
#
# When ETH_ONLY=yes and the camera has a working Ethernet port, keep Wi-Fi
# disabled: wlan0 is brought down, no DHCP lease is taken and mdnsd never
# advertises a Wi-Fi address.
#
# If the Ethernet link stays down for 5 minutes, Wi-Fi is enabled again as a
# fallback. When the Ethernet link comes back and stays up for 60 seconds,
# Wi-Fi is disabled again.
#
# The stock firmware (dispatch) already prefers eth0, but its Wi-Fi manager
# can give wlan0 an address when Wi-Fi reconnects. That address is then
# advertised by mdnsd as a second service instance (yi-xxxx-2) and clients
# can switch to it, losing the camera when the Wi-Fi link drops.
#
# The monitor runs only while ETH_ONLY=yes: wd.sh starts it when the option
# is switched on and restarts it if it dies, and it restores Wi-Fi and exits
# when the option is switched off. With the option off there is no second
# 10 s loop running next to wd.sh.
#

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CONF_FILE="$YI_HACK_PREFIX/etc/system.conf"
. "$YI_HACK_PREFIX/script/net_info.sh"

PID_FILE="/tmp/net_wd.pid"

INTERVAL=10
ETH_DOWN_LIMIT=30      # 5 minutes
ETH_UP_LIMIT=6         # 60 seconds

WIFI_OFF_FLAG="/tmp/wifi_off"
WIFI_FALLBACK_FLAG="/tmp/wifi_fallback"
LOG_FILE="/tmp/sd/hack_ethonly.log"

ETH_DOWN_COUNT=0
ETH_UP_COUNT=0
ETH_NOIP_COUNT=0
FALLBACK=0

# read_conf KEY -> CONF_VAL
# Shell-only, like wd.sh: the 10 s loop must not fork just to read its own
# setting. Command substitution forks in ash even for functions, so results
# are returned in variables.
read_conf()
{
    CONF_VAL=""
    while IFS='=' read -r K V || [ -n "$K" ]; do
        if [ "$K" == "$1" ]; then
            CONF_VAL=$V
            return 0
        fi
    done < "$CONF_FILE"
    return 1
}

log()
{
    echo "$(date): $1" >> "$LOG_FILE"
    if [ -e "$LOG_FILE" ] ; then
        $YI_HACK_PREFIX/usr/bin/tail -n 100 "$LOG_FILE" > "$LOG_FILE.tmp" && mv "$LOG_FILE.tmp" "$LOG_FILE"
    fi
}

wlan0_has_ip()
{
    ifconfig wlan0 2>/dev/null | grep -q "inet addr:"
}

eth0_has_ip()
{
    ifconfig eth0 2>/dev/null | grep -q "inet addr:"
}

wlan0_dhcp_stop()
{
    for PID in $(ps ww | grep udhcpc | grep wlan0 | grep -v grep | awk '{print $1}') ; do
        kill $PID 2>/dev/null
    done
}

#
# Restart only the eth0 DHCP client. A link flap can drop the address and the
# client with it; the stock firmware does not always bring it back, so the
# camera would otherwise sit on the Wi-Fi fallback with a healthy cable.
#
eth0_dhcp_start()
{
    for PID in $(ps ww | grep udhcpc | grep "i eth0" | grep -v grep | awk '{print $1}') ; do
        kill $PID 2>/dev/null
    done
    HN=$(cat $YI_HACK_PREFIX/etc/hostname 2>/dev/null)
    [ -z "$HN" ] && HN="yi-hack"
    if [ -f /backup/tools/default.script ] ; then
        udhcpc -i eth0 -b -s /backup/tools/default.script -x hostname:$HN >/dev/null 2>&1 &
    elif [ -f /home/app/script/default.script ] ; then
        udhcpc -i eth0 -b -s /home/app/script/default.script -x hostname:$HN >/dev/null 2>&1 &
    fi
}

restart_onvif()
{
    read_conf ONVIF
    if [[ "$CONF_VAL" == "yes" ]] && [[ $($YI_HACK_PREFIX/script/service.sh onvif status) == "started" ]] ; then
        WS_DISCOVERY=0
        read_conf ONVIF_WSDD
        if [[ "$CONF_VAL" == "yes" ]] && [[ $($YI_HACK_PREFIX/script/service.sh wsdd status) == "started" ]] ; then
            WS_DISCOVERY=1
        fi

        $YI_HACK_PREFIX/script/service.sh onvif stop >/dev/null 2>&1
        if [ $WS_DISCOVERY -eq 1 ] ; then
            $YI_HACK_PREFIX/script/service.sh wsdd stop >/dev/null 2>&1
        fi
        $YI_HACK_PREFIX/script/service.sh onvif start >/dev/null 2>&1
        if [ $WS_DISCOVERY -eq 1 ] ; then
            $YI_HACK_PREFIX/script/service.sh wsdd start >/dev/null 2>&1
        fi
    fi
}

wifi_off()
{
    CHANGED=0
    if [ ! -f "$WIFI_OFF_FLAG" ] ; then
        log "Ethernet link up: disabling Wi-Fi"
        CHANGED=1
    fi
    touch "$WIFI_OFF_FLAG"
    rm -f "$WIFI_FALLBACK_FLAG"
    # Stop wlan0 DHCP even if the interface is already down, so a client left
    # over from a fallback cannot take a lease if the stock app brings it up.
    wlan0_dhcp_stop
    WLAN_CFG=$(ifconfig wlan0 2>/dev/null)
    if echo "$WLAN_CFG" | grep -q "inet addr:" || echo "$WLAN_CFG" | grep -q "UP" ; then
        # busybox ifconfig brings the interface up when clearing the address,
        # so bring it down again afterwards.
        ifconfig wlan0 0.0.0.0 2>/dev/null
        ifconfig wlan0 down 2>/dev/null
    fi
    FALLBACK=0
    if [ $CHANGED -eq 1 ] ; then
        restart_onvif
    fi
}

wifi_on()
{
    CHANGED=0
    if [ ! -f "$WIFI_FALLBACK_FLAG" ] ; then
        log "Enabling Wi-Fi fallback"
        CHANGED=1
    fi
    rm -f "$WIFI_OFF_FLAG"
    touch "$WIFI_FALLBACK_FLAG"
    ifconfig wlan0 up 2>/dev/null
    if [ -x /home/base/tools/wpa_cli ] ; then
        /home/base/tools/wpa_cli -i wlan0 reconfigure >/dev/null 2>&1
    fi
    if ! wlan0_has_ip ; then
        $YI_HACK_PREFIX/script/wifidhcp.sh >/dev/null 2>&1
    fi
    FALLBACK=1
    if [ $CHANGED -eq 1 ] ; then
        restart_onvif
    fi
}

wifi_restore()
{
    log "Ethernet-only mode disabled: restoring Wi-Fi"
    rm -f "$WIFI_OFF_FLAG" "$WIFI_FALLBACK_FLAG"
    if ! wlan0_has_ip ; then
        ifconfig wlan0 up 2>/dev/null
        if [ -x /home/base/tools/wpa_cli ] ; then
            /home/base/tools/wpa_cli -i wlan0 reconfigure >/dev/null 2>&1
        fi
        $YI_HACK_PREFIX/script/wifidhcp.sh >/dev/null 2>&1
    fi
    FALLBACK=0
    ETH_DOWN_COUNT=0
    ETH_UP_COUNT=0
    restart_onvif
}

# Only one monitor. wd.sh reads this file to know whether the monitor is
# already running; the kill check makes a stale file harmless.
if [ -f "$PID_FILE" ] ; then
    { read -r OLD_PID < "$PID_FILE"; } 2>/dev/null
    if [ -n "$OLD_PID" ] && kill -0 "$OLD_PID" 2>/dev/null ; then
        exit 0
    fi
fi
echo $$ > "$PID_FILE"
trap 'rm -f "$PID_FILE"' 0

# Give the stock firmware a few seconds to bring the interfaces up.
sleep 15

read_conf ETH_ONLY
if [[ "$CONF_VAL" != "yes" ]] || ! eth_capable ; then
    # wd.sh starts the monitor briefly after the option is turned off, to
    # clear a state left behind by a monitor that could not exit cleanly.
    if [ -f "$WIFI_OFF_FLAG" ] || [ -f "$WIFI_FALLBACK_FLAG" ] ; then
        wifi_restore
    fi
    exit 0
fi

{ read -r CARRIER < /sys/class/net/eth0/carrier; } 2>/dev/null
if [ "$CARRIER" == "1" ] && eth0_has_ip ; then
    # The sync block in system.sh may have created the flag already, so
    # restart ONVIF here even when wifi_off() sees no transition.
    wifi_off
    restart_onvif
else
    # No usable Ethernet at boot: start on Wi-Fi and switch back when the
    # link is up and has an address.
    wifi_on
fi

while true
do
    read_conf ETH_ONLY

    if [[ "$CONF_VAL" != "yes" ]] || ! eth_capable ; then
        # The option was turned off: put Wi-Fi back and stop. wd.sh starts
        # the monitor again if the option is turned back on.
        if [ -f "$WIFI_OFF_FLAG" ] || [ -f "$WIFI_FALLBACK_FLAG" ] ; then
            wifi_restore
        fi
        exit 0
    fi

    { read -r CARRIER < /sys/class/net/eth0/carrier; } 2>/dev/null

    if [ "$CARRIER" == "1" ] && eth0_has_ip ; then
        ETH_DOWN_COUNT=0
        ETH_NOIP_COUNT=0
        if [ $FALLBACK -eq 1 ] ; then
            ETH_UP_COUNT=$((ETH_UP_COUNT + 1))
            if [ $ETH_UP_COUNT -ge $ETH_UP_LIMIT ] ; then
                wifi_off
            fi
        else
            ETH_UP_COUNT=0
            # Idempotent: also keeps wlan0 down if the stock Wi-Fi manager
            # brings it back up.
            wifi_off
        fi
    else
        ETH_UP_COUNT=0

        # Carrier up but no address: a flap can drop the lease and its DHCP
        # client, and the stock firmware does not restart it. Retry at once,
        # then once a minute, instead of settling on the Wi-Fi fallback.
        if [ "$CARRIER" == "1" ] && ! eth0_has_ip ; then
            ETH_NOIP_COUNT=$((ETH_NOIP_COUNT + 1))
            if [ $ETH_NOIP_COUNT -eq 1 ] || [ $((ETH_NOIP_COUNT % 6)) -eq 0 ] ; then
                log "Ethernet link up without a lease: restarting eth0 DHCP"
                eth0_dhcp_start
            fi
        else
            ETH_NOIP_COUNT=0
        fi

        if [ $FALLBACK -eq 0 ] ; then
            ETH_DOWN_COUNT=$((ETH_DOWN_COUNT + 1))
            # Keep wlan0 down during the 5-minute grace period.
            wifi_off
            if [ $ETH_DOWN_COUNT -ge $ETH_DOWN_LIMIT ] ; then
                log "Ethernet unavailable for 5 minutes: enabling Wi-Fi fallback"
                wifi_on
            fi
        else
            ETH_DOWN_COUNT=$((ETH_DOWN_COUNT + 1))
            # Retry the fallback DHCP once a minute if Wi-Fi has no address.
            if [ $((ETH_DOWN_COUNT % 6)) -eq 0 ] && ! wlan0_has_ip ; then
                ifconfig wlan0 up 2>/dev/null
                if [ -x /home/base/tools/wpa_cli ] ; then
                    /home/base/tools/wpa_cli -i wlan0 reconfigure >/dev/null 2>&1
                fi
                $YI_HACK_PREFIX/script/wifidhcp.sh >/dev/null 2>&1
            fi
        fi
    fi

    sleep $INTERVAL
done
