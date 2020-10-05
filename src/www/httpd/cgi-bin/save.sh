#!/bin/sh

printf "Content-type: application/octet-stream\r\n\r\n"

TMP_DIR="/tmp/yi-temp-save"
mkdir $TMP_DIR
cd $TMP_DIR
cp /tmp/sd/yi-hack/etc/*.conf .
if [ -f /tmp/sd/yi-hack/etc/hostname ]; then
    cp /tmp/sd/yi-hack/etc/hostname .
fi
tar cvf config.tar * > /dev/null
bzip2 config.tar
cat $TMP_DIR/config.tar.bz2
cd /tmp
rm -rf $TMP_DIR
