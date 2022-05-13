#!/bin/bash

update_init()
{
    ### Update /backup/init.sh with a more friendly one
    echo "### Updating $DIR/backup/init.sh"
    cp $DIR/backup/init.sh $DIR/backup/tmp_init.sh
    sed -n '1{$!N;$!N;$!N;$!N};$!N;s@\nif \[ \-f \/home\/app\/lower_half_init.sh \];then\n    source \/home\/app\/lower_half_init.sh\nelse\n    source \/backup\/lower_half_init.sh\nfi@@;P;D' -i $DIR/backup/tmp_init.sh
    sed -n '1{$!N;$!N;$!N;$!N};$!N;s@\nif \[ \-f \/home\/app\/lower_half_init.sh \];then\n\tsource \/home\/app\/lower_half_init.sh\nelse\n\tsource \/backup\/lower_half_init.sh\nfi@@;P;D' -i $DIR/backup/tmp_init.sh

    echo "# Running telnetd" >> $DIR/backup/tmp_init.sh
    echo "/usr/sbin/telnetd &" >> $DIR/backup/tmp_init.sh
    echo "" >> $DIR/backup/tmp_init.sh
    echo "if [ -f /tmp/sd/lower_half_init.sh ];then" >> $DIR/backup/tmp_init.sh
    echo "    source /tmp/sd/lower_half_init.sh" >> $DIR/backup/tmp_init.sh
    echo "elif [ -f /home/app/lower_half_init.sh ];then" >> $DIR/backup/tmp_init.sh
    echo "    source /home/app/lower_half_init.sh" >> $DIR/backup/tmp_init.sh
    echo "else" >> $DIR/backup/tmp_init.sh
    echo "    source /backup/lower_half_init.sh" >> $DIR/backup/tmp_init.sh
    echo "fi" >> $DIR/backup/tmp_init.sh

    cp $DIR/backup/tmp_init.sh $DIR/backup/init.sh
    rm $DIR/backup/tmp_init.sh
}

TYPE=$1
if [ "$TYPE" != "factory" ] && [ "$TYPE" != "hacked" ]; then
    echo "Usage: $0 type"
    echo -e "\twhere type = \"factory\" or \"hacked\""
    exit
fi

for DIR in * ; do
    if [ -d $DIR ]; then
        FILENAME=`cat $DIR/filename`
        if [ -f $DIR/mtdblock2.bin ]; then
            mkimage -A arm -O linux -T filesystem -C none -a 0x0 -e 0x0 -n "xiaoyi-rootfs" -d $DIR/mtdblock2.bin $DIR/rootfs_$FILENAME
            gzip $DIR/rootfs_$FILENAME
        fi
        if [ -f $DIR/mtdblock3.bin ]; then
            mkimage -A arm -O linux -T filesystem -C none -a 0x0 -e 0x0 -n "xiaoyi-home" -d $DIR/mtdblock3.bin $DIR/home_$FILENAME
            gzip $DIR/home_$FILENAME
        fi
        if [ -f $DIR/mtdblock4.bin ]; then
            if [ "$TYPE" == "hacked" ]; then
                mkdir -p $DIR/mnt
                ./mount.jffs2 $DIR/mtdblock4.bin $DIR/mnt 4
                cp -r $DIR/mnt $DIR/backup
                umount $DIR/mnt
                rm -rf $DIR/mnt
                update_init
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
    fi
done
