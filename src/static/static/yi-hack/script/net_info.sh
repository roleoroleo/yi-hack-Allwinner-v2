#!/bin/sh

#
# Active network interface information.
#
# The active interface is eth0 when its carrier is up and it has an address,
# otherwise wlan0 when it has an address. On cameras without Ethernet this
# always resolves to wlan0. The Ethernet-only monitor clears the wlan0 address
# while eth0 is in use, so this also prevents the status page and the links
# from reporting a stale address during the Wi-Fi fallback.
#

YI_HACK_PREFIX="${YI_HACK_PREFIX:-/tmp/sd/yi-hack}"

iface_has_ip()
{
    ifconfig "$1" 2>/dev/null | grep -q "inet addr:"
}

iface_carrier()
{
    cat /sys/class/net/"$1"/carrier 2>/dev/null
}

#
# Ethernet port detection: the known RJ45 models are always capable, every
# other model is considered capable once the interface has seen a carrier.
# Used to gate ETH_ONLY in net_wd.sh, wd.sh and status.json.
#
eth_capable()
{
    { read -r MODEL < "$YI_HACK_PREFIX/model_suffix"; } 2>/dev/null
    case "$MODEL" in
        r30gb|r35gb|r37gb)
            return 0
            ;;
    esac

    { read -r ETH_CHANGES < /sys/class/net/eth0/carrier_changes; } 2>/dev/null
    case "$ETH_CHANGES" in
        ''|*[!0-9]*)
            return 1
            ;;
        *)
            [ "$ETH_CHANGES" -gt 0 ]
            ;;
    esac
}

active_interface()
{
    if [ "$(iface_carrier eth0)" == "1" ] && iface_has_ip eth0 ; then
        echo eth0
    elif iface_has_ip wlan0 ; then
        echo wlan0
    elif iface_has_ip eth0 ; then
        echo eth0
    else
        echo none
    fi
}

iface_ip()
{
    ifconfig "$1" 2>/dev/null | awk '/inet addr/{print substr($2,6); exit}'
}

iface_netmask()
{
    ifconfig "$1" 2>/dev/null | awk '/inet addr/{print substr($4,6); exit}'
}

iface_mac()
{
    ifconfig "$1" 2>/dev/null | awk '/HWaddr/{print substr($5,1); exit}'
}

default_gateway()
{
    route -n 2>/dev/null | awk '$1 == "0.0.0.0" {print $2; exit}'
}

eth_link()
{
    if [ "$(iface_carrier eth0)" == "1" ] ; then
        echo up
    else
        echo down
    fi
}

eth_speed()
{
    if [ "$(iface_carrier eth0)" != "1" ] ; then
        return
    fi

    SPEED=$(cat /sys/class/net/eth0/speed 2>/dev/null)
    DUPLEX=$(cat /sys/class/net/eth0/duplex 2>/dev/null)
    case "$SPEED" in
        ''|*[!0-9]*)
            return
            ;;
    esac

    if [ -n "$DUPLEX" ] ; then
        echo "$SPEED Mbps $DUPLEX duplex"
    else
        echo "$SPEED Mbps"
    fi
}

wifi_state()
{
    if [ -f /tmp/wifi_off ] ; then
        echo off
    elif [ -f /tmp/wifi_fallback ] ; then
        echo fallback
    else
        echo on
    fi
}

wlan_essid()
{
    iwconfig wlan0 2>/dev/null | sed -n 's/.*ESSID:"\([^"]*\)".*/\1/p'
}

wlan_strength()
{
    cat /proc/net/wireless 2>/dev/null | awk '/wlan0/ {print $3}' | sed 's/\.$//'
}
