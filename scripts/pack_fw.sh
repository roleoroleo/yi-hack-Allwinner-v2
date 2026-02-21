#!/bin/bash

#
#  This file is part of yi-hack-v4 (https://github.com/TheCrypt0/yi-hack-v4).
#  Copyright (c) 2018-2019 Davide Maggioni.
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 3.
# 
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#

get_script_dir()
{
    echo "$(cd `dirname $0` && pwd)"
}

create_tmp_dir()
{
    local TMP_DIR=$(mktemp -d)

    if [[ ! "$TMP_DIR" || ! -d "$TMP_DIR" ]]; then
        echo "ERROR: Could not create temp dir \"$TMP_DIR\". Exiting."
        exit 1
    fi

    echo $TMP_DIR
}

compress_file()
{
    local DIR=$1
    local FILENAME=$2
    local FILE=$DIR/$FILENAME
    echo -n "    Compressing $FILE..."
    7za a "$FILE.7z" "$FILE" > /dev/null
    rm -f "$FILE"
    echo "done!"
}

pack_image()
{
    local TYPE=$1
    local CAMERA_ID=$2
    local DIR=$3
    local OUT=$4

    echo ">>> Packing ${TYPE}_${CAMERA_ID}"

    echo TYPE $TYPE
    echo CAMERA_ID $CAMERA_ID
    echo DIR $DIR
    echo OUT $OUT
    echo -n "    Creating tar.bz2 archive in $DIR/${TYPE}_${CAMERA_ID}.tar.bz2... "
    tar jcvf $OUT/${TYPE}_${CAMERA_ID}.tar.bz2 -C $DIR $TYPE || exit 1
    echo "done!"
}

###############################################################################

source "$(get_script_dir)/common.sh"

require_root


if [ $# -ne 1 ]; then
    echo "Usage: pack_sw.sh camera_name"
    echo ""
    exit 1
fi

CAMERA_NAME=$1

check_camera_name $CAMERA_NAME

CAMERA_ID=$(get_camera_id $CAMERA_NAME)

BASE_DIR=$(get_script_dir)/../
BASE_DIR=$(normalize_path $BASE_DIR)
SYSROOT_DIR=$BASE_DIR/sysroot/$CAMERA_NAME


SDHACK_DIR=$BASE_DIR/sdhack
STATIC_DIR=$BASE_DIR/static
BUILD_DIR=$BASE_DIR/build
OUT_DIR=$BASE_DIR/out/$CAMERA_NAME
VER=$(cat VERSION)

# Append git short hash when building outside a tagged release,
# so local builds are distinguishable from official releases.
GIT_TAG=$(git -C "$BASE_DIR" tag --points-at HEAD 2>/dev/null | grep -Fx "$VER" | head -1)
if [ -z "$GIT_TAG" ]; then
    GIT_HASH=$(git -C "$BASE_DIR" rev-parse --short HEAD 2>/dev/null || echo "custom")
    VER="${VER}-${GIT_HASH}"
fi

echo ""
echo "------------------------------------------------------------------------"
echo " YI-HACK - FIRMWARE PACKER"
echo "------------------------------------------------------------------------"
printf " camera_name      : %s\n" $CAMERA_NAME
printf " camera_id        : %s\n" $CAMERA_ID
printf " version          : %s\n" $VER
printf "                      \n"
printf " sysroot_dir      : %s\n" $SYSROOT_DIR
printf " static_dir       : %s\n" $STATIC_DIR
printf " build_dir        : %s\n" $BUILD_DIR
printf " out_dir          : %s\n" $OUT_DIR
echo "------------------------------------------------------------------------"
echo ""

echo -n ">>> Starting..."

sleep 1

echo -n ">>> Creating the out directory... "
mkdir -p $OUT_DIR
echo "${OUT_DIR} created!"

echo -n ">>> Creating the tmp directory... "
TMP_DIR=$(create_tmp_dir)
echo "${TMP_DIR} created!"
mkdir -p ${TMP_DIR}/yi-hack

# Copy the sysroot to the tmp dir
echo ">>> Copying the sysroot contents to ${TMP_DIR}... "
rsync -a ${SYSROOT_DIR}/* ${TMP_DIR}/ || exit 1
echo "    done!"

# Copy the build files to the tmp dir
echo -n ">>> Copying files from the build directory to ${TMP_DIR}... "
cp -R $BUILD_DIR/* $TMP_DIR/ || exit 1
echo "done!"

# adding defaults
echo -n ">>> Adding defaults... "
(cd $TMP_DIR/yi-hack/etc/ && tar jcvf $TMP_DIR/yi-hack/etc/defaults.tar.bz2 *.conf > /dev/null 2>&1)
echo "done!"

# insert the version file
echo -n ">>> Writing the version file... "
echo "$VER" > $TMP_DIR/yi-hack/version
echo "done!"

# insert the model suffix file
echo -n ">>> Creating the model suffix file... "
echo $CAMERA_ID > $TMP_DIR/yi-hack/model_suffix
echo "done!"

# fix the files ownership
echo -n ">>> Fixing the files ownership... "
chown -R root:root $TMP_DIR/*
echo "done!"

# Copy the sdhack to the output dir
echo ">>> Copying the sdhack contents to $TMP_DIR... "
echo "    Copying sdhack..."
rsync -a ${SDHACK_DIR}/${CAMERA_ID}/* $TMP_DIR || exit 1
echo "    done!"

# create tar.gz
rm -f $TMP_DIR/*.tgz
(cd $TMP_DIR && tar zcvf ${CAMERA_NAME}_${VER}.tgz *)

# copy files to the output dir
echo ">>> Copying files to $OUT_DIR... "
echo "    Copying files..."
cp $TMP_DIR/${CAMERA_NAME}_${VER}.tgz $OUT_DIR
echo "    done!"

# Cleanup
echo -n ">>> Cleaning up the tmp folder... "
rm -rf $TMP_DIR
echo "done!"

echo "------------------------------------------------------------------------"
echo " Finished!"
echo "------------------------------------------------------------------------"
