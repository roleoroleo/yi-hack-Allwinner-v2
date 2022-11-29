#!/bin/sh

CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/tmp/sd/yi-hack"
YI_HACK_VER=$(cat /tmp/sd/yi-hack/version)
MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

start_rtsp()
{
    $YI_HACK_PREFIX/script/wd_rtsp.sh >/dev/null &
}

stop_rtsp()
{
    killall wd_rtsp.sh
    killall $RTSP_DAEMON
    killall h264grabber
}

ps_program()
{
    PS_PROGRAM=$(ps | grep $1 | grep -v grep | grep -c ^)
    if [ $PS_PROGRAM -gt 0 ]; then
        echo "started"
    else
        echo "stopped"
    fi
}

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

VALUE="none"
RES="none"

CONF="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f2)"

if [ "$CONF" == "value" ] ; then
    VALUE="$VAL"
fi

if [ "$VALUE" == "on" ] ; then
    if [ ! -f /tmp/privacy ]; then
        touch /tmp/privacy
        touch /tmp/snapshot.disabled
        stop_rtsp
        killall mp4record
    fi
    RES="on"
elif [ "$VALUE" == "off" ] ; then
    if [ -f /tmp/privacy ]; then
        rm -f /tmp/snapshot.disabled
        start_rtsp $PARAM1 $PARAM2
        if [[ $(get_config DISABLE_CLOUD) == "no" ]] || [[ $(get_config REC_WITHOUT_CLOUD) == "yes" ]] ; then
            cd /home/app
            ./mp4record >/dev/null &
        fi
        rm -f /tmp/privacy
    fi
    RES="off"
elif [ "$VALUE" == "status" ] ; then
    if [ -f /tmp/privacy ]; then
        RES="on"
    else
        RES="off"
    fi
fi

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
if [ ! -z "$RES" ]; then
    printf "\"status\": \"$RES\",\n"
fi
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
