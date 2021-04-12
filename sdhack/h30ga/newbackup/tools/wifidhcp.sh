killall udhcpc
HN="yi-hack"
if [ -f /tmp/sd/yi-hack/etc/hostname ]; then
        HN=$(cat /tmp/sd/yi-hack/etc/hostname)
fi
udhcpc -i wlan0 -b -s /backup/tools/default.script -x hostname:$HN
