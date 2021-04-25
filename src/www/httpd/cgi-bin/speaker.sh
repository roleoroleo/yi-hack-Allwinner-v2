#!/bin/sh

printf "Content-type: application/json\r\n\r\n"
printf "{}"

cat - > /tmp/audio_in_fifo
