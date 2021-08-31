#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"
YI_HACK_UPGRADE_PATH="/tmp/sd/.fw_upgrade"

YI_HACK_VER=$(cat /tmp/sd/yi-hack/version)
MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}

start_buffer()
{
    # Trick to start circular buffer filling
    ./cloud &
    IDX=`hexdump -n 16 /dev/shm/fshare_frame_buf | awk 'NR==1{print $8}'`
    N=0
    while [ "$IDX" -eq "0000" ] && [ $N -lt 60 ]; do
        IDX=`hexdump -n 16 /dev/shm/fshare_frame_buf | awk 'NR==1{print $8}'`
        N=$(($N+1))
        sleep 0.2
    done
    killall cloud
    ipc_cmd -x
}

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

ulimit -s 1024

# Remove core files, if any
rm -f $YI_HACK_PREFIX/bin/core
rm -f $YI_HACK_PREFIX/www/cgi-bin/core

touch /tmp/httpd.conf

if [ -f $YI_HACK_UPGRADE_PATH/yi-hack/fw_upgrade_in_progress ]; then
    echo "#!/bin/sh" > /tmp/fw_upgrade_2p.sh
    echo "# Complete fw upgrade and restore configuration" >> /tmp/fw_upgrade_2p.sh
    echo "sleep 1" >> /tmp/fw_upgrade_2p.sh
    echo "cd $YI_HACK_UPGRADE_PATH" >> /tmp/fw_upgrade_2p.sh
    echo "cp -rf * .." >> /tmp/fw_upgrade_2p.sh
    echo "cd .." >> /tmp/fw_upgrade_2p.sh
    echo "rm -rf $YI_HACK_UPGRADE_PATH" >> /tmp/fw_upgrade_2p.sh
    echo "rm $YI_HACK_PREFIX/fw_upgrade_in_progress" >> /tmp/fw_upgrade_2p.sh
    echo "sync" >> /tmp/fw_upgrade_2p.sh
    echo "sync" >> /tmp/fw_upgrade_2p.sh
    echo "sync" >> /tmp/fw_upgrade_2p.sh
    echo "reboot" >> /tmp/fw_upgrade_2p.sh
    sh /tmp/fw_upgrade_2p.sh
    exit
fi

$YI_HACK_PREFIX/script/check_conf.sh

hostname -F $YI_HACK_PREFIX/etc/hostname
export TZ=$(get_config TIMEZONE)

if [[ $(get_config SWAP_FILE) == "yes" ]] ; then
    SD_PRESENT=$(mount | grep mmc | grep -c ^)
    if [[ $SD_PRESENT -eq 1 ]]; then
        if [[ -f /tmp/sd/swapfile ]]; then
            swapon /tmp/sd/swapfile
        else
            dd if=/dev/zero of=/tmp/sd/swapfile bs=1M count=64
            chmod 0600 /tmp/sd/swapfile
            mkswap /tmp/sd/swapfile
            swapon /tmp/sd/swapfile
        fi
    fi
fi

if [[ x$(get_config USERNAME) != "x" ]] ; then
    USERNAME=$(get_config USERNAME)
    PASSWORD=$(get_config PASSWORD)
    ONVIF_USERPWD="user=$USERNAME\npassword=$PASSWORD"
    echo "/:$USERNAME:$PASSWORD" > /tmp/httpd.conf
fi

if [[ x$(get_config SSH_PASSWORD) != "x" ]] ; then
    SSH_PASSWORD=$(get_config SSH_PASSWORD)
    PASSWORD_MD5="$(echo "${SSH_PASSWORD}" | mkpasswd --method=MD5 --stdin)"
    cp -f "/etc/passwd" "/tmp/sd/yi-hack/etc/passwd"
    sed -i 's|^root::|root:'${PASSWORD_MD5}':|g' "/tmp/sd/yi-hack/etc/passwd"
    sed -i 's|/root|/tmp/sd/yi-hack|g' "/tmp/sd/yi-hack/etc/passwd"
    mount --bind "/tmp/sd/yi-hack/etc/passwd" "/etc/passwd"
    cp -f "/etc/shadow" "/tmp/sd/yi-hack/etc/shadow"
    sed -i 's|^root::|root:'${PASSWORD_MD5}':|g' "/tmp/sd/yi-hack/etc/shadow"
    mount --bind "/tmp/sd/yi-hack/etc/shadow" "/etc/shadow"
fi

case $(get_config RTSP_PORT) in
    ''|*[!0-9]*) RTSP_PORT=554 ;;
    *) RTSP_PORT=$(get_config RTSP_PORT) ;;
esac
case $(get_config ONVIF_PORT) in
    ''|*[!0-9]*) ONVIF_PORT=80 ;;
    *) ONVIF_PORT=$(get_config ONVIF_PORT) ;;
