#!/bin/sh
echo "--------------------------home app init.sh--------------------------"
SUFFIX=r40ga
enable_4g=

PATH="/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin"
LD_LIBRARY_PATH="/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib"
export PATH
export LD_LIBRARY_PATH

NETWORK_IFACE=

if [ "${enable_4g}" = "y" ];then
    NETWORK_IFACE=eth1
else
    NETWORK_IFACE=wlan0
fi

echo "NETWORK_IFACE=${NETWORK_IFACE}"

echo "product_name=${SUFFIX}"


if [ "${SUFFIX}" = "q705ssv" ];then
echo 197 > /sys/class/gpio/export 
echo out > /sys/class/gpio/gpio197/direction  
echo 0 > /sys/class/gpio/gpio197/value 
sleep 1
echo 1 > /sys/class/gpio/gpio197/value
elif [ "${SUFFIX}" = "h50ga" ] || [ "${SUFFIX}" = "y29ga" ]|| [ "${SUFFIX}" = "y21ga" ]|| [ "${SUFFIX}" = "y28ga" ]|| [ "${SUFFIX}" = "h51ga" ]|| [ "${SUFFIX}" = "h60ga" ]|| [ "${SUFFIX}" = "h30ga" ]|| [ "${SUFFIX}" = "h52ga" ]|| [ "${SUFFIX}" = "h53ga" ]|| [ "${SUFFIX}" = "y19ga" ];then
echo "need reset gpio198"

elif [ "${SUFFIX}" = "b092qp" ];then
echo 195 > /sys/class/gpio/export 
echo out > /sys/class/gpio/gpio195/direction  
echo 0 > /sys/class/gpio/gpio195/value 
sleep 1
echo 1 > /sys/class/gpio/gpio195/value
elif [ "${SUFFIX}" = "q321br" ] ||  [ "${SUFFIX}" = "q321br_lsx" ] ||  [ "${SUFFIX}" = "q321br_jd" ] ||  [ "${SUFFIX}" = "q321br_aldz" ] || \
	 [ "${SUFFIX}" = "q321br_aldz_3m" ] || [ "${SUFFIX}" = "q321br_chw" ] || [ "${SUFFIX}" = "q321ssv" ] || [ "${SUFFIX}" = "q321ssv_3m" ] ||  [ "${SUFFIX}" = "y041qp" ];then
echo 145 > /sys/class/gpio/export 
echo out > /sys/class/gpio/gpio145/direction  
echo 0 > /sys/class/gpio/gpio145/value 
sleep 1
echo 1 > /sys/class/gpio/gpio145/value
echo 145 > /sys/class/gpio/unexport 
fi



ulimit -c unlimited
ulimit -s 1024
#echo /tmp/sd/core.exe[%e].pid[%p].sig[%s] > /proc/sys/kernel/core_pattern

echo 100 > /proc/sys/vm/dirty_writeback_centisecs
echo 50 > /proc/sys/vm/dirty_expire_centisecs

if [ "${pix}" = "2m" ];then
echo 2048 > /proc/sys/vm/extra_free_kbytes
fi

echo 400 > /proc/sys/vm/vfs_cache_pressure
echo 0 > /proc/sys/vm/drop_caches
echo 256 > /proc/sys/vm/min_free_kbytes


sysctl -w vm.dirty_background_ratio=2
sysctl -w vm.dirty_expire_centisecs=500
sysctl -w vm.dirty_ratio=2
sysctl -w vm.dirty_writeback_centisecs=100

sysctl -w fs.mqueue.msg_max=256
mkdir /dev/mqueue
mount -t mqueue none /dev/mqueue

mount tmpfs /tmp -t tmpfs -o size=32m
mkdir /tmp/sd
mkdir /dev/shm
mkdir /tmp/var
mkdir /tmp/var/run
mkdir /tmp/run


checkdisk
rm -fr /tmp/sd/FSCK*.REC
umount -l /tmp/sd
mount -t vfat /dev/mmcblk0p1 /tmp/sd

