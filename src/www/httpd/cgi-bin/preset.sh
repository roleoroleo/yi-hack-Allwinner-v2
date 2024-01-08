#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
PTZ_CONF_FILE=$YI_HACK_PREFIX/etc/ptz_presets.conf
PTZ_SCRIPT=$YI_HACK_PREFIX/script/ptz_presets.sh

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

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

if [ "$ACTION" == "none" ] || [ "$ACTION" == "get_presets" ]; then
    return_error -99
    exit
fi

if [ "$NUM" != "-1" ]; then
    NUM="-n $NUM"
else
    NUM=""
fi
if [ ! -z $NAME ]; then
    NAME="-m $NAME"
else
    NAME=""
fi

RES=$($PTZ_SCRIPT -a $ACTION $NUM $NAME)

if [ "$RES" == "" ]; then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "false"
    printf "}"
else
    return_error $RES
fi
