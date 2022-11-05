#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CONF_FILE="$YI_HACK_PREFIX/etc/camera.conf"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

CONF_LAST="CONF_LAST"

for I in 1 2 3 4 5 6 7 8 9 10 11 12
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if ! $(validateString $CONF); then
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}"
        exit
    fi
    if ! $(validateString $VAL); then
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}"
        exit
    fi

    if [ $CONF == $CONF_LAST ]; then
        continue
    fi
    CONF_LAST=$CONF
    CONF_UPPER="$(echo $CONF | tr '[a-z]' '[A-Z]')"

    sed -i "s/^\(${CONF_UPPER}\s*=\s*\).*$/\1${VAL}/" $CONF_FILE

    if [ "$CONF" == "switch_on" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -t off
        else
            ipc_cmd -t on
        fi
    elif [ "$CONF" == "save_video_on_motion" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -v always
        else
            ipc_cmd -v detect
        fi
    elif [ "$CONF" == "sensitivity" ] ; then
        if [ "$VAL" == "low" ] || [ "$VAL" == "medium" ] || [ "$VAL" == "high" ]; then
            ipc_cmd -s $VAL
        fi
    elif [ "$CONF" == "ai_human_detection" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -a off
        else
            ipc_cmd -a on
        fi
    elif [ "$CONF" == "face_detection" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -c off
        else
            ipc_cmd -c on
        fi
    elif [ "$CONF" == "motion_tracking" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -o off
        else
            ipc_cmd -o on
        fi
    elif [ "$CONF" == "sound_detection" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -b off
        else
            ipc_cmd -b on
        fi
    elif [ "$CONF" == "sound_sensitivity" ] ; then
        if [ "$VAL" == "30" ] || [ "$VAL" == "35" ] || [ "$VAL" == "40" ] || [ "$VAL" == "45" ] || [ "$VAL" == "50" ] || [ "$VAL" == "60" ] || [ "$VAL" == "70" ] || [ "$VAL" == "80" ] || [ "$VAL" == "90" ] ; then
            ipc_cmd -n $VAL
        fi
    elif [ "$CONF" == "led" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -l off
        else
            ipc_cmd -l on
        fi
    elif [ "$CONF" == "ir" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -i off
        else
            ipc_cmd -i on
        fi
    elif [ "$CONF" == "rotate" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -r off
        else
            ipc_cmd -r on
        fi
    elif [ "$CONF" == "cruise" ] ; then
        if [ "$VAL" == "off" ]; then
            ipc_cmd -C off
        elif [ "$VAL" == "presets" ]; then
            ipc_cmd -C on
            sleep 0.5
            ipc_cmd -C presets
        elif [ "$VAL" == "360" ]; then
            ipc_cmd -C on
            sleep 0.5
            ipc_cmd -C 360
        fi
    fi
    sleep 1
done

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
