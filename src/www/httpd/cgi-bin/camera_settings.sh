#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

HOMEVER=$(cat /home/homever)
HV=${HOMEVER:0:2}

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

source $YI_HACK_PREFIX/etc/camera.conf

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

CONF_LAST="CONF_LAST"

for I in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
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

    if [ "$CONF" == "switch_on" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -t off
            sleep 1
            ipc_cmd -T  # Stop current motion detection event
        else
            ipc_cmd -t on
        fi
    elif [ "$CONF" == "save_video_on_motion" ] ; then
        if [ "$VAL" == "no" ] ; then
            ipc_cmd -v always
        else
            ipc_cmd -v detect
        fi
    elif [ "$CONF" == "motion_detection" ] ; then
        if [ "$VAL" == "no" ] || [ "$VAL" == "yes" ] ; then
            MOTION_DETECTION=$VAL
        fi
    elif [ "$CONF" == "sensitivity" ] ; then
        if [ "$VAL" == "low" ] || [ "$VAL" == "medium" ] || [ "$VAL" == "high" ]; then
            ipc_cmd -s $VAL
        fi
    elif [ "$CONF" == "ai_human_detection" ] ; then
        if [ "$VAL" == "no" ] ; then
            AI_HUMAN_DETECTION="no"
        else
            AI_HUMAN_DETECTION="yes"
        fi
    elif [ "$CONF" == "ai_vehicle_detection" ] ; then
        if [ "$VAL" == "no" ] ; then
            AI_VEHICLE_DETECTION="no"
        else
            AI_VEHICLE_DETECTION="yes"
        fi
    elif [ "$CONF" == "ai_animal_detection" ] ; then
        if [ "$VAL" == "no" ] ; then
            AI_ANIMAL_DETECTION="no"
        else
            AI_ANIMAL_DETECTION="yes"
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
        if [ "$VAL" == "no" ]; then
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
    sleep 0.5
done

if [ "$HV" == "11" ] || [ "$HV" == "12" ]; then
    if [ "$MOTION_DETECTION" == "no" ]; then
        ipc_cmd -O off

        if [ "$AI_HUMAN_DETECTION" == "no" ]; then
            ipc_cmd -a off
        else
            ipc_cmd -a on
        fi
        if [ "$AI_VEHICLE_DETECTION" == "no" ]; then
            ipc_cmd -E off
        else
            ipc_cmd -E on
        fi
        if [ "$AI_ANIMAL_DETECTION" == "no" ]; then
            ipc_cmd -N off
        else
            ipc_cmd -N on
        fi
    else
        ipc_cmd -O on
    fi
else
    if [ "$AI_HUMAN_DETECTION" == "no" ]; then
        ipc_cmd -a off
    else
        ipc_cmd -a on
    fi
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
