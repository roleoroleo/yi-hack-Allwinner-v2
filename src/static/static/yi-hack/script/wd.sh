#!/bin/sh

CONF_FILE="etc/system.conf"
CAMERA_CONF_FILE="etc/camera.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"
MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

START_STOP_SCRIPT=$YI_HACK_PREFIX/script/service.sh

#LOG_FILE="/tmp/sd/wd.log"
LOG_FILE="/dev/null"
LOGWIFI_FILE="/tmp/sd/hack_wififailsafe.log"

COUNTER=0
COUNTER_LIMIT=10
INTERVAL=10
WIFI_FAILSAFE_COUNTER=0

# WiFi recovery ladder thresholds (each loop is ~INTERVAL seconds). Dropping the
# interface and rebooting are deliberately far out - most "losses" are a single
# missed sample and clear on their own within a cycle or two.
WIFI_SOFT=3      # ~30s: ask wpa_supplicant to re-associate
WIFI_MED=6       # ~60s: reload supplicant config
WIFI_HARD=12     # ~2min: bounce the interface (last resort before reboot)
WIFI_REBOOT=30   # ~5min: reboot as the absolute last resort

# Persistent, low-volume event log (kept 7 days, pruned by system.sh/cron).
LOG_DIR="/tmp/sd/log"
plog()
{
    mkdir -p "$LOG_DIR"
    echo "$(date '+%Y-%m-%d %H:%M:%S') wd: $1" >> "$LOG_DIR/hack-$(date +%Y%m%d).log"
}

get_camera_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CAMERA_CONF_FILE | cut -d "=" -f2-
}

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2-
}

restart_rtsp()
{
    $START_STOP_SCRIPT rtsp start
}

