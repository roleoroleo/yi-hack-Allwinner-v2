#!/bin/sh

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

YI_HACK_PREFIX="/tmp/sd/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Wrong parameter"
    printf "}"
    exit
fi

VOL="1"
VOLDB="0"
IS_DB="0"

PARAM="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f1)"
VALUE="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f2)"

if [ "$PARAM" == "vol" ] ; then
    VOL="$VALUE"

    if ! $(validateNumber $VOL); then
        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Invalid volume"
        printf "}"
        exit
    fi
    IS_DB=0
fi

if [ "$PARAM" == "voldb" ] ; then
    VOLDB="$VALUE"

    if ! $(validateNumber $VOLDB); then
        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "Invalid volume (dB)"
        printf "}"
        exit
    fi
    IS_DB=1
fi

printf "Content-type: application/json\r\n\r\n"

if [ ! -e /tmp/audio_in_fifo ]; then
    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Audio input is not available"
    printf "}"
    exit
fi

# Check if sd is mounted
mount | grep /tmp/sd > /dev/null
if [ $? -eq 0 ]; then
    # If yes, create temp file in /tmp/sd and don't wait for completion
    TMP_FILE="/tmp/sd/speaker.pcm"
    cat - > $TMP_FILE.tmp
    BOUNDARY=$(cat $TMP_FILE.tmp | sed -n '1p' | tr -d '\r\n');

    if [ ! -z $BOUNDARY ]; then
        cat $TMP_FILE.tmp | sed '1,/Content-Type:/d' | tail -c +3 | tail -n +1 | sed '$ d' > $TMP_FILE
        LEN=$(ls -la $TMP_FILE | awk '{print $5}')
        dd if=/dev/null of=$TMP_FILE obs=$((LEN-2)) seek=1
        rm $TMP_FILE.tmp
    else
        mv $TMP_FILE.tmp $TMP_FILE
    fi

    # Check if it's a wave file
    RIFF=$(dd if=$TMP_FILE bs=1 count=4)
    if [ "$RIFF" == "RIFF" ]; then
        dd if=$TMP_FILE of=$TMP_FILE.tmp bs=1 skip=4
        mv $TMP_FILE.tmp $TMP_FILE
    fi

    if
    if [ "$IS_DB" == "1" ]; then
        (speaker on > /dev/null; cat $TMP_FILE | pcmvol -G $VOL > /tmp/audio_in_fifo; sleep 1; speaker off > /dev/null; rm $TMP_FILE) &
    else
        (speaker on > /dev/null; cat $TMP_FILE | pcmvol -g $VOL > /tmp/audio_in_fifo; sleep 1; speaker off > /dev/null; rm $TMP_FILE) &
    fi

    printf "{\n"
    printf "\"%s\":\"%s\",\\n" "error" "false"
    printf "\"%s\":\"%s\"\\n" "description" ""
    printf "}"
else
    # If not, create temp file in /tmp, wait for completion and remove the file
    # But limit the size to 512000 bytes = 16 seconds
    if [ -z "$CONTENT_LENGTH" ]; then
        CONTENT_LENGTH = 0
    fi
    if [ "$CONTENT_LENGTH" -le "512000" ]; then
        TMP_FILE="/tmp/speaker.pcm"
        cat - > $TMP_FILE.tmp
        BOUNDARY=$(cat $TMP_FILE.tmp | sed -n '1p' | tr -d '\r\n');

        if [ ! -z $BOUNDARY ]; then
            cat $TMP_FILE.tmp | sed '1,/Content-Type:/d' | tail -c +3 | tail -n +1 | sed '$ d' > $TMP_FILE
            LEN=$(ls -la $TMP_FILE | awk '{print $5}')
            dd if=/dev/null of=$TMP_FILE obs=$((LEN-2)) seek=1
            rm $TMP_FILE.tmp
        else
            mv $TMP_FILE.tmp $TMP_FILE
        fi

        # Check if it's a wave file
        RIFF=$(dd if=$TMP_FILE bs=1 count=4)
        if [ "$RIFF" == "RIFF" ]; then
            dd if=$TMP_FILE of=$TMP_FILE.tmp bs=1 skip=4
            mv $TMP_FILE.tmp $TMP_FILE
        fi

        speaker on > /dev/null

        if [ "$IS_DB" == "1" ]; then
            cat $TMP_FILE | pcmvol -G $VOL > /tmp/audio_in_fifo
        else
            cat $TMP_FILE | pcmvol -g $VOL > /tmp/audio_in_fifo
        fi
        sleep 1
        speaker off > /dev/null
        rm $TMP_FILE

        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "false"
        printf "\"%s\":\"%s\"\\n" "description" ""
        printf "}"
    else
        printf "{\n"
        printf "\"%s\":\"%s\",\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "File is too big"
        printf "}"
    fi
fi
