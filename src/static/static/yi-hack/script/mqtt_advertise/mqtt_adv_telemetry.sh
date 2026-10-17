#!/bin/sh

export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

# Fix "STDIN Wake": if there is no terminal (boot/cron), re-run the script
if [ ! -t 0 ] && [ -z "$MQTT_STDIN_FIXED" ]; then
    export MQTT_STDIN_FIXED=1
    sleep 60 | "$0" "$@"
    exit $?
fi

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CONF_FILE="etc/mqttv4.conf"
CONF_MQTT_ADVERTISE_FILE="etc/mqtt_advertise.conf"

PATH=$PATH:$YI_HACK_PREFIX/bin:$YI_HACK_PREFIX/usr/bin
LD_LIBRARY_PATH=$YI_HACK_PREFIX/lib:$LD_LIBRARY_PATH

get_config() {
    key=^$1
    grep -w $key $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2-
}

get_mqtt_advertise_config() {
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_MQTT_ADVERTISE_FILE | cut -d "=" -f2-
}

UPTIME=$(cat /proc/uptime | cut -d ' ' -f1)
LOAD_AVG=$(cat /proc/loadavg | cut -d ' ' -f1-3)
TOTAL_MEMORY=$(free -k | awk 'NR==2{print $2}')
FREE_MEMORY=$(free -k | awk 'NR==2{print $7}')
FREE_SD=$(df | grep -m1 '/tmp/sd' |  grep mmc | awk '{print $5}' | tr -d '%')
WLAN_STRENGTH=$(cat /proc/net/wireless | grep wlan0 | awk '{ print $3 }' | sed 's/\.$//')

# CPU usage %: sample /proc/stat twice ~1s apart and diff busy vs total jiffies.
read _cpu u_user u_nice u_sys u_idle u_iowait u_irq u_softirq u_steal _rest < /proc/stat 2>/dev/null
BUSY1=$((u_user + u_nice + u_sys + u_irq + u_softirq + u_steal))
IDLE1=$((u_idle + u_iowait))
sleep 1
read _cpu v_user v_nice v_sys v_idle v_iowait v_irq v_softirq v_steal _rest < /proc/stat 2>/dev/null
BUSY2=$((v_user + v_nice + v_sys + v_irq + v_softirq + v_steal))
IDLE2=$((v_idle + v_iowait))
DTOT=$(( (BUSY2 + IDLE2) - (BUSY1 + IDLE1) ))
DBUSY=$(( BUSY2 - BUSY1 ))
if [ "$DTOT" -gt 0 ]; then
    CPU_USAGE=$(( (DBUSY * 100) / DTOT ))
else
    CPU_USAGE=0
fi

# SoC temperature: Allwinner exposes it through the thermal framework. The value
# may be in millidegrees (e.g. 45123) or already whole degrees; normalise to C.
TEMPERATURE="N/A"
for _tz in /sys/class/thermal/thermal_zone*/temp /sys/class/hwmon/hwmon*/temp1_input; do
    if [ -f "$_tz" ]; then
        _t=$(cat "$_tz" 2>/dev/null)
        case "$_t" in
            ''|*[!0-9-]*) continue ;;
        esac
        if [ "$_t" -gt 1000 ]; then
            TEMPERATURE=$((_t / 1000))
        else
            TEMPERATURE=$_t
        fi
        break
    fi
done

# Free swap (KB) - this device leans on swap, so it is worth keeping an eye on.
FREE_SWAP=$(free -k | awk '/^Swap:/{print $4}')
[ -z "$FREE_SWAP" ] && FREE_SWAP=0

# MQTT configuration

LD_LIBRARY_PATH=$YI_HACK_PREFIX/lib:$LD_LIBRARY_PATH

MQTT_IP=$(get_config MQTT_IP)
MQTT_PORT=$(get_config MQTT_PORT)
MQTT_USER=$(get_config MQTT_USER)
MQTT_PASSWORD=$(get_config MQTT_PASSWORD)
MQTT_TLS=$(get_config MQTT_TLS)
MQTT_CA_CERT=$(get_config MQTT_CA_CERT)
MQTT_CLIENT_CERT=$(get_config MQTT_CLIENT_CERT)
MQTT_CLIENT_KEY=$(get_config MQTT_CLIENT_KEY)

