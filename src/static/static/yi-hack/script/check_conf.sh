#!/bin/sh

SYSTEM_CONF_FILE="/tmp/sd/yi-hack/etc/system.conf"
CAMERA_CONF_FILE="/tmp/sd/yi-hack/etc/camera.conf"
MQTTV4_CONF_FILE="/tmp/sd/yi-hack/etc/mqttv4.conf"

PARMS1="
HTTPD=yes
TELNETD=yes
SSHD=yes
FTPD=yes
BUSYBOX_FTPD=no
DISABLE_CLOUD=no
REC_WITHOUT_CLOUD=no
MQTT=no
RTSP=yes
RTSP_ALT=no
RTSP_STREAM=high
RTSP_AUDIO=yes
SPEAKER_AUDIO=yes
SNAPSHOT=yes
SNAPSHOT_VIDEO=no
SNAPSHOT_LOW=no
TIMELAPSE=no
TIMELAPSE_FTP=no
TIMELAPSE_FTP_SAME_NAME=no
TIMELAPSE_DT=60
TIMELAPSE_VDT=
ONVIF=yes
ONVIF_WSDD=yes
ONVIF_PROFILE=high
ONVIF_NETIF=wlan0
ONVIF_WM_SNAPSHOT=yes
TIME_OSD=no
NTPD=yes
NTP_SERVER=pool.ntp.org
PROXYCHAINSNG=no
SWAP_FILE=yes
SWAP_SWAPPINESS=15
RTSP_PORT=554
HTTPD_PORT=80
USERNAME=
PASSWORD=
TIMEZONE=
EVENTS_TIME=autodetect
FREE_SPACE=0
FTP_UPLOAD=no
FTP_HOST=
FTP_DIR=
FTP_DIR_TREE=no
FTP_USERNAME=
FTP_PASSWORD=
FTP_FILE_DELETE_AFTER_UPLOAD=yes
SSH_PASSWORD=
CRONTAB=
DEBUG_LOG=no"

PARMS2="
SWITCH_ON=yes
SAVE_VIDEO_ON_MOTION=yes
MOTION_DETECTION=no
SENSITIVITY=low
AI_HUMAN_DETECTION=no
AI_VEHICLE_DETECTION=no
AI_ANIMAL_DETECTION=no
FACE_DETECTION=no
MOTION_TRACKING=no
SOUND_DETECTION=no
SOUND_SENSITIVITY=80
LED=no
ROTATE=no
IR=yes
CRUISE=no"

PARMS3="
MQTT_IP=0.0.0.0
MQTT_PORT=1883
MQTT_CLIENT_ID=yi-cam
MQTT_USER=
MQTT_PASSWORD=
MQTT_PREFIX=yicam
TOPIC_BIRTH_WILL=status
TOPIC_MOTION=motion_detection
TOPIC_MOTION_IMAGE=motion_detection_image
MOTION_IMAGE_DELAY=0.5
TOPIC_MOTION_FILES=motion_files
TOPIC_SOUND_DETECTION=sound_detection
BIRTH_MSG=online
WILL_MSG=offline
MOTION_START_MSG=motion_start
MOTION_STOP_MSG=motion_stop
AI_HUMAN_DETECTION_MSG=human
AI_VEHICLE_DETECTION_MSG=vehicle
AI_ANIMAL_DETECTION_MSG=animal
BABY_CRYING_MSG=crying
SOUND_DETECTION_MSG=sound_detected
MQTT_KEEPALIVE=120
MQTT_QOS=1
MQTT_RETAIN_BIRTH_WILL=1
MQTT_RETAIN_MOTION=0
MQTT_RETAIN_MOTION_IMAGE=0
MQTT_RETAIN_MOTION_FILES=0
MQTT_RETAIN_SOUND_DETECTION=0"

if [ ! -f $SYSTEM_CONF_FILE ]; then
    touch $SYSTEM_CONF_FILE
fi
for i in $PARMS1
do
    if [ ! -z "$i" ]; then
        PAR=$(echo "$i" | cut -d= -f1)
        MATCH=$(cat $SYSTEM_CONF_FILE | grep ^$PAR=)
        if [ -z "$MATCH" ]; then
            echo "$i" >> $SYSTEM_CONF_FILE
        fi
    fi
done

if [ ! -f $CAMERA_CONF_FILE ]; then
    touch $CAMERA_CONF_FILE
fi
for i in $PARMS2
do
    if [ ! -z "$i" ]; then
        PAR=$(echo "$i" | cut -d= -f1)
        MATCH=$(cat $CAMERA_CONF_FILE | grep ^$PAR=)
        if [ -z "$MATCH" ]; then
            echo "$i" >> $CAMERA_CONF_FILE
        fi
    fi
done

if [ ! -f $MQTTV4_CONF_FILE ]; then
    touch $MQTTV4_CONF_FILE
fi
for i in $PARMS3
do
    if [ ! -z "$i" ]; then
        PAR=$(echo "$i" | cut -d= -f1)
        MATCH=$(cat $MQTTV4_CONF_FILE | grep ^$PAR=)
        if [ -z "$MATCH" ]; then
            echo "$i" >> $MQTTV4_CONF_FILE
        fi
    fi
done
