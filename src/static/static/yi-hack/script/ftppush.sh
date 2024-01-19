#!/bin/ash
#
# Command line:
# 	ash "/tmp/sd/yi-hack/script/ftppush.sh" cron
# 	ash "/tmp/sd/yi-hack/script/ftppush.sh" start
# 	ash "/tmp/sd/yi-hack/script/ftppush.sh" stop
#
CONF_FILE="etc/system.conf"

YI_HACK_PREFIX="/tmp/sd/yi-hack"

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
FOLDER_TO_WATCH="/tmp/sd/record"
FOLDER_MINDEPTH="1"
FILE_WATCH_PATTERN="*.mp4"
SKIP_UPLOAD_TO_FTP="0"
SLEEP_CYCLE_SECONDS="45"
#
# Runtime Variables.
SCRIPT_FULLFN="ftppush.sh"
SCRIPT_NAME="ftppush"
LOGFILE="/tmp/${SCRIPT_NAME}.log"
LOG_MAX_LINES="200"
LAST_FILE_SENT_FILE="/tmp/last_file_sent"
LAST_FILE_SENT="1970-01-01T00:00"
if [ -f ${LAST_FILE_SENT_FILE} ]; then
	LAST_FILE_SENT=$(cat /tmp/last_file_sent)
fi
echo $LAST_FILE_SENT > ${LAST_FILE_SENT_FILE}

#
# -----------------------------------------------------
# -------------- START OF FUNCTION BLOCK --------------
# -----------------------------------------------------
checkFiles ()
{
	#
	FTP_FILE_DELETE_AFTER_UPLOAD="$(get_config FTP_FILE_DELETE_AFTER_UPLOAD)"
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
		if [ "${#file}" == "44" ]; then
			FILE_DATE=${file:15:4}-${file:20:2}-${file:23:2}T${file:26:2}:${file:32:2}
		else
			FILE_DATE=${file:15:4}-${file:20:2}-${file:23:2}T${file:26:2}:${file:30:2}
		fi
		FILE_YEAR=${FILE_DATE:0:4}
		FILE_REMPART=${FILE_DATE:5:2}${FILE_DATE:8:2}${FILE_DATE:11:2}${FILE_DATE:14:2}
		LAST_FILE_SENT=$(cat /tmp/last_file_sent)
		LAST_FILE_SENT_YEAR=${LAST_FILE_SENT:0:4}
		LAST_FILE_SENT_REMPART=${LAST_FILE_SENT:5:2}${LAST_FILE_SENT:8:2}${LAST_FILE_SENT:11:2}${LAST_FILE_SENT:14:2}
		if [ ${FILE_YEAR} -gt ${LAST_FILE_SENT_YEAR} ] || ( [ ${FILE_YEAR} -eq ${LAST_FILE_SENT_YEAR} ] && [ ${FILE_REMPART} -gt ${LAST_FILE_SENT_REMPART} ] ); then
			if ( ! uploadToFtp -- "${file}" ); then
				logAdd "[ERROR] checkFiles: uploadToFtp FAILED - [${file}]. Retrying in ${SLEEP_CYCLE_SECONDS} s."
				return 0
			fi
			logAdd "[INFO] checkFiles: uploadToFtp SUCCEEDED - [${file}]."
			LAST_FILE_SENT=${FILE_DATE}
			echo $LAST_FILE_SENT > ${LAST_FILE_SENT_FILE}
			sync
			if [ "${FTP_FILE_DELETE_AFTER_UPLOAD}" == "yes" ]; then
				FBASENAME="$(fbasename ${file})"
				rm -f $FBASENAME.mp4
				rm -f $FBASENAME.jpg
			fi
		else
			logAdd "[INFO] checkFiles: ignore file [${file}] - already sent."
		fi
		#
	done
	#
	# Delete empty sub directories
	if [ ! -z "${FOLDER_TO_WATCH}" ]; then
		for d in $(find "${FOLDER_TO_WATCH}/" -mindepth 1 -type d); do
			#find "${FOLDER_TO_WATCH}/" -mindepth 1 -type d -empty -delete
			[ -z "`find $d -type f`" ] && rmdir $d
		done
	fi
	#
	return 0
}


