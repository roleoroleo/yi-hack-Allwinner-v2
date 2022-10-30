#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

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

for I in 1 2
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "action" ] ; then
        ACTION="$VAL"
    elif [ "$CONF" == "num" ] ; then
        if $(validateNumber $VAL); then
            NUM="$VAL"
        fi
    fi
done

if [ $ACTION == "go_preset" ] ; then
    if [ $NUM -lt 0 ] || [ $NUM -gt 7 ] ; then
        return_error "Preset number out of range"
        exit
    fi
    ipc_cmd -p $NUM
elif [ $ACTION == "set_preset" ] || [ "$REQUEST_METHOD" == "POST" ]; then
    # set preset
    ipc_cmd -P
else
    return_error "Invalid action received"
    exit
fi

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
