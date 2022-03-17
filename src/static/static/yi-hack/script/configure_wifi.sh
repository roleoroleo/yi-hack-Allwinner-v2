#!/bin/sh

function print_help {
    echo "configure_wifi.sh"
    echo "will be used on next boot"
}

CFG_FILE=/tmp/configure_wifi.cfg
if [ ! -f "$CFG_FILE" ]; then
    echo "configure_wifi.cfg not found"
    exit 1
fi

TMP=$(cat $CFG_FILE | grep wifi_ssid=)
SSID=$(echo "${TMP:10}")
TMP=$(cat $CFG_FILE | grep wifi_psk=)
KEY=$(echo "${TMP:9}")

if [ -z "$SSID" ]; then
    echo "error: ssid has not been set"
    print_help
    exit 1
fi
if [ ${#SSID} -gt 63 ]; then
    echo "error: ssid is too long"
    print_help
    exit 1
fi

if [ -z "$KEY" ]; then
    echo "error: key has not been set"
    print_help
    exit 1
fi
if [ ${#KEY} -gt 63 ]; then
    echo "error: key is too long"
    print_help
    exit 1
fi

CURRENT_SSID=$(dd bs=1 skip=28 count=64 if=/dev/mtdblock7 2>/dev/null)
CURRENT_KEY=$(dd bs=1 skip=92 count=64 if=/dev/mtdblock7 2>/dev/null)
CONNECTED_BIT=$(hexdump -s 24 -n 4 -v /dev/mtdblock7 | awk 'FNR <=1' | awk '{print $3$2}')

echo $SSID ${#SSID} - $CURRENT_SSID ${#CURRENT_SSID}
echo $KEY ${#KEY} - $CURRENT_KEY ${#CURRENT_KEY}

if [ "$SSID" == "$CURRENT_SSID" ] && [ "$KEY" == "$CURRENT_KEY" ] && [ "$CONNECTED_BIT" == "00000000" ]; then
    echo "ssid and key already configured"
    if [ $# -ne 1 ] || [ "$1" != "force" ]; then
        exit
    fi
fi

echo "creating partition backup..."
DATE=$(date '+%Y%m%d%H%M%S')
dd if=/dev/mtdblock7 of=/tmp/sd/mtdblock7_$DATE.bin 2>/dev/null

# clear the existing passwords (to ensure we are null terminated)
cat /dev/zero | dd of=/dev/mtdblock7 bs=1 seek=28 count=64 conv=notrunc
cat /dev/zero | dd of=/dev/mtdblock7 bs=1 seek=92 count=64 conv=notrunc
# write SSID
echo -n "$SSID" | dd of=/dev/mtdblock7 bs=1 seek=28 count=64 conv=notrunc
# write key
echo -n "$KEY" | dd of=/dev/mtdblock7 bs=1 seek=92 count=64 conv=notrunc
#write "connected" bit
printf "\00\00\00\00" | dd of=/dev/mtdblock7 bs=1 seek=24 count=4 conv=notrunc

sync
sync
sync