#----insmod hardware cyption------

# b091qp ：gpio统一版 comm_cpld驱动早于wifi驱动加载以便控制wifi使能
if [ "${SUFFIX}" = "b091qp" ];then

	# if factory mode write hw and gpio before loading the cpld driver
	if [ -f "/tmp/sd/Factory/factory_test.sh" ]; then
		/tmp/sd/Factory/write_hw_gpio.sh
	fi

	# read gpio and hw & insmod cpld
	GPIO=$(/home/app/read_gpio | grep gpio= | awk 'NR==1{print $5}' |sed 's/gpio=//' |sed 's/,//')
	if [ "${#GPIO}" -ne 128 ];then
		GPIO=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000
	fi
	echo $GPIO
	HW=$(/home/app/read_hw | grep hw_ver= | awk 'NR==1{print $5}' |sed 's/hw_ver=//' |sed 's/,//')
	if [ "${#HW}" -ne 64 ];then
		HW=0000000000000000000000000000000000000000000000000000000000000000
	fi
	echo $HW
	insmod /home/app/localko/cpld_periph.ko hw=$HW gpio=$GPIO
	sleep 1

	#拷贝wifi驱动 未过产测调试用
	if [ -d /tmp/sd/wifi_drive/ ];then

		WIFI_TYPE=`echo ${HW:37:1}`
		echo "WIFI_TYPE:$WIFI_TYPE"
		if [ "${WIFI_TYPE}" = "1" ];then
			if [ ! -f /backup/ko/ssv6x5x.ko ];then
			cp /tmp/sd/wifi_drive/ssv6x5x.ko  /backup/ko/
			fi
		elif [ "${WIFI_TYPE}" = "2" ];then
			if [ ! -f /backup/ko/8188fu.ko ];then
			cp  /tmp/sd/wifi_drive/8188fu.ko /backup/ko/
			fi
		elif [ "${WIFI_TYPE}" = "3" ];then
			if [ ! -f /backup/ko/8189fs.ko ];then
			cp  /tmp/sd/8189fs.ko  /backup/ko/
			fi
		elif [ "${WIFI_TYPE}" = "4" ];then
			if [ ! -f /backup/ko/atbm603x_wifi_usb.ko ];then
			cp  /tmp/sd/wifi_drive/atbm603x_wifi_usb.ko  /backup/ko/
			fi
		else
		echo "no config hw"
		fi
		sync
	fi

	#sensor 
	SENSOR_TYPE=`echo ${HW:38:2}`
	echo "SENSOR_TYPE:$SENSOR_TYPE"
	if [ "${SENSOR_TYPE}" = "01" ];then
		SENSOR_KO_NAME=f37_mipi_321_705_1217.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
	elif [ "${SENSOR_TYPE}" = "02" ];then
		SENSOR_KO_NAME=f37_mipi_321_705.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
	elif [ "${SENSOR_TYPE}" = "03" ];then
		SENSOR_KO_NAME=f37_mipi_b011_h011.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
	elif [ "${SENSOR_TYPE}" = "04" ];then
		SENSOR_KO_NAME=f37_mipi.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
	elif [ "${SENSOR_TYPE}" = "11" ];then
		SENSOR_KO_NAME=h63_mipi.ko
		SENSOR_DRIVE_NAME=h63_mipi
		SENSOR_ADDR=0x80
	elif [ "${SENSOR_TYPE}" = "21" ];then
		SENSOR_KO_NAME=C2390A_mipi.ko
		SENSOR_DRIVE_NAME=C2390A_mipi
		SENSOR_ADDR=0x6C	
	elif [ "${SENSOR_TYPE}" = "31" ];then
		SENSOR_KO_NAME=gc2053_mipi.ko
		SENSOR_DRIVE_NAME=gc2053_mipi
		SENSOR_ADDR=0x6e
	elif [ "${SENSOR_TYPE}" = "35" ];then
		SENSOR_KO_NAME=gc1054_mipi.ko
		SENSOR_DRIVE_NAME=gc1054_mipi
		SENSOR_ADDR=0x42
	elif [ "${SENSOR_TYPE}" = "36" ];then
		SENSOR_KO_NAME=gc2093_mipi.ko
		SENSOR_DRIVE_NAME=gc2093_mipi
		SENSOR_ADDR=0x6e		
	elif [ "${SENSOR_TYPE}" = "61" ];then
		SENSOR_KO_NAME=sc2231_mipi.ko
		SENSOR_DRIVE_NAME=sc2231_mipi
		SENSOR_ADDR=0x60	
	elif [ "${SENSOR_TYPE}" = "63" ];then
		SENSOR_KO_NAME=sc2332_mipi.ko
		SENSOR_DRIVE_NAME=sc2332_mipi
		SENSOR_ADDR=0x60
	elif [ "${SENSOR_TYPE}" = "64" ];then
		SENSOR_KO_NAME=sc2335_mipi.ko
		SENSOR_DRIVE_NAME=sc2335_mipi
		SENSOR_ADDR=0x60	
	elif [ "${SENSOR_TYPE}" = "65" ];then
		SENSOR_KO_NAME=sc3335_mipi.ko
		SENSOR_DRIVE_NAME=sc3335_mipi
		SENSOR_ADDR=0x60
	elif [ "${SENSOR_TYPE}" = "71" ];then
		SENSOR_KO_NAME=sp2305_mipi.ko
		SENSOR_DRIVE_NAME=sp2305_mipi
		SENSOR_ADDR=0x78
	elif [ "${SENSOR_TYPE}" = "72" ];then
		SENSOR_KO_NAME=sp2305_mipi_red.ko
		SENSOR_DRIVE_NAME=sp2305_mipi
		SENSOR_ADDR=0x78					
	else
		echo "no config hw"
	fi


	#拷贝sensor驱动 未过产测调试用
	if [ -d /tmp/sd/sensor_drive/ ];then
		if [ ! -f /backup/ko/cam_sensor.ko ];then
			if [ "${SENSOR_TYPE}" != "0" ];then
				cp /tmp/sd/sensor_drive/$SENSOR_KO_NAME  /backup/ko/cam_sensor.ko
			fi
			sync
		fi
	fi

	#拷贝IQ 未过产测调试用
	if [ -d /tmp/sd/isp/ ];then
		if [ ! -f /backup/isp/day_3a_settings.bin ];then
				cp /tmp/sd/isp/day_3a_settings.bin  /backup/isp
				cp /tmp/sd/isp/day_iso_settings.bin  /backup/isp
				cp /tmp/sd/isp/day_test_settings.bin  /backup/isp
				cp /tmp/sd/isp/day_tunning_settings.bin  /backup/isp
				cp /tmp/sd/isp/night_3a_settings.bin  /backup/isp
				cp /tmp/sd/isp/night_iso_settings.bin  /backup/isp
				cp /tmp/sd/isp/night_test_settings.bin  /backup/isp
				cp /tmp/sd/isp/night_tunning_settings.bin  /backup/isp
			sync
		fi
	fi

