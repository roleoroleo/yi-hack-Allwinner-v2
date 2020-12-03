#!/bin/sh

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "}\n"

# Yeah, it's pretty ugly.. but hey, it works.

sync
sync
sync
killall -q mqttv4
sleep 1
reboot
