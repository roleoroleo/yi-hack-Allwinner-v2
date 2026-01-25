#!/bin/bash

set -e

. ./config.jq
export CROSSPATH=/opt/yi/toolchain-sunxi-musl/toolchain/bin
export PATH=${PATH}:${CROSSPATH}

export TARGET=arm-openwrt-linux
export CROSS=arm-openwrt-linux
export BUILD=x86_64-pc-linux-gnu

export CROSSPREFIX=${CROSS}-

export STRIP=${CROSSPREFIX}strip
export CXX=${CROSSPREFIX}g++
export CC=${CROSSPREFIX}gcc
export LD=${CROSSPREFIX}ld
export AS=${CROSSPREFIX}as
export AR=${CROSSPREFIX}ar

SCRIPT_DIR=$(cd `dirname $0` && pwd)
cd $SCRIPT_DIR

cd jq-${VERSION}

make clean
make -j $(nproc)

mkdir -p ../_install/bin
mkdir -p ../_install/lib

cp ./vendor/oniguruma/src/.libs/libonig.so* ../_install/lib
cp ./.libs/libjq.so* ../_install/lib
cp ./.libs/jq ../_install/bin

$STRIP ../_install/bin/*
$STRIP ../_install/lib/*
