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

###############################################################################
# Cameras list
###############################################################################

declare -A CAMERAS

CAMERAS["y21ga"]="y21ga"
CAMERAS["y211ga"]="y211ga"
CAMERAS["y211ba"]="y211ba"
CAMERAS["y213ga"]="y213ga"
CAMERAS["h30ga"]="h30ga"
CAMERAS["r30gb"]="r30gb"
CAMERAS["r35gb"]="r35gb"
CAMERAS["r37gb"]="r37gb"
CAMERAS["h52ga"]="h52ga"
CAMERAS["h51ga"]="h51ga"
CAMERAS["y28ga"]="y28ga"
CAMERAS["y29ga"]="y29ga"
CAMERAS["y291ga"]="y291ga"
CAMERAS["y623"]="y623"
CAMERAS["r40ga"]="r40ga"
CAMERAS["h60ga"]="h60ga"
CAMERAS["q321br_lsx"]="q321br_lsx"
CAMERAS["qg311r"]="qg311r"
CAMERAS["q705br"]="q705br"
CAMERAS["b091qp"]="b091qp"

###############################################################################
# Common functions
###############################################################################

require_root()
{
    if [ "$(whoami)" != "root" ]; then
        echo "$0 must be run as root!"
        exit 1
    fi
}

normalize_path()
{
    local path=${1//\/.\//\/}
    local npath=$(echo $path | sed -e 's;[^/][^/]*/\.\./;;')
    while [[ $npath != $path ]]; do
        path=$npath
        npath=$(echo $path | sed -e 's;[^/][^/]*/\.\./;;')
    done
    echo $path
}

check_camera_name()
{
    local CAMERA_NAME=$1
    if [[ ! ${CAMERAS[$CAMERA_NAME]+_} ]]; then
        printf "%s not found.\n\n" $CAMERA_NAME

        printf "Here's the list of supported cameras:\n\n"
        print_cameras_list 
        printf "\n"
        exit 1
    fi
}

print_cameras_list()
{
    for CAMERA_NAME in "${!CAMERAS[@]}"; do 
        printf "%s\n" $CAMERA_NAME
    done
}

get_camera_id()
{
    local CAMERA_NAME=$1
    check_camera_name $CAMERA_NAME
    echo ${CAMERAS[$CAMERA_NAME]}
}
