#!/bin/sh

YI_HACK_PREFIX="/home/yi-hack"
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

HOSTNAME=$(hostname)
MQTT_IP=$(get_config MQTT_IP)
MQTT_PORT=$(get_config MQTT_PORT)
MQTT_USER=$(get_config MQTT_USER)
MQTT_PASSWORD=$(get_config MQTT_PASSWORD)
MQTT_QOS=$(get_config MQTT_QOS)

TOPIC_BIRTH_WILL=$(get_config TOPIC_BIRTH_WILL)
BIRTH_MSG=$(get_config BIRTH_MSG)
WILL_MSG=$(get_config WILL_MSG)

TOPIC_MOTION=$(get_config TOPIC_MOTION)
MOTION_START_MSG=$(get_config MOTION_START_MSG)
MOTION_STOP_MSG=$(get_config MOTION_STOP_MSG)

TOPIC_AI_HUMAN_DETECTION=$(get_config TOPIC_AI_HUMAN_DETECTION)
AI_HUMAN_DETECTION_START_MSG=$(get_config AI_HUMAN_DETECTION_START_MSG)
AI_HUMAN_DETECTION_STOP_MSG=$(get_config AI_HUMAN_DETECTION_STOP_MSG)

TOPIC_BABY_CRYING=$(get_config TOPIC_BABY_CRYING)
BABY_CRYING_MSG=$(get_config BABY_CRYING_MSG)

TOPIC_SOUND_DETECTION=$(get_config TOPIC_SOUND_DETECTION)
SOUND_DETECTION_MSG=$(get_config SOUND_DETECTION_MSG)

TOPIC_MOTION_IMAGE=$(get_config TOPIC_MOTION_IMAGE)

HOST=$MQTT_IP
if [ ! -z $MQTT_PORT ]; then
    HOST=$HOST' -p '$MQTT_PORT
fi
if [ ! -z $MQTT_USER ]; then
    HOST=$HOST' -u '$MQTT_USER' -P '$MQTT_PASSWORD
fi

MQTT_PREFIX=$(get_config MQTT_PREFIX)

HOMEASSISTANT_MQTT_PREFIX=$(get_mqtt_advertise_config HOMEASSISTANT_MQTT_PREFIX)
HOMEASSISTANT_RETAIN=$(get_mqtt_advertise_config HOMEASSISTANT_RETAIN)
HOMEASSISTANT_QOS=$(get_mqtt_advertise_config HOMEASSISTANT_QOS)
MQTT_ADV_INFO_GLOBAL_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_ENABLE)
MQTT_ADV_INFO_GLOBAL_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_TOPIC)
MQTT_ADV_INFO_GLOBAL_RETAIN=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_RETAIN)
MQTT_ADV_INFO_GLOBAL_QOS=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_QOS)
MQTT_ADV_CAMERA_SETTING_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_ENABLE)
MQTT_ADV_CAMERA_SETTING_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_TOPIC)
MQTT_ADV_CAMERA_SETTING_RETAIN=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_RETAIN)
MQTT_ADV_CAMERA_SETTING_QOS=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_QOS)
MQTT_ADV_TELEMETRY_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_ENABLE)
MQTT_ADV_TELEMETRY_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_TOPIC)
MQTT_ADV_TELEMETRY_RETAIN=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_RETAIN)
MQTT_ADV_TELEMETRY_QOS=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_RETAIN)
NAME=$(get_mqtt_advertise_config HOMEASSISTANT_NAME)
IDENTIFIERS=$(get_mqtt_advertise_config HOMEASSISTANT_IDENTIFIERS)
MANUFACTURER=$(get_mqtt_advertise_config HOMEASSISTANT_MANUFACTURER)
MODEL=$(get_mqtt_advertise_config HOMEASSISTANT_MODEL)
SW_VERSION=$(cat $YI_HACK_PREFIX/version)
if [ "$HOMEASSISTANT_RETAIN" == "1" ]; then
    HA_RETAIN="-r"
else
    HA_RETAIN=""
fi
if [ "$HOMEASSISTANT_QOS" == "0" ] || [ "$HOMEASSISTANT_QOS" == "1" ] || [ "$HOMEASSISTANT_QOS" == "2" ]; then
    HA_QOS="-q $HOMEASSISTANT_QOS"
else
    HA_QOS=""
fi

