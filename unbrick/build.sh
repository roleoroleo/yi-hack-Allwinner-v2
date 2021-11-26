#!/bin/bash

TYPE=$1
if [ "$TYPE" != "factory" ] && [ "$TYPE" != "hacked" ]; then
    echo "Usage: $0 type"
    echo -e "\twhere type = \"factory\" or \"hacked\""
    exit
fi

for DIR in * ; do
    if [ -d $DIR ]; then
        FILENAME=`cat $DIR/filename`
        if [ "$TYPE" == "hacked" ]; then
            mkdir -p $DIR/mnt
            ./mount.jffs2 $DIR/mtdblock4.bin $DIR/mnt 4
            cp -r $DIR/mnt $DIR/backup
            umount $DIR/mnt
            rm -rf $DIR/mnt
            cp -f $DIR/newbackup/init.sh $DIR/backup/init.sh
            if [ $FILENAME == "b091qp" ]; then
                PAD="0x00150000"
            else
                PAD="0x00130000"
            fi
            ./create.jffs2 $PAD $DIR/backup $DIR/mtdblock4_hacked.bin
            rm -rf $DIR/backup
        else
            cp $DIR/mtdblock4.bin $DIR/mtdblock4_hacked.bin
        fi
        mkimage -A arm -O linux -T filesystem -C none -a 0x0 -e 0x0 -n "xiaoyi-backup" -d $DIR/mtdblock4_hacked.bin $DIR/backup_$FILENAME
        rm -f $DIR/mtdblock4_hacked.bin
        gzip $DIR/backup_$FILENAME
    fi
done
