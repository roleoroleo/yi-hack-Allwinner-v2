#!/bin/sh

printf "Content-type: application/json\r\n\r\n"

if [ ! -e /tmp/audio_in_fifo ]; then
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "\"%s\":\"%s\"\\n" "description" "Audio input is not available"
    printf "}"
    exit
fi

# Check if sd is mounted
mount | grep /tmp/sd > /dev/null
if [ $? -eq 0 ]; then
    # If yes, create temp file in /tmp/sd and don't wait for completion
    TMP_FILE="/tmp/sd/speaker.pcm"
    cat - > $TMP_FILE
    cat $TMP_FILE > /tmp/audio_in_fifo &

    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "false"
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
        cat - > $TMP_FILE
        cat $TMP_FILE > /tmp/audio_in_fifo
        sleep 1
        rm $TMP_FILE

        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "false"
        printf "\"%s\":\"%s\"\\n" "description" ""
        printf "}"
    else
        printf "{\n"
        printf "\"%s\":\"%s\"\\n" "error" "true"
        printf "\"%s\":\"%s\"\\n" "description" "File is too big"
        printf "}"
    fi
fi
