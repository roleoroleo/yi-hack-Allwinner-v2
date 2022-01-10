#!/bin/sh
echo "--------------------------home app init.sh--------------------------"
SUFFIX=r35gb
enable_4g=

PATH="/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin"
LD_LIBRARY_PATH="/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib"
export PATH
export LD_LIBRARY_PATH

NETWORK_IFACE=

if [ "${SUFFIX}" = "b091qp" ];then
	echo "b091qp NETWORK_IFACE depends on HW params!"
else
	if [ "${enable_4g}" = "y" ];then
		NETWORK_IFACE=eth1
	else
		NETWORK_IFACE=wlan0
	fi
	echo "NETWORK_IFACE=${NETWORK_IFACE}"

	echo "product_name=${SUFFIX}"
fi

if [ "${SUFFIX}" = "q705ssv" ];then
echo 197 > /sys/class/gpio/export 
echo out > /sys/class/gpio/gpio197/direction  
echo 0 > /sys/class/gpio/gpio197/value 
sleep 1
echo 1 > /sys/class/gpio/gpio197/value
elif [ "${SUFFIX}" = "h50ga" ] || [ "${SUFFIX}" = "y29ga" ]|| [ "${SUFFIX}" = "y21ga" ]|| [ "${SUFFIX}" = "y28ga" ]|| [ "${SUFFIX}" = "h51ga" ]|| [ "${SUFFIX}" = "h60ga" ]|| [ "${SUFFIX}" = "h31bg" ]|| [ "${SUFFIX}" = "h30ga" ]|| [ "${SUFFIX}" = "h52ga" ]|| [ "${SUFFIX}" = "h53ga" ]|| [ "${SUFFIX}" = "y19ga" ];then
echo "need reset gpio198"

elif [ "${SUFFIX}" = "y211ga" ] || [ "${SUFFIX}" = "y211ba" ] || [ "${SUFFIX}" = "h32ga" ] || [ "${SUFFIX}" = "h33ga" ] || [ "${SUFFIX}" = "y621" ] || [ "${SUFFIX}" = "b621" ];then
    echo "need reset gpio198"
    echo 198 > /sys/class/gpio/export 
    echo out > /sys/class/gpio/gpio198/direction  
    echo 0 > /sys/class/gpio/gpio198/value 
    sleep 1
    echo 1 > /sys/class/gpio/gpio198/value
