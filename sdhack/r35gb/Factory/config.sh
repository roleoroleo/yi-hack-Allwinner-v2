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
