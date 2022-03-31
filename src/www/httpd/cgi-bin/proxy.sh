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

PARAM="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f1)"
VALUE="$(echo $QUERY_STRING | cut -d'&' -f1 | cut -d'=' -f2)"

TEST_WITH_PROXY="0"
if [ "$PARAM" == "proxy" ]; then
    if [ "$VALUE" == "1" ]; then
        TEST_WITH_PROXY="1"
    fi
else
    printf "Content-type: application/json\r\n\r\n"
    printf "{\n"
    printf "\"%s\":\"%s\"\\n" "error" "true"
    printf "}"
    exit
fi

if [ "$TEST_WITH_PROXY" == "1" ]; then
    RES=$(IFS='' proxychains4 wget -O- -q http://ipinfo.io)
else
    RES=$(IFS='' wget -O- -q http://ipinfo.io)
fi

printf "Content-type: application/json\r\n\r\n"
printf "{\n"
printf "\"%s\":\"%s\",\\n" "error" "false"
printf "\"%s\":%s\\n" "result" "$RES"
printf "}"
