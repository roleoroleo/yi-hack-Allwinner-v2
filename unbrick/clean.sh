#!/bin/bash

for DIR in * ; do
    if [ -d $DIR ]; then
        FILENAME=`cat $DIR/filename`
        rm -rf $DIR/mnt
        rm -rf $DIR/backup
        rm -f $DIR/backup_$FILENAME.gz
    fi
done
