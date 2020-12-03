#!/bin/sh

validateNumber()
{
    RES=$(echo ${1} | sed -E 's/^[0-9]*$//g')
    if [ -z $RES ]; then
        TIME=$1
    else
        TIME="invalid"
    fi
}

TIME=60

for I in 1
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "time" ] ; then
        TIME="$VAL"
    fi
done

validateNumber $TIME

if [ "$TIME" == "invalid" ] ; then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"error\":\"Invalid time\"\n"
    printf "}\n"
    exit
fi

ipc_cmd -S $TIME &

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "}\n"
