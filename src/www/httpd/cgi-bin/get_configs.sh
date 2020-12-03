#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

get_conf_type()
{
    CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

    if [ "$CONF" == "conf" ] ; then
        echo $VAL
    fi
}

printf "Content-type: application/json\r\n\r\n"

CONF_TYPE="$(get_conf_type)"
CONF_FILE=""

if [ "$CONF_TYPE" == "mqtt" ] ; then
    CONF_FILE="$YI_HACK_PREFIX/etc/mqttv4.conf"
else
    CONF_FILE="$YI_HACK_PREFIX/etc/$CONF_TYPE.conf"
fi

printf "{\n"

while IFS= read -r LINE ; do
    if [ ! -z "$LINE" ] ; then
        if [ "$LINE" == "${LINE#\#}" ] ; then # skip comments
#            printf "\"%s\",\n" $(echo "$LINE" | sed -r 's/\\/\\\\/g;s/"/\\"/g;s/=/":"/g;') # Format to json and replace = with ":"
            echo -n "\""
            echo -n "$LINE" | sed -r 's/=/":"/g;'
            echo "\","
        fi
    fi
done < "$CONF_FILE"

if [ "$CONF_TYPE" == "system" ] ; then
    printf "\"%s\":\"%s\",\n"  "HOSTNAME" "$(cat $YI_HACK_PREFIX/etc/hostname | sed -r 's/\\/\\\\/g;s/"/\\"/g;')"
fi

# Empty values to "close" the json
printf "\"%s\":\"%s\"\n"  "NULL" "NULL"

printf "}"