if [ "$MQTT_ADV_INFO_GLOBAL_ENABLE" == "yes" ]; then
    #Don't know why... ..Home Assistant don't allow retain for Sensor and Binary Sensor
    ## if [ "$MQTT_ADV_INFO_GLOBAL_RETAIN" == "1" ]; then
    ##    RETAIN='"retain":true, '
    ## else
        RETAIN=""
    ## fi
    if [ "$MQTT_ADV_INFO_GLOBAL_QOS" == "1" ] || [ "$MQTT_ADV_INFO_GLOBAL_QOS" == "2" ]; then
        QOS='"qos":'$MQTT_ADV_INFO_GLOBAL_QOS', '
    else
        QOS=""
    fi
    #Hostname
    UNIQUE_NAME=$NAME" Hostname"
    UNIQUE_ID=$IDENTIFIERS"-hostname"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/hostname/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:network","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.hostname }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #IP
    UNIQUE_NAME=$NAME" Local IP"
    UNIQUE_ID=$IDENTIFIERS"-local_ip"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/local_ip/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:ip","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.local_ip }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Netmask
    UNIQUE_NAME=$NAME" Netmask"
    UNIQUE_ID=$IDENTIFIERS"-netmask"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/netmask/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:ip","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.netmask }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Gateway
    UNIQUE_NAME=$NAME" Gateway"
    UNIQUE_ID=$IDENTIFIERS"-gateway"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/gateway/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:ip","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.gateway }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #WLan ESSID
    UNIQUE_NAME=$NAME" WiFi ESSID"
    UNIQUE_ID=$IDENTIFIERS"-wlan_essid"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/wlan_essid/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:wifi","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.wlan_essid }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Mac Address
    UNIQUE_NAME=$NAME" Mac Address"
    UNIQUE_ID=$IDENTIFIERS"-mac_addr"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/mac_addr/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:network","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.mac_addr }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Home Version
    UNIQUE_NAME=$NAME" Home Version"
    UNIQUE_ID=$IDENTIFIERS"-home_version"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/home_version/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:memory","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.home_version }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Firmware Version
    UNIQUE_NAME=$NAME" Firmware Version"
    UNIQUE_ID=$IDENTIFIERS"-fw_version"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/fw_version/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:network","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.fw_version }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Model Suffix
    UNIQUE_NAME=$NAME" Model Suffix"
    UNIQUE_ID=$IDENTIFIERS"-model_suffix"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/model_suffix/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:network","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.model_suffix }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Serial Number
    UNIQUE_NAME=$NAME" Serial Number"
    UNIQUE_ID=$IDENTIFIERS"-serial_number"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/serial_number/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:webcam","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_INFO_GLOBAL_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.serial_number }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
else
    for ITEM in hostname local_ip netmask gateway wlan_essid mac_addr home_version fw_version model_suffix serial_number; do
        TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/$ITEM/config
        $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -n
    done
fi
if [ "$MQTT_ADV_TELEMETRY_ENABLE" == "yes" ]; then
    #Don't know why... ..Home Assistant don't allow retain for Sensor and Binary Sensor
    ## if [ "$MQTT_ADV_TELEMETRY_RETAIN" == "1" ]; then
    ##    RETAIN='"retain":true, '
    ## else
        RETAIN=""
    ## fi
    if [ "$MQTT_ADV_TELEMETRY_QOS" == "1" ] || [ "$MQTT_ADV_TELEMETRY_QOS" == "2" ]; then
        QOS='"qos":'$MQTT_ADV_TELEMETRY_QOS', '
    else
        QOS=""
    fi
    #Total Memory
    UNIQUE_NAME=$NAME" Total Memory"
    UNIQUE_ID=$IDENTIFIERS"-total_memory"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/total_memory/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:memory","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_TELEMETRY_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.total_memory }}","unit_of_measurement":"KB", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Free Memory
    UNIQUE_NAME=$NAME" Free Memory"
    UNIQUE_ID=$IDENTIFIERS"-free_memory"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/free_memory/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:memory","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_TELEMETRY_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.free_memory }}","unit_of_measurement":"KB", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #FreeSD
    UNIQUE_NAME=$NAME" Free SD"
    UNIQUE_ID=$IDENTIFIERS"-free_sd"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/free_sd/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:micro-sd","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_TELEMETRY_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.free_sd|regex_replace(find=\"%\", replace=\"\", ignorecase=False) }}","unit_of_measurement":"%", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Load AVG
    UNIQUE_NAME=$NAME" Load AVG"
    UNIQUE_ID=$IDENTIFIERS"-load_avg"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/load_avg/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:network","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_TELEMETRY_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.load_avg }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #Uptime
    UNIQUE_NAME=$NAME" Uptime"
    UNIQUE_ID=$IDENTIFIERS"-uptime"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/uptime/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "device_class":"timestamp","icon":"mdi:timer-outline","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_TELEMETRY_TOPIC'","name":"'$UNIQUE_NAME'","'unique_id'":"'$UNIQUE_ID'","value_template":"{{ (as_timestamp(now())-(value_json.uptime|int))|timestamp_local }}", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    #WLanStrenght
    UNIQUE_NAME=$NAME" Wlan Strengh"
    UNIQUE_ID=$IDENTIFIERS"-wlan_strength"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/wlan_strength/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "device_class":"signal_strength","icon":"mdi:wifi","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_TELEMETRY_TOPIC'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ ((value_json.wlan_strength|int) * 100 / 70 )|int }}","unit_of_measurement":"%", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