fi


#extract files
mkdir /tmp/ko/
mkdir /tmp/app/
mkdir /tmp/tools/

if [ -f /home/home_${SUFFIX}m ]; then
    echo "---/home/home_${SUFFIX}m exist, update begin---"
	dd if=/home/home_${SUFFIX}m of=/tmp/newver bs=22 count=1
	newver=$(cat /tmp/newver)
	if [ -f /home/homever ]; then
		curver=$(cat /home/homever)
	else
		curver=0
	fi
	echo check version: newver=$newver, curver=$curver
	if [ $newver != $curver ]; then
		### cipher ###
		sleep 1
		mkdir /tmp/update
		cp -rf /home/base/tools/extpkg.sh /tmp/update/extpkg.sh
		/tmp/update/extpkg.sh /home/home_v429m
		rm -rf /tmp/update
		rm -rf /home/home_${SUFFIX}m
		#sync
		echo "update finish"
		reboot -f
	fi
	echo "---same version ? update fail---"
	rm -rf mv /home/home_${SUFFIX}m
elif [ -f /tmp/sd/home_${SUFFIX}m ]; then
	echo "---tmp/sd/home_${SUFFIX}m exist, update begin---"
	dd if=/tmp/sd/home_${SUFFIX}m of=/tmp/newver bs=22 count=1
	newver=$(cat /tmp/newver)
	if [ -f /home/homever ]; then
		curver=$(cat /home/homever)
	else
		curver=0
	fi
	echo check version: newver=$newver, curver=$curver
	if [ $newver != $curver ]; then
		### cipher ###
		sleep 1
		mkdir /tmp/update
        cp -rf /tmp/sd/home_${SUFFIX}m /tmp/update/
		cp -rf /backup/tools/extpkg.sh /tmp/update/extpkg.sh
		/tmp/update/extpkg.sh /tmp/sd/home_${SUFFIX}m
		rm -rf /tmp/update
		mv /tmp/sd/home_${SUFFIX}m	/tmp/sd/home_${SUFFIX}m.done
		sync
		echo "update finish"
		reboot -f
	fi
	echo "---same version ? update fail---"
	mv /tmp/sd/home_${SUFFIX}m	/tmp/sd/home_${SUFFIX}m.done