elif [ "${SUFFIX}" = "p621" ];then
    echo "need reset gpio145"
    echo 145 > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio145/direction  
    echo 0 > /sys/class/gpio/gpio145/value 
    sleep 1
    echo 1 > /sys/class/gpio/gpio145/value

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
mount -t vfat /dev/mmcblk0 /tmp/sd

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

	#是否为4G设备
	IS_4G_DEVICE=`echo ${HW:49:1}`
	echo "IS_4G_DEVICE:$IS_4G_DEVICE"
	if [ "${IS_4G_DEVICE}" = "0" ];then
		enable_4g=n
	else
		enable_4g=y
		GPIO_4G_ENABLE=`echo ${GPIO:39:3}`
		GPIO_4G_VDD=`echo ${GPIO:70:3}`
		GPIO_4G_VBUS=`echo ${GPIO:73:3}`
		echo "GPIO_4G_ENABLE=${GPIO_4G_ENABLE}"
		echo "GPIO_4G_VDD=${GPIO_4G_VDD}"
		echo "GPIO_4G_VBUS=${GPIO_4G_VBUS}"
	fi
	if [ -d /tmp/sd/usb_4g_driver/ ]&&[ "${enable_4g}" = "y" ];then
		if [ ! -f /backup/usb_4g_driver/usbnet.ko ]; then
			cp  /tmp/sd/usb_4g_driver/*  /backup/usb_4g_driver/
		fi
		if [ ! -f /backup/app/4g ]; then
			cp  /tmp/sd/b091_app/4g  /backup/app/
		fi
		sync
	fi
	
	if [ "${enable_4g}" = "y" ];then
		NETWORK_IFACE=eth1
	else
		NETWORK_IFACE=wlan0
	fi
	
	echo "NETWORK_IFACE=${NETWORK_IFACE}"

	echo "product_name=${SUFFIX}"

	#拷贝wifi驱动 未过产测调试用
	WIFI_TYPE=`echo ${HW:37:1}`
	echo "WIFI_TYPE:$WIFI_TYPE"

	if [ -d /tmp/sd/wifi_drive/ ]&&[ "${enable_4g}" = "n" ];then
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
		elif [ "${WIFI_TYPE}" = "5" ];then
			if [ ! -f /backup/ko/ssv6x5x.ko ];then
			cp /tmp/sd/wifi_drive/ssv6x5x.ko  /backup/ko/
			fi
		elif [ "${WIFI_TYPE}" = "6" ];then
			if [ ! -f /backup/ko/8192fu.ko ];then
			cp  /tmp/sd/wifi_drive/8192fu.ko /backup/ko/
			fi
		elif [ "${WIFI_TYPE}" = "7" ];then
			if [ ! -f /backup/ko/rdawfmac.ko ];then
			cp  /tmp/sd/wifi_drive/rdawfmac.ko /backup/ko/
			fi	

			if [ ! -d /backup/ko/rda5995-usb ];then
			cp  /tmp/sd/wifi_drive/rda5995-usb/  /backup/ko/ -rf
			fi

		else
		echo "no config hw"
		fi
		sync
	fi

	if [ "${WIFI_TYPE}" = "1" ];then
		SSV_WIFI_TYPE=USB

	elif [ "${WIFI_TYPE}" = "5" ];then
		SSV_WIFI_TYPE=SDIO
	fi


	#sensor 
	SENSOR_TYPE=`echo ${HW:38:2}`
	echo "SENSOR_TYPE:$SENSOR_TYPE"
	if [ "${SENSOR_TYPE}" = "01" ];then
		SENSOR_KO_NAME=f37_mipi_321_705_1217.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
		ISP=F37_dropframe
	elif [ "${SENSOR_TYPE}" = "02" ];then
		SENSOR_KO_NAME=f37_mipi_321_705.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
		ISP=F37
	elif [ "${SENSOR_TYPE}" = "03" ];then
		SENSOR_KO_NAME=f37_mipi_b011_h011.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
		ISP=F37
	elif [ "${SENSOR_TYPE}" = "04" ];then
		SENSOR_KO_NAME=f37_mipi.ko
		SENSOR_DRIVE_NAME=f37_mipi
		SENSOR_ADDR=0x80
		ISP=F37
	elif [ "${SENSOR_TYPE}" = "11" ];then
		SENSOR_KO_NAME=h63_mipi.ko
		SENSOR_DRIVE_NAME=h63_mipi
		SENSOR_ADDR=0x80
		ISP=H63
	elif [ "${SENSOR_TYPE}" = "21" ];then
		SENSOR_KO_NAME=C2390A_mipi.ko
		SENSOR_DRIVE_NAME=C2390A_mipi
		SENSOR_ADDR=0x6C
		ISP=C2390A	
	elif [ "${SENSOR_TYPE}" = "31" ];then
		SENSOR_KO_NAME=gc2053_mipi.ko
		SENSOR_DRIVE_NAME=gc2053_mipi
		SENSOR_ADDR=0x6e
		ISP=GC2053
	elif [ "${SENSOR_TYPE}" = "35" ];then
		SENSOR_KO_NAME=gc1054_mipi.ko
		SENSOR_DRIVE_NAME=gc1054_mipi
		SENSOR_ADDR=0x42
		ISP=GC1054
	elif [ "${SENSOR_TYPE}" = "36" ];then
		SENSOR_KO_NAME=gc2093_mipi.ko
		SENSOR_DRIVE_NAME=gc2093_mipi
		SENSOR_ADDR=0x6e
		ISP=GC2093		
	elif [ "${SENSOR_TYPE}" = "61" ];then
		SENSOR_KO_NAME=sc2231_mipi.ko
		SENSOR_DRIVE_NAME=sc2231_mipi
		SENSOR_ADDR=0x60
		ISP=SC2231	
	elif [ "${SENSOR_TYPE}" = "63" ];then
		SENSOR_KO_NAME=sc2332_mipi.ko
		SENSOR_DRIVE_NAME=sc2332_mipi
		SENSOR_ADDR=0x60
		ISP=SC2332
	elif [ "${SENSOR_TYPE}" = "64" ];then
		SENSOR_KO_NAME=sc2335_mipi.ko
		SENSOR_DRIVE_NAME=sc2335_mipi
		SENSOR_ADDR=0x60
		ISP=SC2335	
	elif [ "${SENSOR_TYPE}" = "65" ];then
		SENSOR_KO_NAME=sc3335_mipi.ko
		SENSOR_DRIVE_NAME=sc3335_mipi
		SENSOR_ADDR=0x60
		ISP=SC3335
	elif [ "${SENSOR_TYPE}" = "71" ];then
		SENSOR_KO_NAME=sp2305_mipi.ko
		SENSOR_DRIVE_NAME=sp2305_mipi
		SENSOR_ADDR=0x78
		ISP=SP2305
	elif [ "${SENSOR_TYPE}" = "72" ];then
		SENSOR_KO_NAME=sp2305_mipi_red.ko
		SENSOR_DRIVE_NAME=sp2305_mipi
		SENSOR_ADDR=0x78
		ISP=SP2305_red
	elif [ "${SENSOR_TYPE}" = "73" ];then
		SENSOR_KO_NAME=sp1405_mipi_0122.ko
		SENSOR_DRIVE_NAME=sp1405_mipi
		SENSOR_ADDR=0x6C
		ISP=SP1405						
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
			if [ "${SENSOR_TYPE}" != "0" ];then
				cp /tmp/sd/isp/$ISP/day_3a_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/day_iso_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/day_test_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/day_tunning_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/night_3a_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/night_iso_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/night_test_settings.bin  /backup/isp
				cp /tmp/sd/isp/$ISP/night_tunning_settings.bin  /backup/isp
				sync
			fi	
		fi
	fi
# other prj ssv driver use usb 
else 
	SSV_WIFI_TYPE=USB
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
		if [ "${SUFFIX}" = "b091qp" ];then
			/backup/app/4g ${GPIO_4G_ENABLE} ${GPIO_4G_VDD} ${GPIO_4G_VBUS} 40 &
		else
			/home/app/4g &
		fi
        # sleep 40
    else
        #when ota upgrade failure, will enter this
        echo "--------------------------insmod wifi--------------------------"

		if [ -f /home/base/wifi/8188fu.ko ];then
			insmod /home/base/wifi/8188fu.ko
		elif [ -f /home/base/wifi/8189fs.ko ];then
			insmod /home/base/wifi/8189fs.ko
		elif [ -f /backup/ko/8188fu.ko ];then
			insmod /backup/ko/8188fu.ko
		elif [ -f /backup/ko/8189fs.ko ];then
			insmod /backup/ko/8189fs.ko
		elif [ -f /backup/ko/8192fu.ko ];then
			insmod /backup/ko/8192fu.ko	
		elif [ -f /backup/ko/atbm603x_wifi_usb.ko ];then
			insmod /backup/ko/atbm603x_wifi_usb.ko
		elif [ -f /backup/ko/rdawfmac.ko ];then
			insmod /backup/ko/rdawfmac.ko
		elif [ -f /backup/ko/hi3881.ko ];then
			insmod /backup/ko/hi3881.ko
		fi

		if [ -f /backup/ko/ssv6x5x.ko ];then
			if [ "${SUFFIX}" = "d071qp" ];then
				if [ -f /home/base/firmware/ssv6x5x/ssv6152-wifi.cfg ];then
					insmod /backup/ko/ssv6x5x.ko stacfgpath="/home/base/firmware/ssv6x5x/ssv6152-wifi.cfg" wifi_type=SDIO
				else
					echo "not found ssv6x5x-wifi.cfg"
				fi	
			else
				if [ -f /home/base/firmware/ssv6x5x/ssv6x5x-wifi.cfg ];then
					insmod /backup/ko/ssv6x5x.ko stacfgpath="/home/base/firmware/ssv6x5x/ssv6x5x-wifi.cfg" wifi_type=$SSV_WIFI_TYPE
				else
					echo "not found ssv6x5x-wifi.cfg"
				fi	
			fi	
		elif [ -f /backup/ko/ssv6155.ko ];then
			if [ -f /home/base/firmware/ssv6155_jixian/ssv6155-wifi.cfg ];then
				 insmod /backup/ko/ssv6155.ko stacfgpath="/home/base/firmware/ssv6155_jixian/ssv6155-wifi.cfg" wifi_type=USB
			else
				  echo "not found ssv6155-wifi.cfg"
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
    source /backup/tools/upgrade.sh
    reboot -f
fi

if [ "${enable_4g}" = "y" ];then
	if [ "${SUFFIX}" = "b091qp" ];then
		/backup/app/4g ${GPIO_4G_ENABLE} ${GPIO_4G_VDD} ${GPIO_4G_VBUS} 40 &
		#/home/app/4g 197 194 196 40 &
	else
		/home/app/4g &
	fi
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
