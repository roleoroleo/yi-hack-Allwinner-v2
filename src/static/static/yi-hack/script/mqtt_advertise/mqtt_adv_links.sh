#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CONF_FILE="etc/mqttv4.conf"
CONF_MQTT_ADVERTISE_FILE="etc/mqtt_advertise.conf"

CONTENT=$($YI_HACK_PREFIX/www/cgi-bin/links.sh | sed 1d | sed ':a;N;$!ba;s/\n//g')
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
MQTT_ADV_LINK_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_LINK_TOPIC)
MQTT_ADV_LINK_RETAIN=$(get_mqtt_advertise_config MQTT_ADV_LINK_RETAIN)
MQTT_ADV_LINK_QOS=$(get_mqtt_advertise_config MQTT_ADV_LINK_QOS)
if [ "$MQTT_ADV_LINK_RETAIN" == "1" ]; then
    RETAIN="-r"
else
    RETAIN=""
fi
if [ "$MQTT_ADV_LINK_QOS" == "0" ] || [ "$MQTT_ADV_LINK_QOS" == "1" ] || [ "$MQTT_ADV_LINK_QOS" == "2" ]; then
    QOS="-q $MQTT_ADV_LINK_QOS"
else
    QOS=""
fi
TOPIC=$MQTT_PREFIX/$MQTT_ADV_LINK_TOPIC


$YI_HACK_PREFIX/bin/mosquitto_pub $QOS $RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
