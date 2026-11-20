#!/bin/sh

export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

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

if [ ! -z "$MQTT_USER" ]; then
    MQTT_USER="-u $MQTT_USER"
fi

if [ ! -z "$MQTT_PASSWORD" ]; then
    MQTT_PASSWORD="-w $MQTT_PASSWORD"
fi
if [ ! -z "$MQTT_CLIENT_ID" ]; then
    MQTT_CLIENT_ID="-i $MQTT_CLIENT_ID"_conf2mqtt
fi

while IFS='=' read -r key val ; do
    lkey="$(echo $key | tr '[A-Z]' '[a-z]')"
    $YI_HACK_PREFIX/bin/mqtt-pub -T -h "$MQTT_IP" -p "$MQTT_PORT" $MQTT_CLIENT_ID $MQTT_USER $MQTT_PASSWORD $MQTT_TLS $MQTT_CA_CERT $MQTT_CLIENT_CERT $MQTT_CLIENT_KEY -n $MQTT_PREFIX/stat/camera/$lkey -m $val -r
    sleep 0.1
done < "$CAMERA_CONF_FILE"
