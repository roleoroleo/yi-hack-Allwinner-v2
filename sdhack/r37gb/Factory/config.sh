echo "############## Starting Firmware Dump ##############"
echo "############## Starting Firmware Dump ##############"
echo "############## Starting Firmware Dump ##############"
echo "############## Starting Firmware Dump ##############"

mkdir -p /tmp/sd/backup

cat /proc/mtd > /tmp/sd/backup/mtd.txt

dd if=/dev/mtdblock0 of=/tmp/sd/backup/mtdblock0.bin
dd if=/dev/mtdblock1 of=/tmp/sd/backup/mtdblock1.bin
dd if=/dev/mtdblock2 of=/tmp/sd/backup/mtdblock2.bin
dd if=/dev/mtdblock3 of=/tmp/sd/backup/mtdblock3.bin
dd if=/dev/mtdblock4 of=/tmp/sd/backup/mtdblock4.bin
dd if=/dev/mtdblock5 of=/tmp/sd/backup/mtdblock5.bin
dd if=/dev/mtdblock6 of=/tmp/sd/backup/mtdblock6.bin
dd if=/dev/mtdblock7 of=/tmp/sd/backup/mtdblock7.bin

cp /home/homever /tmp/sd/backup/homever.txt



echo "############## Starting Hack ##############"
echo "############## Starting Hack ##############"
echo "############## Starting Hack ##############"
echo "############## Starting Hack ##############"


### Check if hack is present
echo "### Hacking"
if [ `grep telnetd /backup/init.sh | grep -c ^` -gt 0 ]; then
    echo "Hack already applied"
else
    echo "Applying hack"

    ### Update /backup/init.sh with a more friendly one
    echo "### Updating /backup/init.sh"
    cp /backup/init.sh /tmp/init.sh
    sed -n '1{$!N;$!N;$!N;$!N};$!N;s@\nif \[ \-f \/home\/app\/lower_half_init.sh \];then\n    source \/home\/app\/lower_half_init.sh\nelse\n    source \/backup\/lower_half_init.sh\nfi@@;P;D' -i /tmp/init.sh
    sed -n '1{$!N;$!N;$!N;$!N};$!N;s@\nif \[ \-f \/home\/app\/lower_half_init.sh \];then\n\tsource \/home\/app\/lower_half_init.sh\nelse\n\tsource \/backup\/lower_half_init.sh\nfi@@;P;D' -i /tmp/init.sh
    sed -e 's/^source \/home\/app\/lower_half_init.sh//g' -i /tmp/init.sh

    echo "# Running telnetd" >> /tmp/init.sh
    echo "/usr/sbin/telnetd &" >> /tmp/init.sh
    echo "" >> /tmp/init.sh
    echo "if [ -f /tmp/sd/lower_half_init.sh ];then" >> /tmp/init.sh
    echo "    source /tmp/sd/lower_half_init.sh" >> /tmp/init.sh
    echo "elif [ -f /home/app/lower_half_init.sh ];then" >> /tmp/init.sh
    echo "    source /home/app/lower_half_init.sh" >> /tmp/init.sh
    echo "else" >> /tmp/init.sh
    echo "    source /backup/lower_half_init.sh" >> /tmp/init.sh
    echo "fi" >> /tmp/init.sh

    cp /tmp/init.sh /backup/init.sh
    rm /tmp/init.sh
fi

### Set wireless credentials if configure_wifi.cfg exists
if [ -e /tmp/sd/Factory/configure_wifi.cfg ]; then
    /tmp/sd/Factory/configure_wifi.sh
fi

### Disable the hack for next reboot
echo "### Disabling hack for next reboot"
if [ -e /tmp/sd/Factory.done ]; then
    rm -rf /tmp/sd/Factory.done
fi
if [ -e /tmp/sd/Factory ]; then
    mv /tmp/sd/Factory /tmp/sd/Factory.done
fi

sync
sync
sync

reboot
