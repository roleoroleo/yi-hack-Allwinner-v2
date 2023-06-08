#!/bin/ash
#
# Command line:
# 	ash "/tmp/sd/yi-hack/script/thumb.sh" cron
# 	ash "/tmp/sd/yi-hack/script/thumb.sh" start
# 	ash "/tmp/sd/yi-hack/script/thumb.sh" stop
#
CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"

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
FOLDER_TO_WATCH="/tmp/sd/record"
FOLDER_MINDEPTH="1"
FILE_WATCH_PATTERN="*.mp4"
SLEEP_CYCLE_SECONDS="45"
#
# Runtime Variables.
SCRIPT_FULLFN="thumb.sh"
SCRIPT_NAME="thumb"
LOGFILE="/tmp/${SCRIPT_NAME}.log"
LOG_MAX_LINES="200"


#
# -----------------------------------------------------
# -------------- START OF FUNCTION BLOCK --------------
# -----------------------------------------------------


lbasename ()
{
	echo ${1:0:$((${#1} - 4))}
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


lstat ()
{
	if [ -d "${1}" ]; then
		ls -a -l -td "${1}" | awk '{k=0;for(i=0;i<=8;i++)k+=((substr($1,i+2,1)~/[rwx]/) \
				 *2^(8-i));if(k)printf("%0o ",k);print}' | \
				 cut -d " " -f 1
	else
		ls -a -l "${1}" | awk '{k=0;for(i=0;i<=8;i++)k+=((substr($1,i+2,1)~/[rwx]/) \
				 *2^(8-i));if(k)printf("%0o ",k);print}' | \
				 cut -d " " -f 1
	fi
}


checkFiles ()
{
	#
	logAdd "[INFO] checkFiles"
	#
	# Search for new files.
	if [ -f "/usr/bin/sort" ] || [ -f "/tmp/sd/yi-hack/usr/bin/sort" ]; then
		# Default: Optimized for busybox
		L_FILE_LIST="$(find "${FOLDER_TO_WATCH}" -mindepth ${FOLDER_MINDEPTH} -type f \( -name "${FILE_WATCH_PATTERN}" \) | sort -k 1 -n)"
	else
		# Alternative: Unsorted output
		L_FILE_LIST="$(find "${FOLDER_TO_WATCH}" -mindepth ${FOLDER_MINDEPTH} -type f \( -name "${FILE_WATCH_PATTERN}" \))"
	fi
	if [ -z "${L_FILE_LIST}" ]; then
		return 0
	fi
	#
	echo "${L_FILE_LIST}" | while read file; do
		BASE_NAME=$(lbasename "$file")
		if [ ! -f $BASE_NAME.jpg ]; then
			minimp4_yi -t 1 $file $BASE_NAME.h26x
			if [ $? -ne 0 ]; then
				logAdd "[ERROR] checkFiles: demux mp4 FAILED - [${file}]."
				rm -f $BASE_NAME.h26x
			fi
			imggrabber -f $BASE_NAME.h26x -r low -w > $BASE_NAME.jpg
			if [ $? -ne 0 ]; then
				logAdd "[ERROR] checkFiles: create jpg FAILED - [${file}]. Using fallback.jpg."
				rm -f $BASE_NAME.h26x
				rm -f $BASE_NAME.jpg
				cp /tmp/sd/yi-hack/etc/fallback.jpg $BASE_NAME.jpg
			fi
			rm -f $BASE_NAME.h26x
			logAdd "[INFO] checkFiles: createThumb SUCCEEDED - [${file}]."
			sync
		else
			#logAdd "[INFO] checkFiles: ignore file [${file}] - already present."
			if [ -s $BASE_NAME.jpg ]; then
				logAdd "[INFO] checkFiles: ignore file [${file}] - already present."
			else
				rm -f $BASE_NAME.jpg
				logAdd "[DEBUG] checkFiles: ignore file [${file}] - already present but ZERO, deleted for next try"
			fi
		fi
		#
	done
	#
	return 0
}


serviceMain ()
{
	#
	# Usage:		serviceMain	[--one-shot]
	# Called By:	MAIN
	#
	logAdd "[INFO] === SERVICE START ==="
	# sleep 10
	while (true); do
		# Check if folder exists.
		if [ ! -d "${FOLDER_TO_WATCH}" ]; then
			mkdir -p "${FOLDER_TO_WATCH}"
		fi
		#
		# Ensure correct file permissions.
		if ( ! lstat "${FOLDER_TO_WATCH}/" | grep -q "^755$" ); then
			logAdd "[WARN] Adjusting folder permissions to 0755 ..."
			chmod -R 0755 "${FOLDER_TO_WATCH}"
		fi
		#
		checkFiles
		#
		if [ "${1}" = "--one-shot" ]; then
			break
		fi
		#
		sleep ${SLEEP_CYCLE_SECONDS}
	done
	return 0
}
# ---------------------------------------------------
# -------------- END OF FUNCTION BLOCK --------------
# ---------------------------------------------------
#
# set +m
trap "" SIGHUP
#
if [ "${1}" = "cron" ]; then
	RUNNING=$(ps ww | grep $SCRIPT_FULLFN | grep -v grep | grep /bin/sh | awk 'END { print NR }')
	if [ $RUNNING -gt 1 ]; then
		logAdd "[INFO] === SERVICE ALREADY RUNNING ==="
		exit 0
	fi
	serviceMain --one-shot
	logAdd "[INFO] === SERVICE STOPPED ==="
	exit 0
elif [ "${1}" = "start" ]; then
	RUNNING=$(ps ww | grep $SCRIPT_FULLFN | grep -v grep | grep /bin/sh | awk 'END { print NR }')
	if [ $RUNNING -gt 1 ]; then
		logAdd "[INFO] === SERVICE ALREADY RUNNING ==="
		exit 0
	fi
	serviceMain &
	#
	# Wait for kill -INT.
	wait
	exit 0
elif [ "${1}" = "stop" ]; then
	ps ww | grep -v grep | grep "ash ${0}" | sed 's/ \+/|/g' | sed 's/^|//' | cut -d '|' -f 1 | grep -v "^$$" | while read pidhandle; do
		echo "[INFO] Terminating old service instance [${pidhandle}] ..."
		kill -9 "${pidhandle}"
	done
	#
	# Check if parts of the service are still running.
	if [ "$(ps ww | grep -v grep | grep "ash ${0}" | sed 's/ \+/|/g' | sed 's/^|//' | cut -d '|' -f 1 | grep -v "^$$" | grep -c "^")" -gt 1 ]; then
		logAdd "[ERROR] === SERVICE FAILED TO STOP ==="
		exit 99
	fi
	logAdd "[INFO] === SERVICE STOPPED ==="
	exit 0
fi
#
logAdd "[ERROR] Parameter #1 missing."
logAdd "[INFO] Usage: ${SCRIPT_FULLFN} {cron|start|stop}"
exit 99
