#!/bin/sh

# protect against running this script twice and starting up a bunch of duplicate processes
if [ -f /tmp/init_started ]; then
    exit
fi

touch /tmp/init_started

### wifi 8188 ###
if [ "${enable_4g}" = "y" ];then
    echo "4g is running...."
else
    if [ -f /home/base/wifi/8188fu.ko ];then
        insmod /home/base/wifi/8188fu.ko
    elif [ -f /home/base/wifi/8189fs.ko ];then
        insmod /home/base/wifi/8188fs.ko
    elif [ -f /backup/ko/8188fu.ko ];then
        insmod /backup/ko/8188fu.ko
    elif [ -f /backup/ko/8189fs.ko ];then
        insmod /backup/ko/8189fs.ko
    elif [ -f /backup/ko/atbm603x_wifi_usb.ko ];then
        insmod /backup/ko/atbm603x_wifi_usb.ko
    elif [ -f /backup/ko/ssv6x5x.ko ];then
        if [ -f /home/base/firmware/ssv6x5x/ssv6x5x-wifi.cfg ];then
            insmod /backup/ko/ssv6x5x.ko stacfgpath="/home/base/firmware/ssv6x5x/ssv6x5x-wifi.cfg"
        else
            echo "not found ssv6x5x-wifi.cfg"
        fi
    fi
fi

echo "--------------------------insmod sensor--------------------------"
insmod /home/base/ko/videobuf2-core.ko
insmod /home/base/ko/videobuf2-memops.ko
insmod /home/base/ko/videobuf2-dma-contig.ko
insmod /home/base/ko/videobuf2-v4l2.ko
insmod /home/base/ko/vin_io.ko
insmod /home/base/ko/cam_sensor.ko
insmod /home/base/ko/vin_v4l2.ko

if [ -f /home/base/ko/icplus.ko ];then
    insmod /home/base/ko/icplus.ko
elif [ -f /backup/ko/icplus.ko ];then
    insmod /backup/ko/icplus.ko
fi

if [ -f /home/base/ko/sunxi_gpadc.ko ];then
    insmod /home/base/ko/sunxi_gpadc.ko
fi

#/home/app/script/factory_test.sh

#echo "MTK 7601" > /tmp/MTK
#echo /tmp/sd/core.exe[%e].pid[%p].sig[%s] > /proc/sys/kernel/core_pattern

sleep 1
ifconfig lo up

ifconfig ${NETWORK_IFACE} up
ethmac=d2:`ifconfig ${NETWORK_IFACE} |grep HWaddr|cut -d' ' -f10|cut -d: -f2-`
#if [ "${enable_4g}" = "y" ];then
#    ifconfig usb0 up
#    ethmac=d2:`ifconfig usb0 |grep HWaddr|cut -d' ' -f10|cut -d: -f2-`
#else
#    ifconfig wlan0 up
#    ethmac=d2:`ifconfig wlan0 |grep HWaddr|cut -d' ' -f10|cut -d: -f2-`
#fi

ifconfig eth0 hw ether $ethmac
ifconfig eth0 up

HOMEVER=$(cat /home/homever)
HV=${HOMEVER:0:2}

if [ "$HV" == "11" ] || [ "$HV" == "12" ]; then
    ln -s /home/model/BodyVehicleAnimal3.model /tmp/BodyVehicleAnimal3.model
fi

echo "============================================= home low_half_init.sh... ========================================="
echo "============================================= begin to start app... ========================================="
cd /home/app
if [ -f /home/app/property ];then
    ./property &
fi
#./log_server &

if [ -f "/tmp/sd/Factory/factory_test.sh" ]; then
	/tmp/sd/Factory/config.sh
	exit
fi

#export LD_LIBRARY_PATH=/tmp/:$LD_LIBRARY_PATH
#export PATH=/home/app:/home/app/script:$PATH
export LD_LIBRARY_PATH=/home/app/locallib:/home/app/script:$LD_LIBRARY_PATH:/tmp
echo $LD_LIBRARY_PATH

if [ "$HV" == "11" ] || [ "$HV" == "12" ]; then
    if [ -f "/tmp/sd/log_tools.tar.gz" ];then
        echo "run log_tools start."
        if [ ! -d /tmp/sd/log_tools ];then
            cd /tmp/sd
            mkdir log_tools
        fi
        cd /tmp/sd
        tar -zxvf log_tools.tar.gz -C /tmp/sd/log_tools
        chmod +x /tmp/sd/log_tools/run_log_app.sh
        source /tmp/sd/log_tools/run_log_app.sh
        cd -
        echo "run log_tools end."
        #exit
    fi
fi

mount --bind /tmp/sd/yi-hack/script/wifidhcp.sh /home/app/script/wifidhcp.sh
mount --bind /tmp/sd/yi-hack/script/wifidhcp.sh /backup/tools/wifidhcp.sh
mount --bind /tmp/sd/yi-hack/script/ethdhcp.sh /home/app/script/ethdhcp.sh
mount --bind /tmp/sd/yi-hack/script/ethdhcp.sh /backup/tools/ethdhcp.sh

if [ "$(grep -w CUSTOM_WATERMARK /tmp/sd/yi-hack/etc/system.conf | cut -d= -f2)" = "yes" ]; then
    mount --bind /tmp/sd/yi-hack/etc/watermark/blank.bmp /home/app/main_kami.bmp
    mount --bind /tmp/sd/yi-hack/etc/watermark/blank.bmp /home/app/sub_kami.bmp
fi
LD_PRELOAD=/tmp/sd/yi-hack/lib/ipc_multiplex.so ./dispatch &
#sleep 2
#./rmm &
#sleep 2
#./mp4record &
#./cloud &
#./p2p_tnp &
#./oss &
#./rtmp &
#./watch_process &

chmod 777 /tmp/sd/debug.sh
if [ -f "/tmp/sd/debug.sh" ]; then
    echo "calling /tmp/sd/debug.sh"
    sh /tmp/sd/debug.sh &
fi

chmod 755 /tmp/sd/yi-hack/script/system.sh
sh /tmp/sd/yi-hack/script/system.sh &
