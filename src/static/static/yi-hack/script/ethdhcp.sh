killall udhcpc
HN="yi-hack"
CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/tmp/sd/yi-hack"

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

STATIC_IP=$(get_config STATIC_IP)
STATIC_MASK=$(get_config STATIC_MASK)
STATIC_GW=$(get_config STATIC_GW)
STATIC_DNS1=$(get_config STATIC_DNS1)
STATIC_DNS2=$(get_config STATIC_DNS2)

if [ -z $STATIC_IP ] || [ -z $STATIC_MASK ]; then
    if [ -f /tmp/sd/yi-hack/etc/hostname ]; then
        HN=$(cat /tmp/sd/yi-hack/etc/hostname)
    fi
    if [ -f /backup/tools/default.script ]; then
        udhcpc -i eth0 -b -s /backup/tools/default.script -x hostname:$HN
    elif [ -f /home/app/script/default.script ]; then
        udhcpc -i eth0 -b -s /home/app/script/default.script -x hostname:$HN
    fi
else
    ifconfig wlan0 $STATIC_IP netmask $STATIC_MASK
    if [ ! -z $STATIC_GW ]; then
        route add -net 0.0.0.0 gw $STATIC_GW
    fi
    if [ ! -z $STATIC_DNS1 ] || [ ! -z $STATIC_DNS2 ]; then
        rm /tmp/resolv.conf
        touch /tmp/resolv.conf
        if [ ! -z $STATIC_DNS1 ]; then
            echo "nameserver $STATIC_DNS1" >> /tmp/resolv.conf
        fi
        if [ ! -z $STATIC_DNS2 ]; then
            echo "nameserver $STATIC_DNS2" >> /tmp/resolv.conf
        fi
    fi
fi
