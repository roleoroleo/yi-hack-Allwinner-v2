#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib


NAME="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

if [ "$NAME" != "get" ] ; then
    exit
fi

if [ "$VAL" == "info" ] ; then
    printf "Content-type: application/json\r\n\r\n"

    FW_VERSION=`cat /tmp/sd/yi-hack/version`
    LATEST_FW=`/tmp/sd/yi-hack/usr/bin/wget -O -  https://api.github.com/repos/roleoroleo/yi-hack-Allwinner-v2/releases/latest 2>&1 | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'`

    printf "{\n"
    printf "\"%s\":\"%s\",\n" "fw_version"      "$FW_VERSION"
    printf "\"%s\":\"%s\"\n" "latest_fw"       "$LATEST_FW"
    printf "}"

elif [ "$VAL" == "upgrade" ] ; then

    FREE_SD=$(df /tmp/sd/ | grep mmc | awk '{print $4}')
    if [ -z "$FREE_SD" ]; then
        printf "Content-type: text/html\r\n\r\n"
        printf "No SD detected."
        exit
    fi

    if [ $FREE_SD -lt 100000 ]; then
        printf "Content-type: text/html\r\n\r\n"
        printf "No space left on SD."
        exit
    fi

    # Clean old upgrades
    rm -rf /tmp/sd/.fw_upgrade
    rm -rf /tmp/sd/.fw_upgrade.conf
    rm -rf /tmp/sd/Factory
    rm -rf /tmp/sd/newhome

    mkdir -p /tmp/sd/.fw_upgrade
    mkdir -p /tmp/sd/.fw_upgrade.conf
    cd /tmp/sd/.fw_upgrade

    MODEL_SUFFIX=`cat $YI_HACK_PREFIX/model_suffix`
    FW_VERSION=`cat /tmp/sd/yi-hack/version`
    if [ -f /tmp/sd/${MODEL_SUFFIX}_x.x.x.tgz ]; then
        mv /tmp/sd/${MODEL_SUFFIX}_x.x.x.tgz /tmp/sd/.fw_upgrade/${MODEL_SUFFIX}_x.x.x.tgz
        LATEST_FW="x.x.x"
    else
        LATEST_FW=`/tmp/sd/yi-hack/usr/bin/wget -O -  https://api.github.com/repos/roleoroleo/yi-hack-Allwinner-v2/releases/latest 2>&1 | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'`
        if [ "$FW_VERSION" == "$LATEST_FW" ]; then
            printf "Content-type: text/html\r\n\r\n"
            printf "No new firmware available."
            exit
        fi

        /tmp/sd/yi-hack/usr/bin/wget https://github.com/roleoroleo/yi-hack-Allwinner-v2/releases/download/$LATEST_FW/${MODEL_SUFFIX}_${LATEST_FW}.tgz
        if [ ! -f ${MODEL_SUFFIX}_${LATEST_FW}.tgz ]; then
            printf "Content-type: text/html\r\n\r\n"
            printf "Unable to download firmware file."
            exit
        fi
    fi

    # Backup configuration
    cp -rf $YI_HACK_PREFIX/etc/* /tmp/sd/.fw_upgrade.conf/
    rm /tmp/sd/.fw_upgrade.conf/*.tar.gz

    # Prepare new hack
    tar zxvf ${MODEL_SUFFIX}_${LATEST_FW}.tgz
    rm ${MODEL_SUFFIX}_${LATEST_FW}.tgz
    mkdir -p /tmp/sd/.fw_upgrade/yi-hack/etc
    cp -rf /tmp/sd/.fw_upgrade.conf/* /tmp/sd/.fw_upgrade/yi-hack/etc/

    # Report the status to the caller
    printf "Content-type: text/html\r\n\r\n"
    printf "Download completed, rebooting and upgrading."

    sync
    sync
    sync
    sleep 1
    reboot
fi
