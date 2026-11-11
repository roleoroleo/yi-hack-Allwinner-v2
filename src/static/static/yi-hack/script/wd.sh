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
CPU_SAMPLED=no

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

# The checks below run every 10 s, so they only use shell builtins and read
# /proc and /sys: a healthy loop starts no new process apart from sleep and
# the nc of the RTSP probe.
# Command substitution forks in ash even for functions, so results are
# returned in variables.

# read_conf FILE KEY -> CONF_VAL
read_conf()
{
    CONF_VAL=""
    while IFS='=' read -r K V || [ -n "$K" ]; do
        if [ "$K" == "$2" ]; then
            CONF_VAL=$V
            return 0
        fi
    done < $1
    return 1
}

# PID_<NAME> for the processes the watchdog looks after ("" = not running)
scan_processes()
{
    PID_RRTSPSERVER=""
    PID_H264GRABBER=""
    PID_RTSP_SERVER_YI=""
    PID_GO2RTC=""
    PID_RMM=""
    PID_MQTTV4=""
    for P in /proc/[0-9]*; do
        { read -r C < $P/comm; } 2>/dev/null || continue
        case "$C" in
            rRTSPServer) PID_RRTSPSERVER=${P#/proc/} ;;
            h264grabber) PID_H264GRABBER=${P#/proc/} ;;
            rtsp_server_yi) PID_RTSP_SERVER_YI=${P#/proc/} ;;
            go2rtc) PID_GO2RTC=${P#/proc/} ;;
            rmm) PID_RMM=${P#/proc/} ;;
            mqttv4) PID_MQTTV4=${P#/proc/} ;;
        esac
    done
}

# LISTEN and SOCKET: listening and established TCP sockets on the RTSP port
count_rtsp_sockets()
{
    LISTEN=0
    SOCKET=0
    for F in /proc/net/tcp /proc/net/tcp6; do
        [ -r $F ] || continue
        while read -r SL LOCAL REMOTE STATE REST; do
            case "$LOCAL" in
                *:$RTSP_PORT_HEX)
                    case "$STATE" in
                        0A) LISTEN=$((LISTEN+1)) ;;
                        01) SOCKET=$((SOCKET+1)) ;;
                    esac
                    ;;
            esac
        done < $F
    done
}

# TICKS: CPU time (utime + stime) used so far by process $1, "" if it is gone
proc_ticks()
{
    TICKS=""
    { read -r X X X X X X X X X X X X X U S X < /proc/$1/stat; } 2>/dev/null && TICKS=$((U + S))
}