check_rtsp()
{
    if [[ $(get_camera_config SWITCH_ON) == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        LISTEN=`$YI_HACK_PREFIX/bin/netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep LISTEN | grep -c ^`
        SOCKET=`$YI_HACK_PREFIX/bin/netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep ESTABLISHED | grep -c ^`
        CPU=`top -b -n 2 -d 1 | grep rRTSPServer | grep -v grep | tail -n 1 | awk '{print $8}'`

        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q rRTSPServer
            sleep 1
            restart_rtsp
        fi
        if [ "$CPU" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes, restarting..." >> $LOG_FILE
            killall -q rRTSPServer
            sleep 1
            restart_rtsp
            COUNTER=0
        fi
        if [ $SOCKET -gt 0 ]; then
            if [ "$CPU" == "0.0" ]; then
                COUNTER=$((COUNTER+1))
                echo "$(date +'%Y-%m-%d %H:%M:%S') - Detected possible locked process ($COUNTER)" >> $LOG_FILE
                if [ $COUNTER -ge $COUNTER_LIMIT ]; then
                    echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
                    killall -q rRTSPServer
                    sleep 1
                    restart_rtsp
                    COUNTER=0
                fi
            else
                COUNTER=0
            fi
        fi
    else
        echo "Camera is swiched off no rtsp restart needed" >> $LOG_FILE
    fi
}

check_rtsp_alt()
{
    if [[ $(get_camera_config SWITCH_ON) == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        LISTEN=`$YI_HACK_PREFIX/bin/netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep LISTEN | grep -c ^`
        CPU1=`top -b -n 2 -d 1 | grep h264grabber | grep -v grep | tail -n 1 | awk '{print $8}'`
        CPU2=`top -b -n 2 -d 1 | grep rtsp_server_yi | grep -v grep | tail -n 1 | awk '{print $8}'`

        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q rtsp_server_yi
            killall -q h264grabber
            sleep 1
            restart_rtsp
        fi
        if [ "$CPU1" == "" ] || [ "$CPU2" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes, restarting..." >> $LOG_FILE
            killall -q rtsp_server_yi
            killall -q h264grabber
            sleep 1
            restart_rtsp
            COUNTER=0
        fi
    else
        echo "Camera is switched off, rtsp restart not needed" >> $LOG_FILE
    fi
}

check_rtsp_go2rtc()
{
    if [[ $(get_camera_config SWITCH_ON) == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        LISTEN=`$YI_HACK_PREFIX/bin/netstat -an 2>&1 | grep ":$RTSP_PORT_NUMBER " | grep LISTEN | grep -c ^`
        CPU1=`top -b -n 2 -d 1 | grep h264grabber | grep -v grep | tail -n 1 | awk '{print $8}'`
        CPU2=`top -b -n 2 -d 1 | grep go2rtc | grep -v grep | tail -n 1 | awk '{print $8}'`

        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q go2rtc
            killall -q h264grabber
            sleep 1
            restart_rtsp
        fi
        if [ "$CPU1" == "" ] || [ "$CPU2" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes, restarting..." >> $LOG_FILE
            killall -q go2rtc
            killall -q h264grabber
            sleep 1
            restart_rtsp
            COUNTER=0
        fi
    else
        echo "Camera is switched off, rtsp restart not needed" >> $LOG_FILE
    fi
}

check_rmm()
{
    #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking rmm process..." >> $LOG_FILE

    # Method 1: Basic ps check (most reliable, avoids ps ww parsing issues)
    PS_BASIC=`ps | grep -v grep | grep "./rmm" | grep -c ^`
    if [ $PS_BASIC -gt 0 ]; then
        # Reset failure counter on successful detection
        rm -f /tmp/rmm_fail_count 2>/dev/null
        return 0
    fi

    # Method 2: Extended ps as fallback (original method)
    PS_WW=`ps ww | grep rmm | grep -v grep | grep -c ^`
    if [ $PS_WW -gt 0 ]; then
        # Reset failure counter on successful detection  
        rm -f /tmp/rmm_fail_count 2>/dev/null
        return 0
    fi

    # Failure handling with counter to prevent immediate reboots
    echo "$(date +'%Y-%m-%d %H:%M:%S') - rmm detection failed" >> $LOG_FILE

    # Read current failure count
    if [ -f /tmp/rmm_fail_count ]; then
        FAIL_COUNT=$(cat /tmp/rmm_fail_count)
    else
        FAIL_COUNT=0
    fi

    # Increment failure count
    FAIL_COUNT=$((FAIL_COUNT + 1))
    echo $FAIL_COUNT > /tmp/rmm_fail_count

    echo "$(date +'%Y-%m-%d %H:%M:%S') - rmm failure count: $FAIL_COUNT/5" >> $LOG_FILE

    # Only reboot after 5 consecutive failures (~50 seconds with 10s interval)
    if [ $FAIL_COUNT -ge 5 ]; then
        echo "$(date +'%Y-%m-%d %H:%M:%S') - rmm failed 5 times consecutively, rebooting..." >> $LOG_FILE
        plog "rmm failed 5x consecutively, rebooting"
        reboot
    fi
}

check_mqtt()
{
    #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking mqttv4 process..." >> $LOG_FILE

    PS=`ps ww | grep mqttv4 | grep -v grep | grep -c ^`

    if [ $PS -eq 0 ]; then
        echo "check_mqtt failed, restart it!" >> $LOG_FILE
        plog "mqtt not running, restarting"
        $START_STOP_SCRIPT mqtt start
    fi
}

check_httpd()
{
    # httpd is started once at boot and was never supervised: RTSP could be
    # perfectly healthy while the web UI was dead. Restart it if it is neither
    # listening nor running.
    if [[ $(get_config HTTPD) != "yes" ]] ; then
        return
    fi
    case $(get_config HTTPD_PORT) in
        ''|*[!0-9]*) HTTPD_PORT_NUMBER=80 ;;
        *) HTTPD_PORT_NUMBER=$(get_config HTTPD_PORT) ;;
    esac
    LISTEN=`$YI_HACK_PREFIX/bin/netstat -an 2>&1 | grep ":$HTTPD_PORT_NUMBER " | grep LISTEN | grep -c ^`
    RUNNING=`ps | grep -v grep | grep httpd | grep -c ^`
    if [ $LISTEN -eq 0 ] || [ $RUNNING -eq 0 ]; then
        plog "httpd down (listen=$LISTEN running=$RUNNING), restarting"
        killall -q httpd
        sleep 1
        httpd -p $HTTPD_PORT_NUMBER -h $YI_HACK_PREFIX/www/ -c /tmp/httpd.conf
    fi
}

wpa_cli_run()
{
    # Run wpa_cli with a hard 2s kill guard - it can hang or segfault on some
    # models (e.g. r37gb). $1 is the sub-command (status/reconnect/reconfigure).
    [ -x /home/base/tools/wpa_cli ] || return 1
    (sleep 2 && killall -9 wpa_cli 2>/dev/null) &
    _k=$!
    _out=$(/home/base/tools/wpa_cli -i wlan0 "$1" 2>/dev/null)
    _rc=$?
    kill $_k 2>/dev/null
    wait $_k 2>/dev/null
    echo "$_out"
    return $_rc
}

wifi_has_ip()
{
    # A usable connection means a v4 address is assigned - that is what the web
    # UI and RTSP actually need. Association without an IP is treated as down so
    # the ladder below can renew DHCP.
    ifconfig wlan0 2>/dev/null | grep -q "inet addr:"
}

check_wifi()
{
    if wifi_has_ip; then
        if [ $WIFI_FAILSAFE_COUNTER -gt 0 ]; then
            plog "WiFi recovered after $WIFI_FAILSAFE_COUNTER failed check(s)"
            echo -e "$(date): WiFi connection restored" >> "$LOGWIFI_FILE"
            WIFI_FAILSAFE_COUNTER=0
        fi
        return
    fi

    # No usable IP this cycle. Count it, but act gently and escalate slowly.
    WIFI_FAILSAFE_COUNTER=$((WIFI_FAILSAFE_COUNTER + 1))

    # Keep the wifi log from growing without bound.
    if [ -e "$LOGWIFI_FILE" ]; then
        $YI_HACK_PREFIX/usr/bin/tail -n 145 "$LOGWIFI_FILE" > "$LOGWIFI_FILE.tmp" && mv "$LOGWIFI_FILE.tmp" "$LOGWIFI_FILE"
    fi
    echo -e "$(date): WiFi check failed ($WIFI_FAILSAFE_COUNTER)" >> "$LOGWIFI_FILE"

    if [ $WIFI_FAILSAFE_COUNTER -lt $WIFI_SOFT ]; then
        # Transient - do nothing disruptive, just re-check next cycle.
        return
    elif [ $WIFI_FAILSAFE_COUNTER -eq $WIFI_SOFT ]; then
        plog "WiFi down ${WIFI_FAILSAFE_COUNTER}x: asking supplicant to reconnect"
        echo -e "$(date): soft recovery - wpa_cli reconnect" >> "$LOGWIFI_FILE"
        wpa_cli_run reconnect >/dev/null
    elif [ $WIFI_FAILSAFE_COUNTER -eq $WIFI_MED ]; then
        plog "WiFi still down ${WIFI_FAILSAFE_COUNTER}x: reloading supplicant config"
        echo -e "$(date): medium recovery - wpa_cli reconfigure" >> "$LOGWIFI_FILE"
        wpa_cli_run reconfigure >/dev/null
    elif [ $WIFI_FAILSAFE_COUNTER -eq $WIFI_HARD ]; then
        # Last resort before reboot: bounce the interface and restart DHCP.
        plog "WiFi still down ${WIFI_FAILSAFE_COUNTER}x: bouncing wlan0 (last resort)"
        echo -e "$(date): last resort - bouncing wlan0" >> "$LOGWIFI_FILE"
        ifconfig wlan0 down
        sleep 1
        ifconfig wlan0 up
        sleep 1
        wpa_cli_run reconfigure >/dev/null
        $YI_HACK_PREFIX/script/wifidhcp.sh >/dev/null 2>&1 &
    elif [ $WIFI_FAILSAFE_COUNTER -ge $WIFI_REBOOT ]; then
        plog "WiFi unrecoverable after ${WIFI_FAILSAFE_COUNTER} checks: rebooting"
        echo -e "$(date): WiFi could not be restored. Rebooting..." >> "$LOGWIFI_FILE"
        reboot
    fi
}

if [[ $(get_config RTSP) == "no" ]] ; then
    exit
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac

if [ ! -z $RTSP_PORT ]; then
    RTSP_PORT_NUMBER=$RTSP_PORT
fi

RTSP_ALT=$(get_config RTSP_ALT)

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    if [[ "$RTSP_ALT" == "standard" ]] ; then
        check_rtsp
    elif [[ "$RTSP_ALT" == "alternative" ]] ; then
        check_rtsp_alt
    else
        check_rtsp_go2rtc
    fi
    check_rmm
    check_mqtt
    check_httpd
    check_wifi

    echo 1500 > /sys/class/net/eth0/mtu
    echo 1500 > /sys/class/net/wlan0/mtu

    if [ $COUNTER -eq 0 ]; then
        sleep $INTERVAL
    fi
done
