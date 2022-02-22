killall udhcpc
HN="yi-hack"
if [ -f /tmp/sd/yi-hack/etc/hostname ]; then
        HN=$(cat /tmp/sd/yi-hack/etc/hostname)
fi
if [ -f /backup/tools/default.script ]; then
    udhcpc -i wlan0 -b -s /backup/tools/default.script -x hostname:$HN
elif [ -f /home/app/script/default.script ]; then
    udhcpc -i wlan0 -b -s /home/app/script/default.script -x hostname:$HN
fi
