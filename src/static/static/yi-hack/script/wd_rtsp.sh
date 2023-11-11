#!/bin/sh

CONF_FILE="etc/system.conf"
CAMERA_CONF_FILE="etc/camera.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"
MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

#LOG_FILE="/tmp/sd/wd_rtsp.log"
LOG_FILE="/dev/null"

COUNTER=0
COUNTER_LIMIT=10
INTERVAL=10

get_camera_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CAMERA_CONF_FILE | cut -d "=" -f2
}

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

restart_rtsp()
{
    if [[ $(get_config RTSP) == "yes" ]] ; then
        if [[ "$RTSP_STREAM" == "low" ]]; then
            if [ "$RTSP_ALT" == "yes" ]; then
                h264grabber -m $MODEL_SUFFIX -r low -f &
                sleep 1
            fi
            $RTSP_DAEMON -m $MODEL_SUFFIX -r low $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD &
        fi
        if [[ "$RTSP_STREAM" == "high" ]]; then
            if [ "$RTSP_ALT" == "yes" ]; then
                h264grabber -m $MODEL_SUFFIX -r high -f &
                sleep 1
            fi
            $RTSP_DAEMON -m $MODEL_SUFFIX -r high $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD &
        fi
        if [[ "$RTSP_STREAM" == "both" ]]; then
            if [ "$RTSP_ALT" == "yes" ]; then
                h264grabber -m $MODEL_SUFFIX -r both -f &
                sleep 1
            fi
            $RTSP_DAEMON -m $MODEL_SUFFIX -r both $RTSP_AUDIO_COMPRESSION $RTSP_PORT $RTSP_USER $RTSP_PASSWORD &
        fi
    fi
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
        echo "Camera is swtich off no rtsp restart needed" >> $LOG_FILE
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
        echo "Camera is swtich off, rtsp restart not needed" >> $LOG_FILE
    fi
}

check_rmm()
{
#  echo "$(date +'%Y-%m-%d %H:%M:%S') - Checking rmm process..." >> $LOG_FILE
    PS=`ps ww | grep rmm | grep -v grep | grep -c ^`

    if [ $PS -eq 0 ]; then
        echo "check_rmm failed, reboot!" >> $LOG_FILE
        reboot
    fi
}

if [[ $(get_config RTSP) == "no" ]] ; then
    exit
fi

if [[ "$(get_config USERNAME)" != "" ]] ; then
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac

RTSP_DAEMON="rRTSPServer"
RTSP_AUDIO_COMPRESSION=$(get_config RTSP_AUDIO)

if [[ $(get_config RTSP_ALT) == "yes" ]] ; then
    RTSP_DAEMON="rtsp_server_yi"
fi

if [[ "$RTSP_AUDIO_COMPRESSION" == "none" ]] ; then
    RTSP_AUDIO_COMPRESSION="no"
fi
if [ ! -z $RTSP_AUDIO_COMPRESSION ]; then
    RTSP_AUDIO_COMPRESSION="-a "$RTSP_AUDIO_COMPRESSION
fi
if [ ! -z $RTSP_PORT ]; then
    RTSP_PORT_NUMBER=$RTSP_PORT
    RTSP_PORT="-p "$RTSP_PORT
fi
if [ ! -z $USERNAME ]; then
    RTSP_USER="-u "$USERNAME
fi
if [ ! -z $PASSWORD ]; then
    RTSP_PASSWORD="-w "$PASSWORD
fi

RTSP_STREAM=$(get_config RTSP_STREAM)
RTSP_ALT=$(get_config RTSP_ALT)

echo "$(date +'%Y-%m-%d %H:%M:%S') - Starting RTSP watchdog..." >> $LOG_FILE

while true
do
    if [[ "$RTSP_ALT" == "no" ]] ; then
        check_rtsp
    else
        check_rtsp_alt
    fi
    check_rmm

    echo 1500 > /sys/class/net/eth0/mtu
    echo 1500 > /sys/class/net/wlan0/mtu

    if [ $COUNTER -eq 0 ]; then
        sleep $INTERVAL
    fi
done
