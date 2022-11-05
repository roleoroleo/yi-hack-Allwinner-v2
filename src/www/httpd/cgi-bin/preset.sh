#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
PTZ_CONF_FILE=$YI_HACK_PREFIX/etc/ptz_presets.conf

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

add_del_preset() {
    FIRST_AVAIL=8
    PRESET_0=""
    PRESET_1=""
    PRESET_2=""
    PRESET_3=""
    PRESET_4=""
    PRESET_5=""
    PRESET_6=""
    PRESET_7=""

    IFS="="
    while read -r NUM NAME; do
        if [ -z $NUM ]; then
            continue;
        fi
        if [ "${NUM::1}" == "#" ]; then
            continue;
        fi
        if [ -z $NAME ] && [ $FIRST_AVAIL -gt $NUM ]; then
            FIRST_AVAIL=$NUM
        fi

        for I in 0 1 2 3 4 5 6 7; do
            if [ "$NUM" == "$I" ]; then
                eval PRESET_$I=$NAME
            fi
        done
    done < $PTZ_CONF_FILE

    if [ "$1" == "add" ]; then
        if [ $FIRST_AVAIL -le 7 ]; then
            eval PRESET_$FIRST_AVAIL=$2
        else
            return 8
        fi
    fi

    if [ "$1" == "del" ]; then
        for I in 0 1 2 3 4 5 6 7; do
            if [ "$2" == "$I" ] || [ "$2" == "all" ]; then
                eval PRESET_$I=""
            fi
        done
    fi

    if [ "$1" == "add" ] || [ "$1" == "del" ]; then
        echo 0=$PRESET_0 > $PTZ_CONF_FILE
        echo 1=$PRESET_1 >> $PTZ_CONF_FILE
        echo 2=$PRESET_2 >> $PTZ_CONF_FILE
        echo 3=$PRESET_3 >> $PTZ_CONF_FILE
        echo 4=$PRESET_4 >> $PTZ_CONF_FILE
        echo 5=$PRESET_5 >> $PTZ_CONF_FILE
        echo 6=$PRESET_6 >> $PTZ_CONF_FILE
        echo 7=$PRESET_7 >> $PTZ_CONF_FILE
    fi
}

return_error() {
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "message" "$@"
    printf "}"
}

if ! $(validateQueryString $QUERY_STRING); then
    return_error "Invalid query"
    exit
fi

ACTION="none"
NUM=-1
NAME=""

for I in 1 2 3
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "action" ] ; then
        ACTION="$VAL"
    elif [ "$CONF" == "num" ] ; then
        if $(validateNumber $VAL); then
            NUM="$VAL"
        else
            if [ "$VAL" == "all" ]; then
                NUM="$VAL"
            else
                return_error "Wrong arguments"
                exit
            fi
        fi
    elif [ "$CONF" == "name" ] ; then
        if $(validateString $VAL); then
            NAME="$VAL"
        else
            return_error "Wrong arguments"
            exit
        fi
    fi
done

if [ $ACTION == "go_preset" ] ; then
    if [ $NUM -lt 0 ] || [ $NUM -gt 7 ] ; then
        return_error "Preset number out of range"
        exit
    fi
    ipc_cmd -p $NUM
elif [ $ACTION == "add_preset" ]; then
    # add preset
    add_del_preset add $NAME
    if [ "$?" == "8" ]; then
        return_error "No free preset available"
        exit
    fi
    ipc_cmd -P
elif [ $ACTION == "del_preset" ]; then
    # del preset
    add_del_preset del $NUM
    ipc_cmd -R $NUM
else
    return_error "Invalid action received"
    exit
fi

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
