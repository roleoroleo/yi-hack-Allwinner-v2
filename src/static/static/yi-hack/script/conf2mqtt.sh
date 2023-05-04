#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
CAMERA_CONF_FILE="$YI_HACK_PREFIX/etc/camera.conf"
MQTT_CONF_FILE="$YI_HACK_PREFIX/etc/mqttv4.conf"

. $MQTT_CONF_FILE

if [ -z "$MQTT_IP" ]; then
    exit
fi
if [ -z "$MQTT_PORT" ]; then
    exit
fi

if [ ! -z "$MQTT_USER" ]; then
    MQTT_USER="-u $MQTT_USER"
fi

if [ ! -z "$MQTT_PASSWORD" ]; then
    MQTT_PASSWORD="-P $MQTT_PASSWORD"
fi

while IFS='=' read -r key val ; do
    lkey="$(echo $key | tr '[A-Z]' '[a-z]')"
    $YI_HACK_PREFIX/bin/mosquitto_pub -h "$MQTT_IP" -p "$MQTT_PORT" $MQTT_USER $MQTT_PASSWORD -t $MQTT_PREFIX/stat/camera/$lkey -m $val -r
done < "$CAMERA_CONF_FILE"
