#!/usr/bin/env bash
# Deploy locally-edited yi-hack runtime scripts to a running camera over SSH.
# Works from Linux, macOS and Git Bash on Windows.
#
# Requirements on the camera: SSH must be enabled (web UI -> Configurations ->
# SSHD = yes) and an SSH password set. Only the plain-file parts of the build are
# pushed - the *.sh runtime scripts and the cgi-bin scripts. The rest of the web
# UI is gzipped at build time and must be deployed with a full firmware update.
#
# Usage:
#   scripts/deploy.sh <camera-ip> [--user root] [--reboot] [--dry-run]
#
# Examples:
#   scripts/deploy.sh 192.168.1.50            # push scripts + cgi-bin
#   scripts/deploy.sh 192.168.1.50 --reboot   # push, then reboot the camera
set -eu

CAM=""
USER_NAME="root"
DO_REBOOT=0
DRY_RUN=0

while [ $# -gt 0 ]; do
    case "$1" in
        --user)    USER_NAME="$2"; shift 2 ;;
        --reboot)  DO_REBOOT=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*)        echo "Unknown option: $1" >&2; exit 2 ;;
        *)         if [ -z "$CAM" ]; then CAM="$1"; else echo "Unexpected arg: $1" >&2; exit 2; fi; shift ;;
    esac
done

if [ -z "$CAM" ]; then
    echo "Usage: $0 <camera-ip> [--user root] [--reboot] [--dry-run]" >&2
    exit 2
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$SCRIPT_DIR")"
SRC_SCRIPTS="$REPO/src/static/static/yi-hack/script"
SRC_CGI="$REPO/src/www/httpd/cgi-bin"
DST_SCRIPTS="/tmp/sd/yi-hack/script"
DST_CGI="/tmp/sd/yi-hack/www/cgi-bin"

if [ ! -d "$SRC_SCRIPTS" ]; then
    echo "Cannot find $SRC_SCRIPTS - run this from inside the repo." >&2
    exit 1
fi

SSH_OPTS="-o StrictHostKeyChecking=accept-new -o ConnectTimeout=8"
run() { echo "+ $*"; [ "$DRY_RUN" = "1" ] || "$@"; }

echo "Deploying to $USER_NAME@$CAM"
echo "  scripts : $SRC_SCRIPTS/*.sh (+ mqtt_advertise) -> $DST_SCRIPTS/"
echo "  cgi-bin : $SRC_CGI/*                            -> $DST_CGI/"
echo

# Runtime shell scripts (top level + the mqtt_advertise helpers) and the CGI
# scripts are all plain files, so they can be copied straight onto the card.
run scp $SSH_OPTS "$SRC_SCRIPTS"/*.sh "$USER_NAME@$CAM:$DST_SCRIPTS/"
run scp $SSH_OPTS "$SRC_SCRIPTS"/mqtt_advertise/*.sh "$USER_NAME@$CAM:$DST_SCRIPTS/mqtt_advertise/"
run scp $SSH_OPTS "$SRC_CGI"/* "$USER_NAME@$CAM:$DST_CGI/"

# Keep everything executable (FAT32 loses the bit, but the camera re-applies it).
run ssh $SSH_OPTS "$USER_NAME@$CAM" "chmod +x $DST_SCRIPTS/*.sh $DST_SCRIPTS/mqtt_advertise/*.sh $DST_CGI/* 2>/dev/null; sync"

if [ "$DO_REBOOT" = "1" ]; then
    echo "Rebooting camera..."
    run ssh $SSH_OPTS "$USER_NAME@$CAM" "reboot" || true
    echo "Done. Camera is rebooting."
else
    echo
    echo "Done. CGI changes are live now; boot scripts (system.sh, wd.sh,"
    echo "check_conf.sh, enforce_settings.sh) take effect on the next reboot."
    echo "Re-run with --reboot to reboot the camera automatically."
fi