HOST=$MQTT_IP
if [ ! -z $MQTT_PORT ]; then
    HOST=$HOST' -p '$MQTT_PORT
fi
if [ ! -z $MQTT_USER ]; then
    HOST=$HOST' -u '$MQTT_USER' -w '$MQTT_PASSWORD
fi

if [ "$MQTT_TLS" == "1" ]; then
    MQTT_TLS="-t"

    if [ -f $YI_HACK_PREFIX/etc/mqtt/ca.crt ]; then
        MQTT_CA_CERT="-A $YI_HACK_PREFIX/etc/mqtt/ca.crt"

        if [ -f $YI_HACK_PREFIX/etc/mqtt/client.crt ]; then
            MQTT_CLIENT_CERT="-c $YI_HACK_PREFIX/etc/mqtt/client.crt"

            if [ -f $YI_HACK_PREFIX/etc/mqtt/client.key ]; then
                MQTT_CLIENT_KEY="-K $YI_HACK_PREFIX/etc/mqtt/client.key"
            else
                MQTT_CLIENT_CERT=""
                MQTT_CLIENT_KEY=""
            fi
        else
            MQTT_CLIENT_CERT=""
            MQTT_CLIENT_KEY=""
        fi
    else
        MQTT_TLS=""
        MQTT_CA_CERT=""
        MQTT_CLIENT_CERT=""
        MQTT_CLIENT_KEY=""
    fi
else
    MQTT_TLS=""
    MQTT_CA_CERT=""
    MQTT_CLIENT_CERT=""
    MQTT_CLIENT_KEY=""
fi

MQTT_PREFIX=$(get_config MQTT_PREFIX)
MQTT_ADV_TELEMETRY_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_TOPIC)
MQTT_ADV_TELEMETRY_RETAIN=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_RETAIN)
MQTT_ADV_TELEMETRY_QOS=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_QOS)
if [ "$MQTT_ADV_TELEMETRY_RETAIN" == "1" ]; then
    RETAIN="-r"
else
    RETAIN=""
fi
if [ "$MQTT_ADV_TELEMETRY_QOS" == "0" ] || [ "$MQTT_ADV_TELEMETRY_QOS" == "1" ] || [ "$MQTT_ADV_TELEMETRY_QOS" == "2" ]; then
    QOS="-q $MQTT_ADV_TELEMETRY_QOS"
else
    QOS=""
fi
TOPIC=$MQTT_PREFIX/$MQTT_ADV_TELEMETRY_TOPIC

# MQTT Publish
CONTENT="{ "
CONTENT=$CONTENT'"uptime":"'$UPTIME'",'
CONTENT=$CONTENT'"load_avg":"'$LOAD_AVG'",'
if [ ! -z "$FREE_SD" ]; then
    FREE_SD=$((100 - $FREE_SD))%
    CONTENT=$CONTENT'"free_sd":"'$FREE_SD'",'
fi
CONTENT=$CONTENT'"total_memory":"'$TOTAL_MEMORY'",'
CONTENT=$CONTENT'"free_memory":"'$FREE_MEMORY'",'
CONTENT=$CONTENT'"cpu_usage":"'$CPU_USAGE'",'
CONTENT=$CONTENT'"temperature":"'$TEMPERATURE'",'
CONTENT=$CONTENT'"free_swap":"'$FREE_SWAP'",'
CONTENT=$CONTENT'"wlan_strength":"'$WLAN_STRENGTH'"'
CONTENT=$CONTENT" }"
$YI_HACK_PREFIX/bin/mqtt-pub $QOS $RETAIN -h $HOST $MQTT_TLS $MQTT_CA_CERT $MQTT_CLIENT_CERT $MQTT_CLIENT_KEY -n $TOPIC -m "$CONTENT"