esac
case $(get_config HTTPD_PORT) in
    ''|*[!0-9]*) HTTPD_PORT=8080 ;;
    *) HTTPD_PORT=$(get_config HTTPD_PORT) ;;
esac

# todo
#if [ ! -f $YI_PREFIX/cloudAPI_real ]; then
#    mv $YI_PREFIX/cloudAPI $YI_PREFIX/cloudAPI_real
#    cp $YI_HACK_PREFIX/script/cloudAPI $YI_PREFIX/
#fi

if [[ $(get_config DISABLE_CLOUD) == "no" ]] ; then
    (
        if [ $(get_config RTSP_AUDIO) != "no" ]; then
            touch /tmp/audio_fifo.requested
        fi
        if [ $(get_config SPEAKER_AUDIO) != "no" ]; then
            touch /tmp/audio_in_fifo.requested
        fi
        cd /home/app
        LD_LIBRARY_PATH="/tmp/sd/yi-hack/lib:/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb" ./rmm &
        sleep 6
        dd if=/tmp/audio_fifo of=/dev/null bs=1 count=8192
#        dd if=/dev/zero of=/tmp/audio_in_fifo bs=1 count=1024
        ./mp4record &
        ./cloud &
        ./p2p_tnp &
        ./oss &
        if [ -f ./oss_fast ]; then
            ./oss_fast &
        fi
        if [ -f ./oss_lapse ]; then
            ./oss_lapse &
        fi
        ./rtmp &
        ./watch_process &
    )
