#!/bin/sh

validateFile()
{
    if [ "Y${1:4:1}" != "YY" ] ; then
        DIR = "none"
    fi
    if [ "M${1:7:1}" != "MM" ] ; then
        DIR = "none"
    fi
    if [ "D${1:10:1}" != "DD" ] ; then
        DIR = "none"
    fi
    if [ "H${1:13:1}" != "HH" ] ; then
        DIR = "none"
    fi

    if [ "M${1:17:1}" != "MM" ] ; then
        FILE = "none"
    fi
    if [ "S${1:20:1}" != "SS" ] ; then
        FILE = "none"
    fi
}

case $QUERY_STRING in
    *[\'!\"@\#\$%^*\(\)_+,:\;]* ) exit;;
esac

FILE="none"

CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

if [ "$CONF" == "file" ] ; then
    FILE="$VAL"
fi

validateFile $FILE

if [ "$FILE" != "none" ] ; then
    rm -f /tmp/sd/record/$FILE
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "}"
