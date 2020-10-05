var APP = APP || {};

APP.status = (function ($) {

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
                    var aTag = $('<a>', {href: data[key]});
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
                    if (key != "uptime" && key != "total_memory" && key != "free_memory" && key != "wlan_strength") {
                        $('#' + key).text(data[key]);
                    }
                }

                $('#uptime').text(String.format("%t", parseInt(data.uptime)));
                $('#memory').text("" + data.free_memory + "/" + data.total_memory + " KB");
                wifiStrength = parseInt((parseInt(data.wlan_strength) * 100) / 70);
                if (wifiStrength >= 80) {
                    $('#wlan_strength').attr("src","images/wlan_strong_signal.png")
                    $('#wlan_strength_percent').text(" " + wifiStrength.toString() + " %")
                } else if (wifiStrength >= 40 && wifiStrength < 80) {
                    $('#wlan_strength').attr("src","images/wlan_medium_signal.png")
                    $('#wlan_strength_percent').text(" " + wifiStrength.toString() + " %")
                } else if (wifiStrength >= 0 && wifiStrength < 40) {
                    $('#wlan_strength').attr("src","images/wlan_weak_signal.png")
                    $('#wlan_strength_percent').text(" " + wifiStrength.toString() + " %")
                } else {
                    $('#wlan_strength').attr("src","images/wlan_no_signal.png")
                    $('#wlan_strength_percent').text(" No signal")
                }
            },
            error: function(response) {
                console.log('error', response);
            },
            complete: function () {
                clearTimeout(timeoutVar);
                timeoutVar = setTimeout(updateStatusPage, 10000);

            }
        });
    }

    return {
        init: init
    };

})(jQuery);
