#!/bin/sh

YI_HACK_PREFIX="/tmp/sd/yi-hack"

. $YI_HACK_PREFIX/www/cgi-bin/validate.sh

if ! $(validateQueryString $QUERY_STRING); then
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

BASE64="no"
RES="-r high"
WATERMARK="no"
OUTPUT_FILE="none"
MODEL=$(cat /tmp/sd/yi-hack/model_suffix)

for I in 1 2 3 4
do
    CONF="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f1)"
    VAL="$(echo $QUERY_STRING | cut -d'&' -f$I | cut -d'=' -f2)"

    if [ "$CONF" == "res" ] ; then
        if [ "$VAL" == "low" ] || [ "$VAL" == "high" ] ; then
            RES="-r $VAL"
        fi
    elif [ "$CONF" == "watermark" ] ; then
        if [ "$VAL" == "yes" ] || [ "$VAL" == "no" ] ; then
            WATERMARK=$VAL
        fi
    elif [ "$CONF" == "base64" ] ; then
        if [ "$VAL" == "yes" ] || [ "$VAL" == "no" ] ; then
            BASE64=$VAL
        fi
    elif [ "$CONF" == "file" ] ; then
        OUTPUT_FILE=$VAL
    fi
done

REDIRECT=""
if $(validateBaseName $OUTPUT_FILE); then
    if [ "$OUTPUT_FILE" != "none" ] ; then
        OUTPUT_DIR=$(cd "$(dirname "/tmp/sd/record/$OUTPUT_FILE")"; pwd)
        OUTPUT_DIR=$(echo "$OUTPUT_DIR" | cut -c1-14)
        if [ "$OUTPUT_DIR" == "/tmp/sd/record" ]; then
            REDIRECT="yes"
        fi
    fi
fi

if [ "$WATERMARK" == "yes" ] ; then
    WATERMARK="-w"
elif [ "$WATERMARK" == "no" ] ; then
    WATERMARK=""
fi

if [ "$REDIRECT" == "yes" ] ; then
    if [ "$BASE64" == "no" ] ; then
        imggrabber -m $MODEL $RES $WATERMARK > /tmp/sd/record/$OUTPUT_FILE
    elif [ "$BASE64" == "yes" ] ; then
        imggrabber -m $MODEL $RES $WATERMARK | base64 > /tmp/sd/record/$OUTPUT_FILE
    fi
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "false"
    printf "}"
else
    if [ "$BASE64" == "no" ] ; then
        printf "Content-type: image/jpeg\r\n\r\n"
        imggrabber -m $MODEL $RES $WATERMARK
    elif [ "$BASE64" == "yes" ] ; then
        printf "Content-type: image/jpeg;base64\r\n\r\n"
        imggrabber -m $MODEL $RES $WATERMARK | base64
    fi
fi
