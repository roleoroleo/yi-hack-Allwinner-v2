#!/bin/sh

card_root="$1"
[ -d "$card_root/yi-hack" ] || exit 1
config="$card_root/yi-hack/etc/system.conf"
report="$card_root/YI-LOCAL-STATUS.txt"

previous_count=$(sed -n 's/^boot_count=//p' "$report" 2>/dev/null)
case "$previous_count" in
    ''|*[!0-9]*) previous_count=0 ;;
esac
boot_count=$((previous_count + 1))

write_report() {
    {
        printf 'status=%s\n' "$1"
        printf 'boot_count=%s\n' "$boot_count"
        printf 'model=y21ga\n'
        printf 'ip=%s\n' "$2"
        printf 'disable_cloud=%s\n' "$(sed -n 's/^DISABLE_CLOUD=//p' "$config")"
        printf 'rtsp_enabled=%s\n' "$(sed -n 's/^RTSP=//p' "$config")"
    } > "$report.tmp" && mv "$report.tmp" "$report" && sync
}

write_report startup_hook_reached ''
attempt=0
while [ "$attempt" -lt 30 ]; do
    ip=$(ifconfig wlan0 2>/dev/null | awk '
        $1 == "inet" {
            sub(/^addr:/, "", $2)
            if ($2 ~ /^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$/ && $2 != "0.0.0.0") {
                print $2
                exit
            }
        }
    ')
    if [ -n "$ip" ]; then
        write_report wifi_ipv4_assigned "$ip"
        exit
    fi
    attempt=$((attempt + 1))
    sleep 2
done

write_report no_wifi_ipv4_after_60s ''