fbasename ()
{
	echo ${1:0:$((${#1} - 4))}
}


lbasename ()
{
	echo "${1}" | sed "s/.*\///"
}


lparentdir ()
{
	echo "${1}" | xargs -I{} dirname {}| grep -o '[^/]*$'
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


translateFULLFNWithTz ()
{
	TIMEZONE=$(get_config TIMEZONE)
	TR_PREFIX=${1:0:15}
	TR_DATE=""
	TR_HOUR_PREFIX=""
	TR_SUFFIX=""
	if [ "${#1}" == "44" ]; then
		TR_DATE="${1:15:4}-${1:20:2}-${1:23:2} ${1:26:2}:${1:32:2}:${1:35:2}"
		TR_HOUR_PREFIX=${1:30:2}
		TR_SUFFIX=${1:38:6}
	else
		TR_DATE="${1:15:4}-${1:20:2}-${1:23:2} ${1:26:2}:${1:30:2}:${1:33:2}"
		TR_SUFFIX=${1:36:6}
	fi
	TR_SECONDS_1970=$(date +%s -u -d "$TR_DATE")
	TR_RET=$(TZ=$TIMEZONE date +$TR_PREFIX%YY%mM%dD%HH/$TR_HOUR_PREFIX%MM%SS$TR_SUFFIX -d "@$TR_SECONDS_1970")
	echo $TR_RET
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
	# Consts.
	FTP_HOST="$(get_config FTP_HOST)"
	FTP_DIR="$(get_config FTP_DIR)"
	FTP_DIR_TREE="$(get_config FTP_DIR_TREE)"
	FTP_USERNAME="$(get_config FTP_USERNAME)"
	FTP_PASSWORD="$(get_config FTP_PASSWORD)"
	#
	# Variables.
	UTF_FULLFN="${2}"
	FTP_DIR_HOUR="$(lparentdir ${UTF_FULLFN})"
	#
	if [ "${SKIP_UPLOAD_TO_FTP}" = "1" ]; then
		logAdd "[INFO] uploadToFtp skipped due to SKIP_UPLOAD_TO_FTP == 1."
		return 1
	fi
	#
	if [ ! -z "${FTP_DIR}" ]; then
		# Create directory on FTP server
		echo -e "USER ${FTP_USERNAME}\r\nPASS ${FTP_PASSWORD}\r\nmkd ${FTP_DIR}\r\nquit\r\n" | nc -w 5 ${FTP_HOST} 21 | grep "${FTP_DIR}"
		FTP_DIR="${FTP_DIR}/"
	fi
	#
	if [ "${FTP_DIR_TREE}" == "yes" ]; then
		if [ ! -z "${FTP_DIR_HOUR}" ]; then
			# Create hour directory on FTP server
			echo -e "USER ${FTP_USERNAME}\r\nPASS ${FTP_PASSWORD}\r\nmkd ${FTP_DIR}/${FTP_DIR_HOUR}\r\nquit\r\n" | nc -w 5 ${FTP_HOST} 21 | grep "${FTP_DIR_HOUR}"
			FTP_DIR_HOUR="${FTP_DIR_HOUR}/"
		fi
	fi
	#
	if [ ! -f "${UTF_FULLFN}" ]; then
		echo "[ERROR] uploadToFtp: File not found."
		return 1
	fi
	#
	if [ "${FTP_DIR_TREE}" == "yes" ]; then
		if ( ! ftpput -u "${FTP_USERNAME}" -p "${FTP_PASSWORD}" "${FTP_HOST}" "${FTP_DIR}${FTP_DIR_HOUR}$(lbasename "${UTF_FULLFN}")" "${UTF_FULLFN}" ); then
			logAdd "[ERROR] uploadToFtp: ftpput FAILED."
			return 1
		fi
	else
		if ( ! ftpput -u "${FTP_USERNAME}" -p "${FTP_PASSWORD}" "${FTP_HOST}" "${FTP_DIR}$(lbasename "${UTF_FULLFN}")" "${UTF_FULLFN}" ); then
			logAdd "[ERROR] uploadToFtp: ftpput FAILED."
			return 1
		fi
	fi
	#
	# Return SUCCESS.
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
		if [[ $(get_config FTP_UPLOAD) == "yes" ]] ; then
			checkFiles
		fi
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
