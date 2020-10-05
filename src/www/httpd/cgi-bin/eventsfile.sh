#!/bin/sh

printf "Content-type: application/json\r\n\r\n"

case $QUERY_STRING in
    *[\'!\"@\#\$%^*\(\)_+.,:\;]* ) exit;;
esac

CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

if [ "$CONF" == "dirname" ]; then
     DIR=$VAL
fi

printf "{\"date\":\"${DIR:0:4}-${DIR:5:2}-${DIR:8:2}\",\n"
printf "\"records\":[\n"

COUNT=`ls -r /tmp/sd/record/$DIR | grep mp4 -c`
IDX=1
for f in `ls -r /tmp/sd/record/$DIR | grep mp4`; do
    if [ ${#f} == 12 ]; then
        printf "{\n"
        printf "\"%s\":\"%s\",\n" "time" "Time: ${DIR:11:2}:${f:0:2}"
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
