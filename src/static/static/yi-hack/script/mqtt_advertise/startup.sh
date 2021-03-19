#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
PATH=$PATH:$YI_HACK_PREFIX/bin:$YI_HACK_PREFIX/usr/bin
CONF_MQTT_ADVERTISE_FILE="etc/mqtt_advertise.conf"

get_mqtt_advertise_config() {
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_MQTT_ADVERTISE_FILE | cut -d "=" -f2
}

$YI_HACK_PREFIX/script/mqtt_advertise/check_conf.sh

MQTT_ADV_LINK_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_LINK_ENABLE)
MQTT_ADV_LINK_BOOT=$(get_mqtt_advertise_config MQTT_ADV_LINK_BOOT)
MQTT_ADV_LINK_CRON=$(get_mqtt_advertise_config MQTT_ADV_LINK_CRON)
MQTT_ADV_LINK_CRONTAB=$(get_mqtt_advertise_config MQTT_ADV_LINK_CRONTAB)
MQTT_ADV_INFO_GLOBAL_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_ENABLE)
MQTT_ADV_INFO_GLOBAL_BOOT=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_BOOT)
MQTT_ADV_INFO_GLOBAL_CRON=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_CRON)
MQTT_ADV_INFO_GLOBAL_CRONTAB=$(get_mqtt_advertise_config MQTT_ADV_INFO_GLOBAL_CRONTAB)
MQTT_ADV_CAMERA_SETTING_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_ENABLE)
MQTT_ADV_CAMERA_SETTING_BOOT=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_BOOT)
MQTT_ADV_CAMERA_SETTING_CRON=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_CRON)
MQTT_ADV_CAMERA_SETTING_CRONTAB=$(get_mqtt_advertise_config MQTT_ADV_CAMERA_SETTING_CRONTAB)
MQTT_ADV_TELEMETRY_ENABLE=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_ENABLE)
MQTT_ADV_TELEMETRY_BOOT=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_BOOT)
MQTT_ADV_TELEMETRY_CRON=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_CRON)
MQTT_ADV_TELEMETRY_CRONTAB=$(get_mqtt_advertise_config MQTT_ADV_TELEMETRY_CRONTAB)
HOMEASSISTANT_ENABLE=$(get_mqtt_advertise_config HOMEASSISTANT_ENABLE)
HOMEASSISTANT_BOOT=$(get_mqtt_advertise_config HOMEASSISTANT_BOOT)
HOMEASSISTANT_CRON=$(get_mqtt_advertise_config HOMEASSISTANT_CRON)
HOMEASSISTANT_CRONTAB=$(get_mqtt_advertise_config HOMEASSISTANT_CRONTAB)

if [ "$MQTT_ADV_LINK_ENABLE" == "yes" ]; then
    if [ "$MQTT_ADV_LINK_BOOT" == "yes" ]; then
        $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_links.sh &
    fi
    if [ "$MQTT_ADV_LINK_CRON" == "yes" ]; then
        echo "$MQTT_ADV_LINK_CRONTAB  $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_link.sh" >>/var/spool/cron/crontabs/root
    fi
fi
if [ "$MQTT_ADV_INFO_GLOBAL_ENABLE" == "yes" ]; then
    if [ "$MQTT_ADV_INFO_GLOBAL_BOOT" == "yes" ]; then
        $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_info_global.sh &
    fi
    if [ "$MQTT_ADV_INFO_GLOBAL_CRON" == "yes" ]; then
        echo "$MQTT_ADV_INFO_GLOBAL_CRONTAB  $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_info_global.sh" >>/var/spool/cron/crontabs/root
    fi
fi
if [ "$MQTT_ADV_CAMERA_SETTING_ENABLE" == "yes" ]; then
    if [ "$MQTT_ADV_CAMERA_SETTING_BOOT" == "yes" ]; then
        $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_config.sh &
    fi
    if [ "$MQTT_ADV_CAMERA_SETTING_CRON" == "yes" ]; then
        echo "$MQTT_ADV_CAMERA_SETTING_CRONTAB  $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_config.sh" >>/var/spool/cron/crontabs/root
    fi
    FW_VERSION=$(cat $YI_HACK_PREFIX/version)

    $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_set_config.sh &
fi
if [ "$MQTT_ADV_TELEMETRY_ENABLE" == "yes" ]; then
    if [ "$MQTT_ADV_TELEMETRY_BOOT" == "yes" ]; then
        $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_telemetry.sh &
    fi
    if [ "$MQTT_ADV_TELEMETRY_CRON" == "yes" ]; then
        echo "$MQTT_ADV_TELEMETRY_CRONTAB  $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_telemetry.sh" >>/var/spool/cron/crontabs/root
    fi
fi

# $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_homeassistant_clean.sh
if [ "$HOMEASSISTANT_ENABLE" == "yes" ]; then
    if [ "$HOMEASSISTANT_BOOT" == "yes" ]; then
        $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_homeassistant.sh &
    fi
    if [ "$HOMEASSISTANT_CRON" == "yes" ]; then
        echo "$HOMEASSISTANT_CRONTAB  $YI_HACK_PREFIX/script/mqtt_advertise/mqtt_adv_homeassistant.sh" >>/var/spool/cron/crontabs/root
    fi
fi