else
    for ITEM in total_memory free_memory free_sd load_avg uptime wlan_strength; do
        TOPIC=$HOMEASSISTANT_MQTT_PREFIX/sensor/$IDENTIFIERS/$ITEM/config
        $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -n
    done
fi

# Motion Detection
UNIQUE_NAME=$NAME" Movement"
UNIQUE_ID=$IDENTIFIERS"-motion_detection"
TOPIC=$HOMEASSISTANT_MQTT_PREFIX/binary_sensor/$IDENTIFIERS/motion_detection/config
MQTT_RETAIN_MOTION=$(get_config MQTT_RETAIN_MOTION)
#Don't know why... ..Home Assistant don't allow retain for Sensor and Binary Sensor
# if [ "$MQTT_RETAIN_MOTION" == "1" ]; then
#    RETAIN='"retain":true, '
# else
    RETAIN=""
# fi
CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"}, "qos": "'$MQTT_QOS'", '$RETAIN' "device_class":"motion","state_topic":"'$MQTT_PREFIX'/'$TOPIC_MOTION'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","payload_on":"'$MOTION_START_MSG'","payload_off":"'$MOTION_STOP_MSG'", "platform": "mqtt"}'
$YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
# Human Detection
UNIQUE_NAME=$NAME" Human Detection"
UNIQUE_ID=$IDENTIFIERS"-ai_human_detection"
TOPIC=$HOMEASSISTANT_MQTT_PREFIX/binary_sensor/$IDENTIFIERS/ai_human_detection/config
MQTT_RETAIN_AI_HUMAN_DETECTION=$(get_config MQTT_RETAIN_AI_HUMAN_DETECTION)
#Don't know why... ..Home Assistant don't allow retain for Sensor and Binary Sensor
# if [ "$MQTT_RETAIN_AI_HUMAN_DETECTION" == "1" ]; then
#    RETAIN='"retain":true, '
# else
    RETAIN=""
# fi
CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"}, "qos": "'$MQTT_QOS'", '$RETAIN' "device_class":"motion","state_topic":"'$MQTT_PREFIX'/'$TOPIC_AI_HUMAN_DETECTION'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","payload_on":"'$AI_HUMAN_DETECTION_START_MSG'","payload_off":"'$AI_HUMAN_DETECTION_STOP_MSG'", "platform": "mqtt"}'
$YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
# Sound Detection
UNIQUE_NAME=$NAME" Sound Detection"
UNIQUE_ID=$IDENTIFIERS"-sound_detection"
TOPIC=$HOMEASSISTANT_MQTT_PREFIX/binary_sensor/$IDENTIFIERS/sound_detection/config
MQTT_RETAIN_SOUND_DETECTION=$(get_config MQTT_RETAIN_SOUND_DETECTION)
#Don't know why... ..Home Assistant don't allow retain for Sensor and Binary Sensor
# if [ "$MQTT_RETAIN_SOUND_DETECTION" == "1" ]; then
#    RETAIN='"retain":true, '
# else
    RETAIN=""
