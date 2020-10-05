#!/bin/sh

printf "Content-type: application/json\r\n\r\n"

printf "{\"records\":[\n"

COUNT=`ls -r /tmp/sd/record | grep H -c`
IDX=1
for f in `ls -r /tmp/sd/record | grep H`; do
    if [ ${#f} == 14 ]; then
        printf "{\n"
        printf "\"%s\":\"%s\",\n" "datetime" "Date: ${f:0:4}-${f:5:2}-${f:8:2} Time: ${f:11:2}:00"
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
