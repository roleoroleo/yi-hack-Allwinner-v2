#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

DIR="none"
TIME="0.3"

for I in 1 2
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "dir" ] ; then
        if [ "$MODEL_SUFFIX" == "h60ga" ] || [ "$MODEL_SUFFIX" == "h51ga" ] || [ "$MODEL_SUFFIX" == "h52ga" ]; then
            if [ "$VAL" == "up" ] || [ "$VAL" == "down" ]; then
                DIR="-m $VAL"
            else
                DIR="-M $VAL"
            fi
        else
            DIR="-M $VAL"
        fi
    elif [ "$CONF" == "time" ] ; then
        TIME="$VAL"
    fi
done

if ! $(validateString $DIR); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi
if ! $(validateNumber $TIME); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

if [ "$DIR" != "none" ] ; then
    ipc_cmd $DIR
    sleep $TIME
    ipc_cmd -M stop
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "\"%s\":\"%s\"\\n" "error" "false"
printf "}"
