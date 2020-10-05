#!/bin/sh

validateDir()
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
}

case $QUERY_STRING in
    *[\'!\"@\#\$%^*\(\)_+.,:\;]* ) exit;;
esac

DIR="none"

CONF="$(echo $QUERY_STRING | cut -d'=' -f1)"
VAL="$(echo $QUERY_STRING | cut -d'=' -f2)"

if [ "$CONF" == "dir" ] ; then
    DIR="$VAL"
fi

if [ "$DIR" == "all" ]; then
    DIR="*"
else
    validateDir $DIR
fi

if [ "$DIR" != "none" ] ; then
    rm -rf /tmp/sd/record/$DIR
fi

printf "Content-type: application/json\r\n\r\n"

printf "{\n"
printf "}"
