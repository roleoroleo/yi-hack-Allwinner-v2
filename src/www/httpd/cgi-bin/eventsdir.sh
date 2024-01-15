#!/bin/sh

CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/tmp/sd/yi-hack"

HOMEVER=$(cat /home/homever)
HV=${HOMEVER:0:2}

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

printf "Content-type: application/json\r\n\r\n"
printf "{\"records\":[\n"

COUNT=`ls -r /tmp/sd/record | grep H -c`
IDX=1
for f in `ls -r /tmp/sd/record | grep H`; do
    if [ ${#f} == 14 ]; then
        FS00="${f:0:4}-${f:5:2}-${f:8:2} ${f:11:2}:00"
        if [[ $(get_config EVENTS_TIME) == "autodetect" ]] ; then
            FS00E=$(date -d "$FS00" +"%s")
        elif [[ $(get_config EVENTS_TIME) == "local" ]] ; then
            FS00E=$(date -d "$FS00" +"%s")
        elif [[ $(get_config EVENTS_TIME) == "gmt" ]] ; then
            FS00E=$(date -u -d "$FS00" +"%s")
        fi
        FL=$(date +%YY%mM%dD%HH -d "@$FS00E")
        printf "{\n"
        printf "\"%s\":\"%s\",\n" "datetime" "Date: ${FL:0:4}-${FL:5:2}-${FL:8:2} Time: ${FL:11:2}:00"
        printf "\"%s\":\"%s\"\n" "dirname" "$f"
        if [ "$IDX" == "$COUNT" ]; then
            printf "}\n"
        else
            printf "},\n"
        fi
        IDX=$(($IDX+1))
    fi
done

printf "]}\n"
