#!/bin/sh

validateTLFile()
{
    if [ "${#1}" != "23" ]; then
        FILE="none"
    fi

    if [ "${1:4:1}" != "-" ] ; then
        FILE="none"
    fi
    if [ "${1:7:1}" != "-" ] ; then
        FILE="none"
    fi
    if [ "${1:10:1}" != "_" ] ; then
        FILE="none"
    fi
    if [ "${1:13:1}" != "-" ] ; then
        FILE="none"
    fi
    if [ "${1:16:1}" != "-" ] ; then
        FILE="none"
    fi
    if [ "${1:19:4}" != ".avi" ] ; then
        FILE="none"
    fi
}

fbasename()
{
    echo ${1:0:$((${#1} - 4))}
}

YI_HACK_PREFIX="/tmp/sd/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}\n"
    return
fi

ACTION="none"
FILE="none"

for I in 1 2
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "action" ]; then
        ACTION=$VAL
    elif [ "$CONF" == "file" ]; then
        FILE=$VAL
    else
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}\n"
        return
    fi
done

if [ "$ACTION" == "list" ]; then
    printf "Content-type: application/json\r\n\r\n"
    printf "{"
    printf "\"%s\":\"%s\",\\n" "error" "false"
    printf "\"records\":[\n"

    COUNT=`ls -r /tmp/sd/record/timelapse | grep .avi -c`
    IDX=1
    for f in `ls -r /tmp/sd/record/timelapse | grep .avi`; do
        if [ ${#f} == 23 ]; then
            printf "{\n"
            printf "\"%s\":\"%s\",\n" "date" "Date: ${f:0:10}"
            printf "\"%s\":\"%s\",\n" "time" "Time: ${f:11:2}:${f:14:2}:${f:17:2}"
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
elif [ "$ACTION" == "delete" ]; then
    if [ "$FILE" == "none" ]; then
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}\n"
        return
    fi
    validateTLFile $FILE
    if [ "$FILE" == "none" ]; then
        printf "Content-type: application/json\r\n\r\n"
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "}\n"
        return
    fi

    BASE_NAME=$(fbasename "$FILE")

    rm -f /tmp/sd/record/timelapse/$BASE_NAME.avi

    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "false"
    printf "}\n"

fi
