#!/bin/sh

CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/tmp/sd/yi-hack"
START_STOP_SCRIPT=$YI_HACK_PREFIX/script/service.sh

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

NAME="none"
ACTION="none"
PARAM1="none"
PARAM2="none"
RES=""

for I in 1 2 3 4
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "name" ] ; then
        NAME="$VAL"
    elif [ "$CONF" == "action" ] ; then
        ACTION="$VAL"
    elif [ "$CONF" == "param1" ] ; then
        PARAM1="$VAL"
    elif [ "$CONF" == "param2" ] ; then
        PARAM2="$VAL"
    fi
done

if [ "$ACTION" == "start" ] ; then
    if [ "$NAME" == "rtsp" ]; then
        $START_STOP_SCRIPT rtsp start $PARAM1 $PARAM2
    elif [ "$NAME" == "onvif" ]; then
        $START_STOP_SCRIPT onvif start $PARAM1 $PARAM2
    elif [ "$NAME" == "wsdd" ]; then
        $START_STOP_SCRIPT wsdd start
    elif [ "$NAME" == "ftpd" ]; then
        $START_STOP_SCRIPT ftpd start $PARAM1
    elif [ "$NAME" == "mqtt" ]; then
        $START_STOP_SCRIPT mqtt start
    elif [ "$NAME" == "mqtt-config" ]; then
        $START_STOP_SCRIPT mqtt-config start
    elif [ "$NAME" == "mp4record" ]; then
        $START_STOP_SCRIPT mp4record start
    elif [ "$NAME" == "all" ]; then
        $START_STOP_SCRIPT all start
    fi
elif [ "$ACTION" == "stop" ] ; then
    if [ "$NAME" == "rtsp" ]; then
        $START_STOP_SCRIPT rtsp stop
    elif [ "$NAME" == "onvif" ]; then
        $START_STOP_SCRIPT onvif stop
    elif [ "$NAME" == "wsdd" ]; then
        $START_STOP_SCRIPT wsdd stop
    elif [ "$NAME" == "ftpd" ]; then
        $START_STOP_SCRIPT ftpd stop
    elif [ "$NAME" == "mqtt-config" ]; then
        $START_STOP_SCRIPT mqtt-config stop
    elif [ "$NAME" == "mqtt" ]; then
        $START_STOP_SCRIPT mqtt stop
    elif [ "$NAME" == "mp4record" ]; then
        $START_STOP_SCRIPT mp4record stop
    elif [ "$NAME" == "all" ]; then
        $START_STOP_SCRIPT all stop
    fi
elif [ "$ACTION" == "status" ] ; then
    if [ "$NAME" == "rtsp" ]; then
        RES=$($START_STOP_SCRIPT rtsp status)
    elif [ "$NAME" == "onvif" ]; then
        RES=$($START_STOP_SCRIPT onvif status)
    elif [ "$NAME" == "wsdd" ]; then
        RES=$($START_STOP_SCRIPT wsdd status)
    elif [ "$NAME" == "ftpd" ]; then
        RES=$($START_STOP_SCRIPT ftpd status)
    elif [ "$NAME" == "mqtt" ]; then
        RES=$($START_STOP_SCRIPT mqtt status)
    elif [ "$NAME" == "mqtt-config" ]; then
        RES=$($START_STOP_SCRIPT mqtt-config status)
    elif [ "$NAME" == "mp4record" ]; then
        RES=$($START_STOP_SCRIPT mp4record status)
    fi
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
if [ ! -z "$RES" ]; then
    printf "\"status\": \"$RES\"\n"
fi
printf "}"