# fi
CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"}, "qos": "'$MQTT_QOS'", '$RETAIN' "device_class":"sound","state_topic":"'$MQTT_PREFIX'/'$TOPIC_SOUND_DETECTION'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","payload_on":"'$SOUND_DETECTION_MSG'","off_delay":60, "platform": "mqtt"}'
$YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
# try to remove baby_crying topic
TOPIC=$HOMEASSISTANT_MQTT_PREFIX/binary_sensor/$IDENTIFIERS/baby_crying/config
$YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME -h $HOST -t $TOPIC -m ""
# Motion Detection Image
UNIQUE_NAME=$NAME" Motion Detection Image"
UNIQUE_ID=$IDENTIFIERS"-motion_detection_image"
TOPIC=$HOMEASSISTANT_MQTT_PREFIX/camera/$IDENTIFIERS/motion_detection_image/config
MQTT_RETAIN_MOTION_IMAGE=$(get_config MQTT_RETAIN_MOTION_IMAGE)
#Don't know why... ..Home Assistant don't allow retain for Sensor and Binary Sensor
# if [ "$MQTT_RETAIN_MOTION_IMAGE" == "1" ]; then
#    RETAIN='"retain":true, '
# else
    RETAIN=""
# fi
CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"}, "qos": "'$MQTT_QOS'", '$RETAIN' "topic":"'$MQTT_PREFIX'/'$TOPIC_MOTION_IMAGE'","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'", "platform": "mqtt"}'
$YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
if [ "$MQTT_ADV_CAMERA_SETTING_ENABLE" == "yes" ]; then
    if [ "$MQTT_ADV_CAMERA_SETTING_RETAIN" == "1" ]; then
        RETAIN='"retain":true, '
    else
        RETAIN=""
    fi
    if [ "$MQTT_ADV_CAMERA_SETTING_QOS" == "1" ] || [ "$MQTT_ADV_CAMERA_SETTING_QOS" == "2" ]; then
        QOS='"qos":'$MQTT_ADV_CAMERA_SETTING_QOS', '
    else
        QOS=""
    fi
    # Switch On
    UNIQUE_NAME=$NAME" Switch Status"
    UNIQUE_ID=$IDENTIFIERS"-SWITCH_ON"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/SWITCH_ON/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:video","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'","command_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/SWITCH_ON/set","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.SWITCH_ON }}","payload_on":"yes","payload_off":"no", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    # Sound Detection
    UNIQUE_NAME=$NAME" Sound Detection"
    UNIQUE_ID=$IDENTIFIERS"-SOUND_DETECTION"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/SOUND_DETECTION/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"}, '$QOS' '$RETAIN' "icon":"mdi:music-note","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'","command_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/SOUND_DETECTION/set","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.SOUND_DETECTION }}","payload_on":"yes","payload_off":"no", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    # try to remove baby_crying topic
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/BABY_CRYING_DETECT/config
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME -h $HOST -t $TOPIC -n
    # Led
    UNIQUE_NAME=$NAME" Status Led"
    UNIQUE_ID=$IDENTIFIERS"-LED"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/LED/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:led-on","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'","command_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/LED/set","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.LED }}","payload_on":"yes","payload_off":"no", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    # IR
    UNIQUE_NAME=$NAME" IR Led"
    UNIQUE_ID=$IDENTIFIERS"-IR"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/IR/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:remote","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'","command_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/IR/set","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.IR }}","payload_on":"yes","payload_off":"no", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
    # Rotate
    UNIQUE_NAME=$NAME"  Rotate"
    UNIQUE_ID=$IDENTIFIERS"-ROTATE"
    TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/ROTATE/config
    CONTENT='{"availability_topic":"'$MQTT_PREFIX'/'$TOPIC_BIRTH_WILL'","payload_available":"'$BIRTH_MSG'","payload_not_available":"'$WILL_MSG'","device":{"identifiers":["'$IDENTIFIERS'"],"manufacturer":"'$MANUFACTURER'","model":"'$MODEL'","name":"'$NAME'","sw_version":"'$SW_VERSION'"},'$QOS' '$RETAIN' "icon":"mdi:monitor","state_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'","command_topic":"'$MQTT_PREFIX'/'$MQTT_ADV_CAMERA_SETTING_TOPIC'/ROTATE/set","name":"'$UNIQUE_NAME'","unique_id":"'$UNIQUE_ID'","value_template":"{{ value_json.ROTATE }}","payload_on":"yes","payload_off":"no", "platform": "mqtt"}'
    $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
else
    for ITEM in SWITCH_ON SOUND_DETECTION BABY_CRYING_DETECT LED IR ROTATE; do
        TOPIC=$HOMEASSISTANT_MQTT_PREFIX/switch/$IDENTIFIERS/$ITEM/config
        $YI_HACK_PREFIX/bin/mosquitto_pub -i $HOSTNAME $HA_QOS $HA_RETAIN -h $HOST -t $TOPIC -n
    done
fi
