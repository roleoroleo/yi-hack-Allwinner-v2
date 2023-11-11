#!/bin/ash
CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"

MODEL_SUFFIX=$(cat $YI_HACK_PREFIX/model_suffix)
HOMEVER=$(cat /home/homever)
HV=${HOMEVER:0:2}

get_config()
{
    key=$1
    grep -w $1 $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2
}
# Setup env.
export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib
#
# Script Configuration.
FOLDER_TO_SAVE="/tmp/sd/record/timelapse"
#
# Runtime Variables.
SCRIPT_NAME="time_lapse"
LOGFILE="/tmp/${SCRIPT_NAME}.log"
LOG_MAX_LINES="200"

#
# -----------------------------------------------------
# -------------- START OF FUNCTION BLOCK --------------
# -----------------------------------------------------
lbasename ()
{
    echo "${1}" | sed "s/.*\///"
}



logAdd ()
{
    TMP_DATETIME="$(date '+%Y-%m-%d [%H-%M-%S]')"
    TMP_LOGSTREAM="$(tail -n ${LOG_MAX_LINES} ${LOGFILE} 2>/dev/null)"
    echo "${TMP_LOGSTREAM}" > "$LOGFILE"
    echo "${TMP_DATETIME} $*" >> "${LOGFILE}"
    echo "${TMP_DATETIME} $*"
    return 0
}



uploadToFtp ()
{
    #
    # Usage:			uploadToFtp -- "[FULLFN]"
    # Example:			uploadToFtp -- "/tmp/test.txt"
    # Purpose:
    # 	Uploads file to FTP
    #
    # Returns:
    # 	"0" on SUCCESS
    # 	"1" on FAILURE
    #
    # Consts:
    FTP_HOST="$(get_config FTP_HOST)"
    FTP_DIR="$(get_config FTP_DIR)"
    FTP_USERNAME="$(get_config FTP_USERNAME)"
    FTP_PASSWORD="$(get_config FTP_PASSWORD)"
    #
    # Variables:
    UTF_FULLFN="${2}"
    if [ ! -z "${FTP_DIR}" ]; then
        # Create directory on FTP server
        echo -e "USER ${FTP_USERNAME}\r\nPASS ${FTP_PASSWORD}\r\nmkd ${FTP_DIR}\r\nquit\r\n" | nc -w 5 ${FTP_HOST} 21 | grep "${FTP_DIR}"
        FTP_DIR="${FTP_DIR}/timelapse"
        echo -e "USER ${FTP_USERNAME}\r\nPASS ${FTP_PASSWORD}\r\nmkd ${FTP_DIR}\r\nquit\r\n" | nc -w 5 ${FTP_HOST} 21 | grep "${FTP_DIR}"
        FTP_DIR="${FTP_DIR}/"
    fi
    #
    if ( ! ftpput -u "${FTP_USERNAME}" -p "${FTP_PASSWORD}" "${FTP_HOST}" "${FTP_DIR}$(lbasename "${UTF_FULLFN}")" "${UTF_FULLFN}" ); then
        logAdd "[ERROR] uploadToFtp: ftpput FAILED."
        return 1
    fi
    #
    # Return SUCCESS.
    return 0
}

# ---------------------------------------------------
# -------------- END OF FUNCTION BLOCK --------------
# ---------------------------------------------------
#

logAdd "[INFO] === STARTING TIME_LAPSE.SH ==="

if [[ $(get_config SNAPSHOT_LOW) == "no" ]] ; then
    RES="high"
else
    RES="low"
fi

TZ_LOCAL=$(get_config TIMEZONE)
if [ -z $TZ_LOCAL ]; then
    DATE_LOCAL=$(date +%Y-%m-%d_%H-%M-00)
else
    DATE_LOCAL=$(TZ=$TZ_LOCAL date +%Y-%m-%d_%H-%M-00)
fi

logAdd "[INFO] starting capture."
mkdir -p $FOLDER_TO_SAVE
imggrabber -m $MODEL_SUFFIX -r $RES -w > $FOLDER_TO_SAVE/$DATE_LOCAL.jpg

if [ -s $FOLDER_TO_SAVE/$DATE_LOCAL.jpg ]; then
    logAdd "[INFO] capture completed."

    if [ "$1" == "yes" ] ; then
        if [[ $(get_config TIMELAPSE_FTP_SAME_NAME) == "yes" ]] ; then
            mv $FOLDER_TO_SAVE/$DATE_LOCAL.jpg $FOLDER_TO_SAVE/snapshot.jpg
            FILE_TO_PUSH=$FOLDER_TO_SAVE/snapshot.jpg
        else
            FILE_TO_PUSH=$FOLDER_TO_SAVE/$DATE_LOCAL.jpg
        fi

        logAdd "[INFO] starting ftp put."
        if ( ! uploadToFtp -- $FILE_TO_PUSH ); then
            logAdd "[ERROR] uploadToFtp FAILED - [$FILE_TO_PUSH]."
        else
            rm -f $FILE_TO_PUSH
            logAdd "[INFO] ftp put completed and local file removed."
        fi
    fi
else
    logAdd "[ERROR] capture error."
fi


logAdd "[INFO] === TIME_LAPSE.SH COMPLETED ==="
