#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CONF_FILE="etc/mqttv4.conf"
CONF_MQTT_ADVERTISE_FILE="etc/mqtt_advertise.conf"

PATH=$PATH:$YI_HACK_PREFIX/bin:$YI_HACK_PREFIX/usr/bin
LD_LIBRARY_PATH=$YI_HACK_PREFIX/lib:$LD_LIBRARY_PATH

get_config() {
    key=^$1
    grep -w $key $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

get_mqtt_advertise_config() {
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_MQTT_ADVERTISE_FILE | cut -d "=" -f2
}

UPTIME=$(cat /proc/uptime | cut -d ' ' -f1)
LOAD_AVG=$(cat /proc/loadavg | cut -d ' ' -f1-3)
TOTAL_MEMORY=$(free -k | awk 'NR==2{print $2}')
FREE_MEMORY=$(free -k | awk 'NR==2{print $7}')
FREE_SD=$(df | grep -m1 '/tmp/sd' |  grep mmc | awk '{print $5}' | tr -d '%')
WLAN_STRENGTH=$(cat /proc/net/wireless | grep wlan0 | awk '{ print $3 }' | sed 's/\.$//')

# MQTT configuration

LD_LIBRARY_PATH=$YI_HACK_PREFIX/lib:$LD_LIBRARY_PATH

MQTT_IP=$(get_config MQTT_IP)
MQTT_PORT=$(get_config MQTT_PORT)
MQTT_USER=$(get_config MQTT_USER)
MQTT_PASSWORD=$(get_config MQTT_PASSWORD)

HOST=$MQTT_IP
if [ ! -z $MQTT_PORT ]; then
    HOST=$HOST' -p '$MQTT_PORT
fi
if [ ! -z $MQTT_USER ]; then
    HOST=$HOST' -u '$MQTT_USER' -P '$MQTT_PASSWORD
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
CONTENT=$CONTENT'"wlan_strength":"'$WLAN_STRENGTH'"'
CONTENT=$CONTENT" }"
$YI_HACK_PREFIX/bin/mosquitto_pub $QOS $RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
