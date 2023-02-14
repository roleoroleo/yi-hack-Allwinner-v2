#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

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
TOPIC=$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/+/set'

while :; do
$YI_HACK_PREFIX/bin/mosquitto_sub -v -h $HOST -t $TOPIC | while read -r SUBSCRIBED; do
    CONF_UPPER=$(echo $SUBSCRIBED | awk '{print $1}' | awk -F / '{ print $(NF-1)}')
    CONF=$(echo $CONF_UPPER | awk '{ print tolower($0) }')
    VAL=$(echo $SUBSCRIBED | awk '{print $2}')

    sed -i "s/^\(${CONF_UPPER}\s*=\s*\).*$/\1${VAL}/" $YI_HACK_PREFIX/$CONF_FILE
    IPC_OPT=""
    case "$CONF" in
        switch_on)
            IPC_OPT="-t"
        ;;
        save_video_on_motion)
            IPC_OPT="-v"
            if [ "$VAL" == "no" ] || [ "$VAL" == "off" ] ; then
                VAL="always"
            else
                VAL="detect"
            fi
        ;;
        motion_detection)
            IPC_OPT="-O"
        ;;
        sensitivity)
            IPC_OPT="-s"
        ;;
        ai_human_detection)
            IPC_OPT="-a"
        ;;
        ai_vehicle_detection)
            IPC_OPT="-E"
        ;;
        ai_animal_detection)
            IPC_OPT="-N"
        ;;
        face_detection)
            IPC_OPT="-c"
        ;;
        motion_tracking)
            IPC_OPT="-o"
        ;;
        sound_detection)
            IPC_OPT="-b"
        ;;
        led)
            IPC_OPT="-l"
        ;;
        ir)
            IPC_OPT="-i"
        ;;
        rotate)
            IPC_OPT="-r"
        ;;
        ptz_preset)
            IPC_OPT="-p"
        ;;
    esac
    if [ "$VAL" == "no" ] || [ "$VAL" == "off" ] ; then
        VAL="off"
    elif [ "$VAL" == "yes" ] || [ "$VAL" == "on" ] ; then
        VAL="on"
    fi
    ipc_cmd $IPC_OPT $VAL &

    $YI_HACK_PREFIX/$CONFIG_SET
done

# program exited, wait and retry
sleep 4
done
