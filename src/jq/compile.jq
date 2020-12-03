#!/bin/bash

SCRIPT_DIR=$(cd `dirname $0` && pwd)
cd $SCRIPT_DIR

cd jq-1.5 || exit 1

make clean
make || exit 1

mkdir -p ../_install/bin || exit 1

cp ./jq ../_install/bin || exit 1

arm-openwrt-linux-strip ../_install/bin/* || exit 1
