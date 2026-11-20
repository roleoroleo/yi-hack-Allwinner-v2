#!/bin/sh

CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"
MODEL_SUFFIX=$(cat /tmp/sd/yi-hack/model_suffix)

HOMEVER=$(cat /home/homever)
HV=${HOMEVER:0:2}
if [ "${HV:1:1}" == "." ]; then
    HV=${HV:0:1}
fi

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2-
}

TZ_TMP=$(get_config TIMEZONE)

# Enable time osd
$YI_HACK_PREFIX/bin/set_tz_offset -c osd -o on
# The Yi processes run with TZ=TIMEZONE (see system.sh), so the OSD already
# follows the local timezone: the extra offset must be 0. Re-applying the
# timezone offset here made the OSD one hour ahead.
TZP_SET=0
$YI_HACK_PREFIX/bin/set_tz_offset -c tz_offset_osd -m $MODEL_SUFFIX -f $HV -v $TZP_SET
