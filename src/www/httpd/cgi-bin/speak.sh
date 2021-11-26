#!/bin/sh

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

LANG="en-US"

validateLang()
{
    RES=$(echo ${1} | sed -E 's/[a-zA-Z-]//g')
    if [ -z "$RES" ]; then
        LANG=$1
    else
        LANG="invalid"
    fi
}

PARAM="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f1)"
VALUE="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f2)"

if [ "$PARAM" == "lang" ] ; then
    LANG="$VALUE"
fi

validateLang $LANG
if [ "$LANG" == "invalid" ]; then
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Invalid language"
    printf "}"
    exit
fi

read -r POST_DATA

printf "Content-type: application/json\r\n\r\n"

if [ -f /tmp/sd/yi-hack/bin/nanotts ] && [ -e /tmp/audio_in_fifo ]; then
    TMP_FILE="/tmp/sd/speak.pcm"
    if [ ! -f $TMP_FILE ]; then
        speaker on > /dev/null
        echo "$POST_DATA" | /tmp/sd/yi-hack/bin/nanotts -l /tmp/sd/yi-hack/usr/share/pico/lang -v $LANG -c > $TMP_FILE
        cat $TMP_FILE > /tmp/audio_in_fifo
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
    if [ ! -f /tmp/sd/yi-hack/bin/speak ]; then
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "TTS engine not found"
        printf "}"
    elif [ ! -e /tmp/audio_in_fifo ]; then
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Audio input disabled"
        printf "}"
    fi
fi
