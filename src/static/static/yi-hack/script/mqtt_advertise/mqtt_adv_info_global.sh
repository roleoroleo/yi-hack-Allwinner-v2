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

HOSTNAME=$(hostname)
FW_VERSION=$(cat $YI_HACK_PREFIX/version)
HOME_VERSION=$(cat /home/app/.appver)
MODEL_SUFFIX=$(cat $YI_HACK_PREFIX/model_suffix)
MFG_PART=$(grep  -oE  ".{0,0}mfg@.{0,9}" /sys/firmware/devicetree/base/chosen/bootargs | cut -c 5-14)
SERIAL_NUMBER=$(dd bs=1 count=20 skip=36 if=/dev/$MFG_PART 2>/dev/null | tr '\0' '0' | cut -c1-20)
LOCAL_IP=$(ifconfig eth0 | awk '/inet addr/{print substr($2,6)}')
NETMASK=$(ifconfig eth0 | awk '/inet addr/{print substr($4,6)}')
MAC_ADDR=$(ifconfig eth0 | awk '/HWaddr/{print substr($5,1)}')
if [ -z $LOCAL_IP ]; then
    LOCAL_IP=$(ifconfig wlan0 | awk '/inet addr/{print substr($2,6)}')
    NETMASK=$(ifconfig wlan0 | awk '/inet addr/{print substr($4,6)}')
    MAC_ADDR=$(ifconfig wlan0 | awk '/HWaddr/{print substr($5,1)}')
fi
GATEWAY=$(route -n | awk 'NR==3{print $2}')
WLAN_ESSID=$(iwconfig wlan0 | grep ESSID | cut -d\" -f2)

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
MQTT_ADV_INFO_GLOBAL_TOPIC=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_TOPIC)
MQTT_ADV_INFO_GLOBAL_RETAIN=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_RETAIN)
MQTT_ADV_INFO_GLOBAL_QOS=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_QOS)
if [ "$MQTT_ADV_INFO_GLOBAL_RETAIN" == "1" ]; then
    RETAIN="-r"
else
    RETAIN=""
fi
if [ "$MQTT_ADV_INFO_GLOBAL_QOS" == "0" ] || [ "$MQTT_ADV_INFO_GLOBAL_QOS" == "1" ] || [ "$MQTT_ADV_INFO_GLOBAL_QOS" == "2" ]; then
    QOS="-q $MQTT_ADV_INFO_GLOBAL_QOS"
else
    QOS=""
fi
TOPIC=$MQTT_PREFIX/$MQTT_ADV_INFO_GLOBAL_TOPIC

# MQTT Publish
CONTENT="{ "
CONTENT=$CONTENT'"hostname":"'$HOSTNAME'",'
CONTENT=$CONTENT'"fw_version":"'$FW_VERSION'",'
CONTENT=$CONTENT'"home_version":"'$HOME_VERSION'",'
CONTENT=$CONTENT'"model_suffix":"'$MODEL_SUFFIX'",'
CONTENT=$CONTENT'"serial_number":"'$SERIAL_NUMBER'",'
CONTENT=$CONTENT'"local_ip":"'$LOCAL_IP'",'
CONTENT=$CONTENT'"netmask":"'$NETMASK'",'
CONTENT=$CONTENT'"gateway":"'$GATEWAY'",'
CONTENT=$CONTENT'"mac_addr":"'$MAC_ADDR'",'
CONTENT=$CONTENT'"wlan_essid":"'$WLAN_ESSID'"'
CONTENT=$CONTENT" }"
$YI_HACK_PREFIX/bin/mosquitto_pub $QOS $RETAIN -h $HOST -t $TOPIC -m "$CONTENT"
