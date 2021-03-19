#!/bin/sh

### wifi 8188 ###
if [ -f /home/base/wifi/8188fu.ko ];then
    insmod /home/base/wifi/8188fu.ko
elif [ -f /home/base/wifi/8189fs.ko ];then
    insmod /home/base/wifi/8188fs.ko
elif [ -f /backup/ko/8188fu.ko ];then
    insmod /backup/ko/8188fu.ko
elif [ -f /backup/ko/8189fs.ko ];then
    insmod /backup/ko/8189fs.ko
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
ifconfig wlan0 up
ethmac=d2:`ifconfig wlan0 |grep HWaddr|cut -d' ' -f10|cut -d: -f2-`
ifconfig eth0 hw ether $ethmac
ifconfig eth0 up

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

mount --bind /backup/tools/wifidhcp.sh /home/app/script/wifidhcp.sh

./dispatch &
#sleep 2
#./rmm &
#sleep 2
#./mp4record &
#./cloud &
#./p2p_tnp &
#./oss &
#./watch_process &

chmod 755 /tmp/sd/yi-hack/script/system.sh
sh /tmp/sd/yi-hack/script/system.sh &
