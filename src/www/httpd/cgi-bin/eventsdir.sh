#!/bin/sh
# One awk process for the whole list instead of several forks (date, grep, cut) per folder.

CONF_FILE="etc/system.conf"
YI_HACK_PREFIX="/tmp/sd/yi-hack"

# Read the option once. Anything other than "local" is treated like "autodetect" and "gmt":
# the folder name is UTC, the displayed time is converted to the camera's timezone.
EVENTS_TIME=$(grep -w EVENTS_TIME $YI_HACK_PREFIX/$CONF_FILE | cut -d "=" -f2-)

printf "Content-type: application/json\r\n\r\n"
printf "{\"records\":[\n"

ls -r /tmp/sd/record | grep H | awk -v mode="$EVENTS_TIME" '
# Days since 1970-01-01 for a civil date (Howard Hinnant). Avoids a date(1) fork per folder.
function days(y, m, d,    era, yoe, mp, doy, doe) {
    if (m <= 2) y--
    era = int(y / 400)
    yoe = y - era * 400
    mp = (m + 9) % 12
    doy = int((153 * mp + 2) / 5) + d - 1
    doe = yoe * 365 + int(yoe / 4) - int(yoe / 100) + doy
    return era * 146097 + doe - 719468
}
length($0) == 14 {
    y = substr($0, 1, 4) + 0
    mo = substr($0, 6, 2) + 0
    d = substr($0, 9, 2) + 0
    h = substr($0, 12, 2) + 0
    if (mode == "local")
        e = mktime(y " " mo " " d " " h " 0 0")
    else
        e = days(y, mo, d) * 86400 + h * 3600
    if (n++)
        printf "},\n"
    printf "{\n\"datetime\":\"Date: %s\",\n\"dirname\":\"%s\"\n", strftime("%Y-%m-%d Time: %H:00", e), $0
}
END {
    if (n)
        printf "}\n"
}'

printf "]}\n"
