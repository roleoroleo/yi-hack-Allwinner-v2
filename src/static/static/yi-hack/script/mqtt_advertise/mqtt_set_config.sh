#!/bin/sh

YI_HACK_PREFIX="/home/yi-hack"

CONFIG_SET="script/mqtt_advertise/mqtt_adv_config.sh"
CONF_FILE="etc/camera.conf"
CONF_MQTT_ADVERTISE_FILE="etc/mqtt_advertise.conf"
MQTT_FILE="etc/mqttv4.conf"

PATH=$PATH:$YI_HACK_PREFIX/bin:$YI_HACK_PREFIX/usr/bin
LD_LIBRARY_PATH=$YI_HACK_PREFIX/lib:$LD_LIBRARY_PATH

get_config() {
    key=^$1
    grep -w $key $YI_HACK_PREFIX/$MQTT_FILE | cut -d "=" -f2
}

get_mqtt_advertise_config() {
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_MQTT_ADVERTISE_FILE | cut -d "=" -f2
}

HOSTNAME=$(hostname)
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
MQTT_ADV_CAMERA_SETTING_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_TOPIC)

while :; do
    TOPIC=$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/+/set'
    SUBSCRIBED=$($YI_HACK_PREFIX/bin/mosquitto_sub -i $HOSTNAME -v -C 1 -h $HOST -t $TOPIC)
    CONF_UPPER=$(echo $SUBSCRIBED | awk '{print $1}' | awk -F / '{ print $(NF-1)}')
    CONF=$(echo $CONF_UPPER | awk '{ print tolower($0) }')
    VAL=$(echo $SUBSCRIBED | awk '{print $2}')

    sed -i "s/^\(${CONF_UPPER}\s*=\s*\).*$/\1${VAL}/" $YI_HACK_PREFIX/$CONF_FILE
    if [ "$CONF" == "switch_on" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -t off
        else
            ipc_cmd -t on
        fi
    elif [ "$CONF" == "save_video_on_motion" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -v always
        else
            ipc_cmd -v detect
        fi
    elif [ "$CONF" == "sensitivity" ]; then
        ipc_cmd -s $VAL
    elif [ "$CONF" == "ai_human_detection" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -a off
        else
            ipc_cmd -a on
        fi
    elif [ "$CONF" == "sound_detection" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -b off
        else
            ipc_cmd -b on
        fi
    elif [ "$CONF" == "led" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -l off
        else
            ipc_cmd -l on
        fi
    elif [ "$CONF" == "ir" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -i off
        else
            ipc_cmd -i on
        fi
    elif [ "$CONF" == "rotate" ]; then
        if [ "$VAL" == "no" ]; then
            ipc_cmd -r off
        else
            ipc_cmd -r on
        fi
    fi
    $YI_HACK_PREFIX/$CONFIG_SET
done
