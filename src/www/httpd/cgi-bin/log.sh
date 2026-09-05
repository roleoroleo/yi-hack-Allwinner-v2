#!/bin/sh

# Serve the persistent hack logs as plain text so they can be read straight from
# a browser at  http://<cam>/cgi-bin/log.sh  even before the HTML UI is rebuilt.
# Protected by the same httpd basic auth as the rest of the site.
#
# hack_debug.log is deliberately NOT exposed here: when DEBUG_LOG=yes it dumps ps
# output, which can contain the RTSP password from the daemon command line. The
# event log and the wifi failsafe log never contain secrets.

YI_HACK_PREFIX="/tmp/sd/yi-hack"
LOG_DIR="/tmp/sd/log"
TAIL="$YI_HACK_PREFIX/usr/bin/tail"

printf "Content-type: text/plain\r\n\r\n"

# How many lines to show (numeric, capped) and which log to show (whitelisted).
LINES=$(echo "$QUERY_STRING" | tr '&' '\n' | grep '^lines=' | head -n1 | cut -d= -f2)
case "$LINES" in ''|*[!0-9]*) LINES=200 ;; esac
[ "$LINES" -gt 2000 ] && LINES=2000

WHICH=$(echo "$QUERY_STRING" | tr '&' '\n' | grep '^log=' | head -n1 | cut -d= -f2)
case "$WHICH" in wifi) WHICH=wifi ;; *) WHICH=event ;; esac

echo "host: $(hostname)   time: $(date)"

if [ "$WHICH" = "wifi" ]; then
    echo "===== hack_wififailsafe.log (last $LINES lines) ====="
    if [ -f /tmp/sd/hack_wififailsafe.log ]; then
        $TAIL -n "$LINES" /tmp/sd/hack_wififailsafe.log
    else
        echo "(no wifi log yet)"
    fi
    exit 0
fi

echo "===== yi-hack event log (last $LINES lines) ====="
LATEST=$(ls -1 "$LOG_DIR"/hack-*.log 2>/dev/null | $TAIL -n 2)
if [ -z "$LATEST" ]; then
    echo "(no event log yet - $LOG_DIR is empty)"
else
    cat $LATEST | $TAIL -n "$LINES"
fi
