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

    ### Replace /backup/init.sh with a more friendly one
    echo "### Updating /backup/init.sh"
    if [ -f /tmp/sd/newbackup/init.sh ]; then
        cp -f /tmp/sd/newbackup/init.sh /backup/init.sh
        chmod 0755 /backup/init.sh
    fi

    ### Replace /backup/tools/wifidhcp.sh with hostname option
    echo "### Updating /backup/tools/wifidhcp.sh"
    if [ -f /tmp/sd/newbackup/tools/wifidhcp.sh ]; then
        cp -f /tmp/sd/newbackup/tools/wifidhcp.sh /backup/tools/wifidhcp.sh
        chmod 0755 /backup/tools/wifidhcp.sh
    fi
fi

### Disable the hack for next reboot
echo "### Disabling hack for next reboot"
if [ -e /tmp/sd/Factory.done ]; then
    rm -rf /tmp/sd/Factory.done
fi
if [ -e /tmp/sd/Factory ]; then
    mv /tmp/sd/Factory /tmp/sd/Factory.done
fi

reboot
