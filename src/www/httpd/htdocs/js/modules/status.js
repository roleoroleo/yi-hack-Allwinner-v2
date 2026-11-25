var APP = APP || {};

APP.status = (function($) {

    var timeoutVar;
    var wifiStrength;

    function init() {
        updateLinks();
        updateStatusPage();
    }

    function updateLinks() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/links.sh',
            dataType: "json",
            success: function(data) {
                for (let key in data) {
                    var aTag = $('<a>', {
                        href: data[key]
                    });
                    aTag.text(data[key]);
                    $('#td_' + key).text('').append(aTag);
                    $('#tr_' + key).show();
                }
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function updateStatusPage() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/status.json',
            dataType: "json",
            success: function(data) {
                for (let key in data) {
                    if (key != "wlan_essid" && key != "uptime" && key != "total_memory" && key != "free_memory" && key != "wlan_strength" && key != "active_if" && key != "eth_link" && key != "eth_speed" && key != "wifi_state") {
                        $('#' + key).text(data[key]);
                    }
                }

                $('#local_ip').html(data.local_ip + "<input class=\"button-primary\" type=\"button\" id=\"button-static-ip-edit\" value=\"Edit\" style=\"margin-left:40px;\" onclick=\"window.location.href='?page=static_ip'\"/>");
                $('#uptime').text(String.format("%t", parseInt(data.uptime)));
                $('#memory').text("" + data.free_memory + "/" + data.total_memory + " KB");

                var wifiEdit = "<input class=\"button-primary\" type=\"button\" id=\"button-wifi-edit\" value=\"Edit\" style=\"margin-left:40px;\" onclick=\"window.location.href='?page=wifi'\"/>";

                if (data.active_if == "eth0") {
                    $('#active_if').text("Ethernet (eth0)");
                    if (data.eth_link == "up") {
                        $('#link_info').text("Up" + (data.eth_speed ? " - " + data.eth_speed : ""));
                    } else {
                        $('#link_info').text("Down");
                    }
                    $('#wlan_essid').html("not in use" + wifiEdit);
                    $('#wlan_strength').hide();
                    $('#wlan_strength_percent').text(" n/a");
                } else if (data.active_if == "wlan0") {
                    $('#active_if').text("Wi-Fi (wlan0)");
                    if (data.wifi_state == "fallback") {
                        $('#link_info').text("Wi-Fi fallback");
                    } else if (data.wifi_state == "off") {
                        $('#link_info').text("Wi-Fi off");
                    } else {
                        $('#link_info').text("Wi-Fi");
                    }
                    $('#wlan_essid').html((data.wlan_essid || "not connected") + wifiEdit);
                    $('#wlan_strength').show();
                    wifiStrength = parseInt((parseInt(data.wlan_strength) * 100) / 70);
                    if (wifiStrength >= 80) {
                        $('#wlan_strength').attr("src", "img/wlan_strong_signal.png")
                        $('#wlan_strength_percent').text(" " + wifiStrength.toString() + " %")
                    } else if (wifiStrength >= 40 && wifiStrength < 80) {
                        $('#wlan_strength').attr("src", "img/wlan_medium_signal.png")
                        $('#wlan_strength_percent').text(" " + wifiStrength.toString() + " %")
                    } else if (wifiStrength >= 0 && wifiStrength < 40) {
                        $('#wlan_strength').attr("src", "img/wlan_weak_signal.png")
                        $('#wlan_strength_percent').text(" " + wifiStrength.toString() + " %")
                    } else {
                        $('#wlan_strength').attr("src", "img/wlan_no_signal.png")
                        $('#wlan_strength_percent').text(" No signal")
                    }
                } else {
                    $('#active_if').text("none");
                    $('#link_info').text("n/a");
                    $('#wlan_essid').html("not connected" + wifiEdit);
                    $('#wlan_strength').hide();
                    $('#wlan_strength_percent').text(" n/a");
                }
            },
            error: function(response) {
                console.log('error', response);
            },
            complete: function() {
                clearTimeout(timeoutVar);
                timeoutVar = setTimeout(updateStatusPage, 10000);

            }
        });
    }

    return {
        init: init
    };

})(jQuery);