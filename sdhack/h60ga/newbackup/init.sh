#!/bin/sh
SUFFIX=h60ga

PATH="/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin"
LD_LIBRARY_PATH="/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib"
export PATH
export LD_LIBRARY_PATH

NETWORK_IFACE=wlan0

ulimit -c unlimited
ulimit -s 1024

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

#extract files
mkdir /tmp/ko/
mkdir /tmp/app/
mkdir /tmp/tools/

if [ -f /home/home_${SUFFIX}m ]; then
	dd if=/home/home_${SUFFIX}m of=/tmp/newver bs=22 count=1
	newver=$(cat /tmp/newver)
	if [ -f /home/homever ]; then
		curver=$(cat /home/homever)
	else
		curver=0
	fi
	if [ $newver != $curver ]; then
		### cipher ###
		sleep 1
		mkdir /tmp/update
		cp -rf /home/base/tools/extpkg.sh /tmp/update/extpkg.sh
		/tmp/update/extpkg.sh /home/home_v429m
		rm -rf /tmp/update
		rm -rf /home/home_${SUFFIX}m
		#sync
		reboot -f
	fi
	rm -rf mv /home/home_${SUFFIX}m
elif [ -f /tmp/sd/home_${SUFFIX}m ]; then
	dd if=/tmp/sd/home_${SUFFIX}m of=/tmp/newver bs=22 count=1
	newver=$(cat /tmp/newver)
	if [ -f /home/homever ]; then
		curver=$(cat /home/homever)
	else
		curver=0
	fi
	if [ $newver != $curver ]; then
		### cipher ###
		sleep 1
		mkdir /tmp/update
        cp -rf /tmp/sd/home_${SUFFIX}m /tmp/update/
		cp -rf /backup/tools/extpkg.sh /tmp/update/extpkg.sh
		/tmp/update/extpkg.sh /tmp/sd/home_${SUFFIX}m
		rm -rf /tmp/update
		mv /tmp/sd/home_${SUFFIX}m /tmp/sd/home_${SUFFIX}m.done
		sync
		reboot -f
	fi
	mv /tmp/sd/home_${SUFFIX}m /tmp/sd/home_${SUFFIX}m.done
fi

if [ -f /backup/url ];then
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
	ifconfig eth0 up

	/backup/tools/upgrade.sh
	reboot -f
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
