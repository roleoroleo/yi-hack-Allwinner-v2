var APP = APP || {};

APP.wifi = (function($) {

    function init() {
        $('#input-container').hide();
        registerEventHandler();
        updateWiFiPage();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save-wifi', function(e) {
            saveWiFi();
        });
        $(document).on("change", '#WIFI_ESSID', function(e) {
            toggleESSIDInput();
        });
    }

    function saveWiFi() {
        var saveStatusElem;
        let configs = {};

        saveStatusElem = $('#save-wifi-status');
        saveStatusElem.text("Saving...");

        if ($('select[data-key="WIFI_ESSID"]').prop('value') == "Other...") {
            configs["WIFI_ESSID"] = $('input[type="text"][data-key="WIFI_ESSID_MANUAL"]').prop('value')
        } else {
            configs["WIFI_ESSID"] = $('select[data-key="WIFI_ESSID"]').prop('value');
        }
        configs["WIFI_PASSWORD"] = $('input[type="password"][data-key="WIFI_PASSWORD"]').prop('value');
        configs["WIFI_PASSWORD2"] = $('input[type="password"][data-key="WIFI_PASSWORD2"]').prop('value');

        if (configs["WIFI_ESSID"] == "") {
            saveStatusElem.text("Not saved, essid is blank.");
        } else if (configs["WIFI_PASSWORD"] == "") {
            saveStatusElem.text("Not saved, password is blank.");
        } else if (configs["WIFI_PASSWORD"] == "" || configs["WIFI_PASSWORD"] != configs["WIFI_PASSWORD2"]) {
            saveStatusElem.text("Not saved, passwords don't match.");
        } else {
            var configData = JSON.stringify(configs);
            var escapedConfigData = configData.replace(/\\/g, "\\")
                .replace(/\\"/g, '\\"');

            $.ajax({
                type: "POST",
                url: 'cgi-bin/wifi.sh?action=save',
                data: escapedConfigData,
                dataType: "json",
                success: function(response) {
                    if (!response || !response.error) {
                        saveStatusElem.text("Not saved, generic error.");
                    } else if (response.error == "true") {
                        saveStatusElem.text("Not saved, passwords don't match.");
                    } else {
                        saveStatusElem.text("Saved");
                    }
                },
                error: function(response) {
                    saveStatusElem.text("Error while saving");
                    console.log('error', response);
                }
            });
        }
    }

    function updateWiFiPage() {
        loadingStatusElem = $('#loading-wifi-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/wifi.sh?action=scan',
            dataType: "json",
            success: function(data) {
                loadingStatusElem.fadeOut(500);

                html = "<select data-key=\"WIFI_ESSID\" id=\"WIFI_ESSID\">";
                for (var i = 0; i < data.wifi.length; i++) {
                    if (data.wifi[i].length > 0) {
                        html += "<option value=\"" + data.wifi[i] + "\">" + data.wifi[i] + "</option>";
                    }
                }
                html += "<option value=\"Other...\">Other...</option>";
                html += "</select>"
                document.getElementById("select-container").innerHTML = html;
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function toggleESSIDInput() {
        if ($("#WIFI_ESSID option:selected" ).text() == "Other...") {
            $('#input-container').show();
        } else {
            $('#input-container').hide();
        }
    }

    return {
        init: init
    };

})(jQuery);