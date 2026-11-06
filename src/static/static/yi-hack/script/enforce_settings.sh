#!/bin/sh

# Re-assert ("persist") camera-side settings that the native Yi firmware or the
# official app can silently change behind the hack's back. It runs once at boot
# (arg: boot) and hourly from cron (arg: cron).
#
# At boot we apply the configured state unconditionally so the camera comes up
# the way the user asked. From cron we only touch a setting when its matching
# *_PERSIST flag is "yes", so someone who changes something in the web UI is not
# fought every hour unless they explicitly asked us to hold the line.

YI_HACK_PREFIX="/tmp/sd/yi-hack"
SYSTEM_CONF_FILE="$YI_HACK_PREFIX/etc/system.conf"
CAMERA_CONF_FILE="$YI_HACK_PREFIX/etc/camera.conf"
LOG_DIR="/tmp/sd/log"

export PATH=/usr/bin:/usr/sbin:/bin:/sbin:/home/base/tools:/home/app/localbin:/home/base:/tmp/sd/yi-hack/bin:/tmp/sd/yi-hack/sbin:/tmp/sd/yi-hack/usr/bin:/tmp/sd/yi-hack/usr/sbin
export LD_LIBRARY_PATH=/lib:/usr/lib:/home/lib:/home/qigan/lib:/home/app/locallib:/tmp/sd:/tmp/sd/gdb:/tmp/sd/yi-hack/lib

MODE="$1"
[ -z "$MODE" ] && MODE="cron"

# Anchored, first-match lookups so a stray duplicate line can never turn a value
# into a multi-line string (which breaks every downstream string comparison).
get_config()
{
    grep "^$1=" "$SYSTEM_CONF_FILE" 2>/dev/null | head -n1 | cut -d= -f2-
}

get_camera_config()
{
    grep "^$1=" "$CAMERA_CONF_FILE" 2>/dev/null | head -n1 | cut -d= -f2-
}

log()
{
    mkdir -p "$LOG_DIR"
    echo "$(date '+%Y-%m-%d %H:%M:%S') enforce($MODE): $1" >> "$LOG_DIR/hack-$(date +%Y%m%d).log"
}

# --- Save-on-motion mode (item 5) --------------------------------------------
# yes -> record only on motion (ipc_cmd -v detect)
# no  -> record continuously   (ipc_cmd -v always)
# There is no "record nothing" here; that is what CAMERA_RECORDING below is for.
SVOM=$(get_camera_config SAVE_VIDEO_ON_MOTION)
SVOM_PERSIST=$(get_camera_config SAVE_VIDEO_ON_MOTION_PERSIST)
# Only touch the live Yi state when the user opted into persistence; otherwise
# leave whatever they last set (via the app or the camera page) alone.
if [ "$SVOM_PERSIST" = "yes" ]; then
    if [ "$SVOM" = "no" ]; then
        ipc_cmd -v always && log "save-on-motion enforced: always (continuous)"
    else
        ipc_cmd -v detect && log "save-on-motion enforced: detect (on motion)"
    fi
fi

# --- Camera-side recording to SD (item 6) ------------------------------------
# no -> the mp4record writer must not run; Frigate records from RTSP instead.
# A plain "no" is already honoured at boot by system.sh not starting mp4record,
# so here we only need to undo it if something (the native app) restarted it.
CAMREC=$(get_config CAMERA_RECORDING)
CAMREC_PERSIST=$(get_config CAMERA_RECORDING_PERSIST)
if [ "$CAMREC" = "no" ]; then
    if [ "$MODE" = "boot" ] || [ "$CAMREC_PERSIST" = "yes" ]; then
        if ps | grep -v grep | grep -q mp4record; then
            killall mp4record 2>/dev/null && log "camera recording disabled: stopped mp4record"
        fi
    fi
fi