else
    (
        if [ $(get_config RTSP_AUDIO) != "no" ]; then
            touch /tmp/audio_fifo.requested
        fi
        if [ $(get_config SPEAKER_AUDIO) != "no" ]; then
            touch /tmp/audio_in_fifo.requested
        fi
        cd /home/app
        LD_LIBRARY_PATH="/tmp/sd/yi-hack/lib:/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb" ./rmm &
        sleep 6
        dd if=/tmp/audio_fifo of=/dev/null bs=1 count=8192
#        dd if=/dev/zero of=/tmp/audio_in_fifo bs=1 count=1024
        # Trick to start circular buffer filling
        start_buffer
        if [[ $(get_config REC_WITHOUT_CLOUD) == "yes" ]] ; then
            ./mp4record &
        fi

        mkdir /tmp/etc
        cp -R /etc/* /tmp/etc
        mount --bind /tmp/etc /etc
        echo "127.0.0.1    api.eu.xiaoyi.com" >> /etc/hosts
    )
fi

if [[ $(get_config HTTPD) == "yes" ]] ; then
    mkdir -p /tmp/sd/record
    mkdir -p /tmp/sd/yi-hack/www/record
    mount --bind /tmp/sd/record /tmp/sd/yi-hack/www/record
    httpd -p $HTTPD_PORT -h $YI_HACK_PREFIX/www/ -c /tmp/httpd.conf
fi

if [[ $(get_config TELNETD) == "no" ]] ; then
    killall telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
    if [[ $(get_config BUSYBOX_FTPD) == "yes" ]] ; then
        tcpsvd -vE 0.0.0.0 21 ftpd -w &
    else
        pure-ftpd -B
    fi
fi

if [[ $(get_config SSHD) == "yes" ]] ; then
    mkdir -p $YI_HACK_PREFIX/etc/dropbear
    if [ ! -f $YI_HACK_PREFIX/etc/dropbear/dropbear_ecdsa_host_key ]; then
        dropbearkey -t ecdsa -f /tmp/dropbear_ecdsa_host_key
        mv /tmp/dropbear_ecdsa_host_key $YI_HACK_PREFIX/etc/dropbear/
    fi
    # Restore keys
#    mkdir -p /etc/dropbear
#    cp -f $SONOFF_HACK_PREFIX/etc/dropbear/* /etc/dropbear/
    chmod 0600 $YI_HACK_PREFIX/etc/dropbear/*
    dropbear -R -B  -p 0.0.0.0:22
fi

if [[ $(get_config NTPD) == "yes" ]] ; then
    # Wait until all the other processes have been initialized
    sleep 5 && ntpd -p $(get_config NTP_SERVER) &
fi

ipc_multiplexer &
sleep 1
if [[ $(get_config MQTT) == "yes" ]] ; then
    mqttv4 &
    mqtt-config &
fi

sleep 5

if [[ $RTSP_PORT != "554" ]] ; then
    D_RTSP_PORT=:$RTSP_PORT
fi

if [[ $HTTPD_PORT != "80" ]] ; then
    D_HTTPD_PORT=:$HTTPD_PORT
fi

if [[ $ONVIF_PORT != "80" ]] ; then
    D_ONVIF_PORT=:$ONVIF_PORT
fi

if [[ $(get_config ONVIF_WM_SNAPSHOT) == "yes" ]] ; then
    WATERMARK="&watermark=yes"
fi

if [[ $(get_config SNAPSHOT) == "no" ]] ; then
    touch /tmp/snapshot.disabled
fi

if [[ $(get_config RTSP) == "yes" ]] ; then
    RTSP_AUDIO_COMPRESSION=$(get_config RTSP_AUDIO)
    if [[ "$RTSP_AUDIO_COMPRESSION" == "none" ]] ; then
        RTSP_AUDIO_COMPRESSION="no"
    fi

    RRTSP_MODEL=$MODEL_SUFFIX RRTSP_RES=$(get_config RTSP_STREAM) RRTSP_AUDIO=$RTSP_AUDIO_COMPRESSION RRTSP_PORT=$RTSP_PORT RRTSP_USER=$USERNAME RRTSP_PWD=$PASSWORD rRTSPServer &
    if [[ $(get_config RTSP_STREAM) == "low" ]]; then
        ONVIF_PROFILE_1="name=Profile_1\nwidth=640\nheight=360\nurl=rtsp://%s$D_RTSP_PORT/ch0_1.h264\nsnapurl=http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=low$WATERMARK\ntype=H264"
    fi
    if [[ $(get_config RTSP_STREAM) == "high" ]]; then
        ONVIF_PROFILE_0="name=Profile_0\nwidth=1920\nheight=1080\nurl=rtsp://%s$D_RTSP_PORT/ch0_0.h264\nsnapurl=http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=high$WATERMARK\ntype=H264"
    fi
    if [[ $(get_config RTSP_STREAM) == "both" ]]; then
        if [[ $(get_config ONVIF_PROFILE) == "low" ]] || [[ $(get_config ONVIF_PROFILE) == "both" ]] ; then
            ONVIF_PROFILE_1="name=Profile_1\nwidth=640\nheight=360\nurl=rtsp://%s$D_RTSP_PORT/ch0_1.h264\nsnapurl=http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=low$WATERMARK\ntype=H264"
        fi
        if [[ $(get_config ONVIF_PROFILE) == "high" ]] || [[ $(get_config ONVIF_PROFILE) == "both" ]] ; then
            ONVIF_PROFILE_0="name=Profile_0\nwidth=1920\nheight=1080\nurl=rtsp://%s$D_RTSP_PORT/ch0_0.h264\nsnapurl=http://%s$D_HTTPD_PORT/cgi-bin/snapshot.sh?res=high$WATERMARK\ntype=H264"
        fi
    fi
    $YI_HACK_PREFIX/script/wd_rtsp.sh &
fi

if [ "$MODEL_SUFFIX" == "h60ga" ]; then
    SERIAL_NUMBER=$(dd bs=1 count=20 skip=784 if=/tmp/mmap.info 2>/dev/null | tr '\0' '0' | cut -c1-20)
    HW_ID=$(dd bs=1 count=4 skip=784 if=/tmp/mmap.info 2>/dev/null | tr '\0' '0' | cut -c1-4)
else
    SERIAL_NUMBER=$(dd bs=1 count=20 skip=656 if=/tmp/mmap.info 2>/dev/null | tr '\0' '0' | cut -c1-20)
    HW_ID=$(dd bs=1 count=4 skip=656 if=/tmp/mmap.info 2>/dev/null | tr '\0' '0' | cut -c1-4)
fi
PTZ_UD_INV="-M"
if [ "$MODEL_SUFFIX" == "h60ga" ] || [ "$MODEL_SUFFIX" == "h51ga" ]; then
    PTZ_UD_INV="-m"
fi

if [[ $(get_config ONVIF) == "yes" ]] ; then
    if [[ $(get_config ONVIF_NETIF) == "wlan0" ]] ; then
        ONVIF_NETIF="wlan0"
    else
        ONVIF_NETIF="eth0"
    fi

    ONVIF_SRVD_CONF="/tmp/onvif_srvd.conf"

    echo "pid_file=/var/run/onvif_srvd.pid" > $ONVIF_SRVD_CONF
    echo "model=Yi Hack" >> $ONVIF_SRVD_CONF
    echo "manufacturer=Yi" >> $ONVIF_SRVD_CONF
    echo "firmware_ver=$YI_HACK_VER" >> $ONVIF_SRVD_CONF
    echo "hardware_id=$HW_ID" >> $ONVIF_SRVD_CONF
    echo "serial_num=$SERIAL_NUMBER" >> $ONVIF_SRVD_CONF
    echo "ifs=$ONVIF_NETIF" >> $ONVIF_SRVD_CONF
    echo "port=$ONVIF_PORT" >> $ONVIF_SRVD_CONF
    echo "scope=onvif://www.onvif.org/Profile/S" >> $ONVIF_SRVD_CONF
    echo "" >> $ONVIF_SRVD_CONF
    if [ ! -z $ONVIF_PROFILE_0 ]; then
        echo "#Profile 0" >> $ONVIF_SRVD_CONF
        echo -e $ONVIF_PROFILE_0 >> $ONVIF_SRVD_CONF
        echo "" >> $ONVIF_SRVD_CONF
    fi
    if [ ! -z $ONVIF_PROFILE_1 ]; then
        echo "#Profile 1" >> $ONVIF_SRVD_CONF
        echo -e $ONVIF_PROFILE_1 >> $ONVIF_SRVD_CONF
        echo "" >> $ONVIF_SRVD_CONF
    fi
    if [ ! -z $ONVIF_USERPWD ]; then
        echo -e $ONVIF_USERPWD >> $ONVIF_SRVD_CONF
        echo "" >> $ONVIF_SRVD_CONF
    fi

    if [[ $MODEL_SUFFIX == "r30gb" ]] || [[ $MODEL_SUFFIX == "r40ga" ]] || [[ $MODEL_SUFFIX == "h51ga" ]] || [[ $MODEL_SUFFIX == "h52ga" ]] || [[ $MODEL_SUFFIX == "h60ga" ]] || [[ $MODEL_SUFFIX == "q321br_lsx" ]] || [[ $MODEL_SUFFIX == "qg311r" ]] ; then
        echo "#PTZ" >> $ONVIF_SRVD_CONF
        echo "ptz=1" >> $ONVIF_SRVD_CONF
        echo "move_left=/tmp/sd/yi-hack/bin/ipc_cmd -M left" >> $ONVIF_SRVD_CONF
        echo "move_right=/tmp/sd/yi-hack/bin/ipc_cmd -M right" >> $ONVIF_SRVD_CONF
        echo "move_up=/tmp/sd/yi-hack/bin/ipc_cmd $PTZ_UD_INV up" >> $ONVIF_SRVD_CONF
        echo "move_down=/tmp/sd/yi-hack/bin/ipc_cmd $PTZ_UD_INV down" >> $ONVIF_SRVD_CONF
        echo "move_stop=/tmp/sd/yi-hack/bin/ipc_cmd -M stop" >> $ONVIF_SRVD_CONF
        echo "move_preset=/tmp/sd/yi-hack/bin/ipc_cmd -p %t" >> $ONVIF_SRVD_CONF
    fi

    onvif_srvd --conf_file $ONVIF_SRVD_CONF

    if [[ $(get_config ONVIF_WSDD) == "yes" ]] ; then
        wsdd --pid_file /var/run/wsdd.pid --if_name $ONVIF_NETIF --type tdn:NetworkVideoTransmitter --xaddr "http://%s$D_ONVIF_PORT" --scope "onvif://www.onvif.org/name/Unknown onvif://www.onvif.org/Profile/Streaming"
    fi
fi

if [ ! -f /tmp/snapshot.disabled ] ; then
    framefinder $MODEL_SUFFIX &
fi

# Add crontab
CRONTAB=$(get_config CRONTAB)
FREE_SPACE=$(get_config FREE_SPACE)
mkdir -p /var/spool/cron/crontabs/
if [ ! -z "$CRONTAB" ]; then
    echo -e "$CRONTAB" > /var/spool/cron/crontabs/root
fi
if [ "$FREE_SPACE" != "0" ]; then
    echo "0 * * * * /tmp/sd/yi-hack/script/clean_records.sh $FREE_SPACE" >> /var/spool/cron/crontabs/root
fi
$YI_HACK_PREFIX/usr/sbin/crond -c /var/spool/cron/crontabs/

# Add MQTT Advertise
if [ -f "$YI_HACK_PREFIX/script/mqtt_advertise/startup.sh" ]; then
    $YI_HACK_PREFIX/script/mqtt_advertise/startup.sh
fi

# Remove log files written to SD on boot containing the WiFi password
#rm -f "/tmp/sd/log/log_first_login.tar.gz"
#rm -f "/tmp/sd/log/log_login.tar.gz"
#rm -f "/tmp/sd/log/log_p2p_clr.tar.gz"
#rm -f "/tmp/sd/log/log_wifi_connected.tar.gz"

if [[ $(get_config FTP_UPLOAD) == "yes" ]] ; then
    /tmp/sd/yi-hack/script/ftppush.sh start &
fi

if [ -f "/tmp/sd/yi-hack/startup.sh" ]; then
    /tmp/sd/yi-hack/startup.sh
fi
