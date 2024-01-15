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

validateRecDir()
{
    if [ "${#1}" != "14" ]; then
        DIR = "none"
    fi
    if [ "Y${1:4:1}" != "YY" ] ; then
        DIR = "none"
    fi
    if [ "M${1:7:1}" != "MM" ] ; then
        DIR = "none"
    fi
    if [ "D${1:10:1}" != "DD" ] ; then
        DIR = "none"
    fi
    if [ "H${1:13:1}" != "HH" ] ; then
        DIR = "none"
    fi
}

fbasename()
{
    echo ${1:0:$((${#1} - 4))}
}

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

DIR="none"

CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

if [ "$CONF" == "dirname" ]; then
     DIR=$VAL
fi

validateRecDir $DIR

if [ "$DIR" == "none" ] ; then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

DIRS00="${DIR:0:4}-${DIR:5:2}-${DIR:8:2} ${DIR:11:2}:00"
if [[ $(get_config EVENTS_TIME) == "autodetect" ]] ; then
    DIRS00E=$(date -d "$DIRS00" +"%s")
elif [[ $(get_config EVENTS_TIME) == "local" ]] ; then
    DIRS00E=$(date -d "$DIRS00" +"%s")
elif [[ $(get_config EVENTS_TIME) == "gmt" ]] ; then
    DIRS00E=$(date -u -d "$DIRS00" +"%s")
fi
DIRL=$(date +%YY%mM%dD%HH -d "@$DIRS00E")

printf "Content-type: application/json\r\n\r\n"

printf "{"
printf "\"%s\":\"%s\",\\n" "error" "false"
printf "\"date\":\"${DIRL:0:4}-${DIRL:5:2}-${DIRL:8:2}\",\n"
printf "\"records\":[\n"

COUNT=`ls -r /tmp/sd/record/$DIR | grep mp4 -c`
IDX=1
for f in `ls -r /tmp/sd/record/$DIR | grep mp4`; do
    if [ ${#f} == 12 ] || [ ${#f} == 14 ]; then
        base_name=$(fbasename "$f")
        if [ -f /tmp/sd/record/$DIR/$base_name.jpg ] && [ -s /tmp/sd/record/$DIR/$base_name.jpg ]; then
            thumbbasename="$base_name.jpg"
        else
            thumbbasename=""
        fi
        printf "{\n"
        if [ ${#f} == 14 ]; then
            printf "\"%s\":\"%s\",\n" "time" "Time: ${DIRL:11:2}:${f:2:2}"
        else
            printf "\"%s\":\"%s\",\n" "time" "Time: ${DIRL:11:2}:${f:0:2}"
        fi
        printf "\"%s\":\"%s\",\n" "filename" "$f"
        printf "\"%s\":\"%s\"\n" "thumbfilename" "$thumbbasename"
        if [ "$IDX" == "$COUNT" ]; then
            printf "}\n"
        else
            printf "},\n"
        fi
        IDX=$(($IDX+1))
    fi
done

printf "]}\n"