else
	echo "---update file(home_${SUFFIX}m) Not exist---"
fi

if [ -f /backup/url ];then
    
    if [ "${enable_4g}" = "y" ];then
        /home/app/4g &
        # sleep 40
    else
        #when ota upgrade failure, will enter this
        echo "--------------------------insmod wifi--------------------------"

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
			if [ "${SUFFIX}" = "d071qp" ];then
				if [ -f /home/base/firmware/ssv6x5x/ssv6152-wifi.cfg ];then
					insmod /backup/ko/ssv6x5x.ko stacfgpath="/home/base/firmware/ssv6x5x/ssv6152-wifi.cfg"
				else
					echo "not found ssv6x5x-wifi.cfg"
				fi	
			else
				if [ -f /home/base/firmware/ssv6x5x/ssv6x5x-wifi.cfg ];then
					insmod /backup/ko/ssv6x5x.ko stacfgpath="/home/base/firmware/ssv6x5x/ssv6x5x-wifi.cfg"
				else
					echo "not found ssv6x5x-wifi.cfg"
				fi	
			fi	
		fi

		if [ -f /home/base/ko/icplus.ko ];then
			insmod /home/base/ko/icplus.ko
		elif [ -f /backup/ko/icplus.ko ];then
			insmod /backup/ko/icplus.ko
		fi

        sleep 1
		ifconfig lo up
		ifconfig ${NETWORK_IFACE} up
		ethmac=d2:`ifconfig ${NETWORK_IFACE} |grep HWaddr|cut -d' ' -f10|cut -d: -f2-`
		ifconfig eth0 hw ether $ethmac
		a=1
		if [ "${SUFFIX}" = "b111qp" ] || [ "${SUFFIX}" = "b101qp" ] || [ "${SUFFIX}" = "b092qp" ] || [ "${SUFFIX}" = "b091qp" ] || [ "${SUFFIX}" = "q321br_aldz_3m" ]; then
			while ( ! ifconfig eth0 up)
			do
				echo "ifconfig eth0 up failed"
				let a++
				if [ $a -eq 10 ]; then
					break
				fi
				sleep 1
			done
		else
			ifconfig eth0 up
		fi
    fi

    echo "============================================= begin to upgrade...... ========================================="
    /backup/tools/upgrade.sh
    reboot -f
fi

if [ "${enable_4g}" = "y" ];then
    /home/app/4g &
fi

# Running telnetd
/usr/sbin/telnetd &

if [ -f /tmp/sd/lower_half_init.sh ];then
    source /tmp/sd/lower_half_init.sh
elif [ -f /home/app/lower_half_init.sh ];then
    source /home/app/lower_half_init.sh
else
    source /backup/lower_half_init.sh
fi
