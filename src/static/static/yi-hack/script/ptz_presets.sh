#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"
PTZ_CONF_FILE=$YI_HACK_PREFIX/etc/ptz_presets.conf

add_del_preset() {
    FIRST_AVAIL=8
    PRESET_0=""
    PRESET_1=""
    PRESET_2=""
    PRESET_3=""
    PRESET_4=""
    PRESET_5=""
    PRESET_6=""
    PRESET_7=""

    OLD_IFS="$IFS"
    IFS="=,"
    while read -r PNUM PNAME PX PY; do
        if [ -z $PNUM ]; then
            continue;
        fi
        if [ "${PNUM::1}" == "#" ]; then
            continue;
        fi
        if [ -z $PNAME ] && [ $FIRST_AVAIL -gt $PNUM ]; then
            FIRST_AVAIL=$PNUM
        fi
        if [ "$1" == "add" ] && [ "$PNAME" == "$2" ]; then
            return 1
        fi

        for I in 0 1 2 3 4 5 6 7; do
            if [ "$PNUM" == "$I" ]; then
                eval PRESET_$I=$PNAME,$PX,$PY
            fi
        done
    done < $PTZ_CONF_FILE

    IFS="$OLD_IFS"

    if [ "$1" == "add" ]; then
        if [ $FIRST_AVAIL -le 7 ]; then
            eval PRESET_$FIRST_AVAIL=$2,$3,$4
        else
            return 2
        fi
    fi

    if [ "$1" == "del" ]; then
        for I in 0 1 2 3 4 5 6 7; do
            if [ "$2" == "$I" ] || [ "$2" == "all" ]; then
                eval PRESET_$I=""
            fi
        done
    fi

    if [ "$1" == "add" ] || [ "$1" == "del" ]; then
        echo 0=$PRESET_0 > $PTZ_CONF_FILE
        echo 1=$PRESET_1 >> $PTZ_CONF_FILE
        echo 2=$PRESET_2 >> $PTZ_CONF_FILE
        echo 3=$PRESET_3 >> $PTZ_CONF_FILE
        echo 4=$PRESET_4 >> $PTZ_CONF_FILE
        echo 5=$PRESET_5 >> $PTZ_CONF_FILE
        echo 6=$PRESET_6 >> $PTZ_CONF_FILE
        echo 7=$PRESET_7 >> $PTZ_CONF_FILE
    fi
}

ACTION="none"
NUM=-1
NAME=""

while [[ $# -gt 0 ]]; do
  case $1 in
    -a|--action)
      ACTION="$2"
      shift # past argument
      shift # past value
      ;;
    -n|--number)
      NUM="$2"
      shift # past argument
      shift # past value
      ;;
    -m|--name)
      NAME="$2"
      shift # past argument
      shift # past value
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      shift # past argument
      ;;
  esac
done

if [ $ACTION == "get_presets" ] ; then
    cat $PTZ_CONF_FILE
elif [ $ACTION == "go_preset" ] ; then
    if [ $NUM -lt 0 ] || [ $NUM -gt 7 ] ; then
        echo "Preset number out of range"
        exit
    fi
    ipc_cmd -p $NUM
elif [ $ACTION == "add_preset" ]; then
    # add preset
    POS=$(ipc_cmd -g)
    if [ -z $POS ]; then
        echo "Error getting PTZ position"
        exit
    fi
    POSX=$(echo $POS | cut -d ',' -f 1)
    POSY=$(echo $POS | cut -d ',' -f 2)
    if [ -z $POSX ] || [ -z $POSY ]; then
        echo "Error getting PTZ position"
        exit
    fi

    add_del_preset add $NAME $POSX $POSY
    RES=$?
    if [ $RES -eq 1 ]; then
        echo "Preset already exists"
        exit
    fi
    if [ $RES -eq 2 ]; then
        echo "No free preset available"
        exit
    fi
    ipc_cmd -P $NAME
elif [ $ACTION == "del_preset" ]; then
    # del preset
    add_del_preset del $NUM
    ipc_cmd -R $NUM
elif [ $ACTION == "set_home_position" ]; then
    # set home position
    add_del_preset del 0
    add_del_preset add "Home"
    ipc_cmd -H
else
    echo "Invalid action received"
    exit
fi
