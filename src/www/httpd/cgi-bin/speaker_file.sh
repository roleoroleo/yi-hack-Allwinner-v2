#!/bin/sh

# Play a file stored in FILE_PATH by name 

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

YI_HACK_PREFIX="/tmp/sd/yi-hack"
FILE_PATH="/tmp/sd/audio/"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

LANG="en-US"
VOLDB="1"

for I in 1 2
do
    PARAM="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VALUE="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$PARAM" == "voldb" ] ; then
        VOLDB="$VALUE"
    fi
done

if ! $(validateNumber $VOLDB); then
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Invalid volume"
    printf "}"
    exit
fi

read -r POST_DATA

printf "Content-type: application/json\r\n\r\n"

if [ -f /tmp/sd/audio/$POST_DATA ] && [ -e /tmp/audio_in_fifo ]; then
    TMP_FILE="/tmp/sd/speak.pcm"
    if [ ! -f $TMP_FILE ]; then
        speaker on > /dev/null
        cat $FILE_PATH$POST_DATA > $TMP_FILE
        cat $TMP_FILE | pcmvol -G $VOLDB > /tmp/audio_in_fifo
        sleep 1
        speaker off > /dev/null
        rm $TMP_FILE

        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "false"
        printf "\"%s\":\"%s\"\\n" "description" "$POST_DATA"
        printf "}"
    else
        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Speaker busy"
        printf "}"
    fi
else
    if [ ! -f $FILE_PATH$POST_DATA ]; then
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "File not found"
        printf "}"
    elif [ ! -e /tmp/audio_in_fifo ]; then
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Audio input disabled"
        printf "}"
    fi
fi
