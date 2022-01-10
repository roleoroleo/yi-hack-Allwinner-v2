#!/bin/sh

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

YI_HACK_PREFIX="/tmp/sd/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

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

printf "Content-type: application/json\r\n\r\n"

printf "{"
printf "\"%s\":\"%s\",\\n" "error" "false"
printf "\"date\":\"${DIR:0:4}-${DIR:5:2}-${DIR:8:2}\",\n"
printf "\"records\":[\n"

COUNT=`ls -r /tmp/sd/record/$DIR | grep mp4 -c`
IDX=1
for f in `ls -r /tmp/sd/record/$DIR | grep mp4`; do
    if [ ${#f} == 12 ] || [ ${#f} == 14 ]; then
        printf "{\n"
        if [ ${#f} == 14 ]; then
            printf "\"%s\":\"%s\",\n" "time" "Time: ${DIR:11:2}:${f:2:2}"
        else
            printf "\"%s\":\"%s\",\n" "time" "Time: ${DIR:11:2}:${f:0:2}"
        fi
        printf "\"%s\":\"%s\"\n" "filename" "$f"
        if [ "$IDX" == "$COUNT" ]; then
            printf "}\n"
        else
            printf "},\n"
        fi
        IDX=$(($IDX+1))
    fi
done

printf "]}\n"