check_rtsp()
{
    read_conf $YI_HACK_PREFIX/$CAMERA_CONF_FILE SWITCH_ON
    if [[ "$CONF_VAL" == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        count_rtsp_sockets
        # CPU: "" if rRTSPServer is not running, otherwise the CPU ticks it
        # used in 1 s (measured only when a client is connected, as before
        # with top -d 1)
        CPU=""
        if [ -n "$PID_RRTSPSERVER" ]; then
            CPU="-"
            if [ $SOCKET -gt 0 ]; then
                proc_ticks $PID_RRTSPSERVER
                T1=$TICKS
                sleep 1
                CPU_SAMPLED=yes
                proc_ticks $PID_RRTSPSERVER
                if [ -n "$T1" ] && [ -n "$TICKS" ]; then
                    CPU=$((TICKS - T1))
                else
                    CPU=""
                fi
            fi
        fi

        # One restart at most per loop: the first failing check restarts the server
        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q rRTSPServer
            sleep 1
            restart_rtsp
            COUNTER=0
        elif [ "$CPU" == "" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - No running processes, restarting..." >> $LOG_FILE
            killall -q rRTSPServer
            sleep 1
            restart_rtsp
            COUNTER=0
        elif [ -z "$(printf "OPTIONS rtsp://127.0.0.1:$RTSP_PORT_NUMBER/ RTSP/1.0\r\nCSeq: 1\r\n\r\n" | $YI_HACK_PREFIX/usr/bin/nc -w 5 127.0.0.1 $RTSP_PORT_NUMBER 2>/dev/null)" ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - RTSP not answering, restarting..." >> $LOG_FILE
            killall -q rRTSPServer
            sleep 1
            restart_rtsp
            COUNTER=0
        elif [ $SOCKET -gt 0 ]; then
            if [ "$CPU" == "0" ]; then
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
    read_conf $YI_HACK_PREFIX/$CAMERA_CONF_FILE SWITCH_ON
    if [[ "$CONF_VAL" == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        count_rtsp_sockets
        CPU1=$PID_H264GRABBER
        CPU2=$PID_RTSP_SERVER_YI

        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q rtsp_server_yi
            killall -q h264grabber
            sleep 1
            restart_rtsp
        elif [ "$CPU1" == "" ] || [ "$CPU2" == "" ]; then
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
    read_conf $YI_HACK_PREFIX/$CAMERA_CONF_FILE SWITCH_ON
    if [[ "$CONF_VAL" == "yes" ]] ; then
        #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking RTSP process..." >> $LOG_FILE
        count_rtsp_sockets
        CPU1=$PID_H264GRABBER
        CPU2=$PID_GO2RTC

        if [ $LISTEN -eq 0 ]; then
            echo "$(date +'%Y-%m-%d %H:%M:%S') - Restarting rtsp process" >> $LOG_FILE
            killall -q go2rtc
            killall -q h264grabber
            sleep 1
            restart_rtsp
        elif [ "$CPU1" == "" ] || [ "$CPU2" == "" ]; then
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

    # Method 1: process name from /proc (see scan_processes)
    if [ -n "$PID_RMM" ]; then
        # Reset failure counter on successful detection
        if [ -f /tmp/rmm_fail_count ]; then
            rm -f /tmp/rmm_fail_count 2>/dev/null
        fi
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
        reboot
    fi
}

check_mqtt()
{
    #  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking mqttv4 process..." >> $LOG_FILE

    if [ -z "$PID_MQTTV4" ]; then
        echo "check_mqtt failed, restart it!" >> $LOG_FILE
        $START_STOP_SCRIPT mqtt start
    fi
}

check_wifi()
{
    # Check WiFi connection using multiple methods for compatibility
    # Some camera models (e.g., r37gb) have broken wpa_cli that causes segfaults

    WIFI_CONNECTED=0

    # Method 0: carrier up and a route on wlan0 (an address is configured)
    # means connected for methods 1-3 too; read without starting processes
    CARRIER=""
    { read -r CARRIER < /sys/class/net/wlan0/carrier; } 2>/dev/null
    if [ "$CARRIER" == "1" ]; then
        while read -r IFACE REST; do
            if [ "$IFACE" == "wlan0" ]; then
                WIFI_CONNECTED=1
                break
            fi
        done < /proc/net/route
    fi

    # Method 1: Try wpa_cli if available and working (preferred method)
    # Wrap in timeout to avoid hanging - wpa_cli may block on some models
    if [ $WIFI_CONNECTED -eq 0 ] && [ -x /home/base/tools/wpa_cli ]; then
        # Run wpa_cli with auto-kill after 2 seconds if it hangs
        (sleep 2 && killall -9 wpa_cli 2>/dev/null) &
        KILLER_PID=$!

        WPA_OUTPUT=$(/home/base/tools/wpa_cli -i wlan0 status 2>/dev/null)
        WPA_EXIT=$?

        # Kill the timeout killer if wpa_cli finished
        kill $KILLER_PID 2>/dev/null
        wait $KILLER_PID 2>/dev/null

        # Check if wpa_cli succeeded and returned valid output
        if [ $WPA_EXIT -eq 0 ] && echo "$WPA_OUTPUT" | grep -q "wpa_state=COMPLETED"; then
            WIFI_CONNECTED=1
        fi
    fi

    # Method 2: Fallback - check if interface has IP address (reliable on all models)
    if [ $WIFI_CONNECTED -eq 0 ]; then
        if ifconfig wlan0 2>/dev/null | grep -q "inet addr:"; then
            WIFI_CONNECTED=1
            # Method 3: the address stays after the link drops, so also require carrier
            if [ "$(cat /sys/class/net/wlan0/carrier 2>/dev/null)" == "0" ]; then
                WIFI_CONNECTED=0
            fi
        fi
    fi

    # Handle WiFi disconnection
    if [ $WIFI_CONNECTED -eq 0 ]; then
        # Rotate log file to prevent it from growing too large
        if [ -e "$LOGWIFI_FILE" ]; then
            $YI_HACK_PREFIX/usr/bin/tail -n 145 "$LOGWIFI_FILE" > "$LOGWIFI_FILE.tmp" && mv "$LOGWIFI_FILE.tmp" "$LOGWIFI_FILE"
        fi

        echo -e "$(date): WiFi connection lost (failsafe attempt $((WIFI_FAILSAFE_COUNTER + 1))/6)" >> "$LOGWIFI_FILE"

        WIFI_FAILSAFE_COUNTER=$((WIFI_FAILSAFE_COUNTER + 1))

        if [ "$WIFI_FAILSAFE_COUNTER" -ge 6 ]; then
            echo -e "$(date): WiFi connection could not be restored after 6 attempts. Rebooting..." >> "$LOGWIFI_FILE"
            reboot
        else
            echo -e "$(date): Attempting WiFi reconnect..." >> "$LOGWIFI_FILE"

            # Try to reconnect
            sleep 2
            ifconfig wlan0 down
            sleep 1
            ifconfig wlan0 up
            sleep 1

            # Try wpa_cli reconfigure if available and working
            if [ -x /home/base/tools/wpa_cli ]; then
                (sleep 2 && killall -9 wpa_cli 2>/dev/null) &
                KILLER_PID=$!
                /home/base/tools/wpa_cli -i wlan0 reconfigure 2>/dev/null
                kill $KILLER_PID 2>/dev/null
                wait $KILLER_PID 2>/dev/null
            fi

            # Run wifidhcp.sh
            $YI_HACK_PREFIX/script/wifidhcp.sh
        fi
    else
        # WiFi is connected - reset failure counter
        if [ $WIFI_FAILSAFE_COUNTER -gt 0 ]; then
            echo -e "$(date): WiFi connection restored" >> "$LOGWIFI_FILE"
            WIFI_FAILSAFE_COUNTER=0
        fi
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
# Local port as in /proc/net/tcp (4 hex digits, upper case)
RTSP_PORT_HEX=$(printf "%04X" $RTSP_PORT_NUMBER)

RTSP_ALT=$(get_config RTSP_ALT)

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    scan_processes
    CPU_SAMPLED=no
    if [[ "$RTSP_ALT" == "standard" ]] ; then
        check_rtsp
    elif [[ "$RTSP_ALT" == "alternative" ]] ; then
        check_rtsp_alt
    else
        check_rtsp_go2rtc
    fi
    check_rmm
    check_mqtt
    check_wifi

    echo 1500 > /sys/class/net/eth0/mtu
    echo 1500 > /sys/class/net/wlan0/mtu

    if [ $COUNTER -eq 0 ]; then
        sleep $INTERVAL
    elif [ "$CPU_SAMPLED" != "yes" ]; then
        # Checking a possibly locked rRTSPServer: loop again soon, but not
        # without a pause. top used to take this time; the CPU sample takes
        # it while a client is connected, this covers the loops without one.
        sleep 1
    fi
done
