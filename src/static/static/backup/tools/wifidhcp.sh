killall udhcpc
udhcpc -i wlan0 -b -s /backup/tools/default.script -x hostname:$(hostname)

