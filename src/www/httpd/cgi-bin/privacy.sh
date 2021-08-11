#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/home/yi-hack"

YI_HACK_VER=$(cat /home/yi-hack/version)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

init_config()
{
    if [[ x$(get_config USERNAME) != "x" ]] ; then
        USERNAME=$(get_config USERNAME)
        PASSWORD=$(get_config PASSWORD)
    fi

    case $(get_config RTSP_PORT) in
        ''|*[!0-9]*) RTSP_PORT=554 ;;
        *) RTSP_PORT=$(get_config RTSP_PORT) ;;
    esac

    if [[ $RTSP_PORT != "554" ]] ; then
        D_RTSP_PORT=:$RTSP_PORT
    fi

    if [[ $(get_config RTSP) == "yes" ]] ; then
        RTSP_AUDIO_COMPRESSION=$(get_config RTSP_AUDIO)
        if [[ "$RTSP_AUDIO_COMPRESSION" == "none" ]]; then
            RTSP_AUDIO_COMPRESSION="no"
        fi
    fi
}

start_rtsp()
{
    if [ "$1" != "none" ]; then
        RTSP_RES=$1
    fi
    if [ "$2" != "none" ]; then
        RTSP_AUDIO_COMPRESSION=$2
    fi

    RRTSP_MODEL=$MODEL_SUFFIX RRTSP_RES=$RTSP_RES RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD RRTSP_AUDIO=$RTSP_AUDIO_COMPRESSION rRTSPServer >/dev/null &
    $YI_HACK_PREFIX/script/wd_rtsp.sh >/dev/null &
}

stop_rtsp()
{
    killall wd_rtsp.sh
    killall rRTSPServer
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

VALUE="none"
PARAM1="none"
PARAM2="none"
RES="none"

CONF="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f2)"

if [ "$CONF" == "value" ] ; then
    VALUE="$VAL"
fi

init_config

if [ "$VALUE" == "on" ] ; then
    touch /tmp/privacy
    touch /tmp/snapshot.disabled
    stop_rtsp
    killall mp4record
    RES="on"
elif [ "$VALUE" == "off" ] ; then
    rm -f /tmp/snapshot.disabled
    start_rtsp $PARAM1 $PARAM2
    cd /home/app
    ./mp4record >/dev/null &
    rm -f /tmp/privacy
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
    printf "\"status\": \"$RES\"\n"
fi
printf "}"
