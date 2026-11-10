#!/bin/sh

# Live stream for the web interface (see js/modules/live.js).
#
# Usage:
#   cgi-bin/live.sh?res=low|high   video: raw H.264/H.265 Annex-B stream
#   cgi-bin/live.sh?audio=yes      audio: raw AAC ADTS stream
#
# The streams come from h264grabber. The browser wraps them into fragmented
# MP4 and plays them through Media Source Extensions.
# h264grabber can't send video and audio together to stdout, so the browser
# opens two connections when the audio is enabled.

YI_HACK_PREFIX="/tmp/sd/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

RES="low"
AUDIO="no"
MODEL=$(cat $YI_HACK_PREFIX/model_suffix)

for I in 1 2 3
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "res" ] ; then
        if [ "$VAL" == "low" ] || [ "$VAL" == "high" ] ; then
            RES=$VAL
        fi
    elif [ "$CONF" == "audio" ] ; then
        if [ "$VAL" == "yes" ] || [ "$VAL" == "no" ] ; then
            AUDIO=$VAL
        fi
    fi
done

# Web streams are recognized by the argument order "-r RES -m MODEL"
# (rtsp/go2rtc daemons use "-m MODEL ... -r RES"): keep it below.
if [ "$AUDIO" == "yes" ] ; then
    PATTERN="h264grabber -r none -m"
else
    PATTERN="h264grabber -r (low|high) -m"
fi

# Limit the number of concurrent web viewers to save cam resources
MAX_VIEWERS=3
VIEWERS=$(ps | grep -E "$PATTERN" | grep -v grep | grep -c ^)
if [ $VIEWERS -ge $MAX_VIEWERS ]; then
    # busybox httpd forwards a CGI response that starts with "HTTP" as is
    printf "HTTP/1.0 503 Service Unavailable\r\n"
    printf "Content-type: text/plain\r\n\r\n"
    printf "Too many live viewers\n"
    exit
fi

if [ "$AUDIO" == "yes" ] ; then
    printf "Content-type: audio/aac\r\n"
else
    printf "Content-type: video/h264\r\n"
fi
printf "Cache-Control: no-cache, no-store, must-revalidate\r\n"
printf "Pragma: no-cache\r\n"
printf "\r\n"

# exec: h264grabber replaces the shell, so when the browser closes the
# connection httpd exits, the pipe breaks and h264grabber terminates.
if [ "$AUDIO" == "yes" ] ; then
    exec h264grabber -r none -m $MODEL -a 2>/dev/null
else
    exec h264grabber -r $RES -m $MODEL 2>/dev/null
fi
