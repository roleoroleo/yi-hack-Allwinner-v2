var APP = APP || {};

APP.static_ip = (function($) {

    function init() {
        $('#input-container').hide();
        registerEventHandler();
        updateStaticIPPage();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save-static-ip', function(e) {
            saveStaticIP();
        });
    }

    function saveStaticIP() {
        var saveStatusElem;
        let configs = {};

        saveStatusElem = $('#save-static-ip-status');
        saveStatusElem.text("Saving...");

        configs["STATIC_IP"] = $('input[type="text"][data-key="STATIC_IP"]').prop('value');
        configs["STATIC_MASK"] = $('input[type="text"][data-key="STATIC_MASK"]').prop('value');
        configs["STATIC_GW"] = $('input[type="text"][data-key="STATIC_GW"]').prop('value');
        configs["STATIC_DNS1"] = $('input[type="text"][data-key="STATIC_DNS1"]').prop('value');
        configs["STATIC_DNS2"] = $('input[type="text"][data-key="STATIC_DNS2"]').prop('value');

        if ((configs["STATIC_IP"] == "") && (configs["STATIC_MASK"] != "")) {
            saveStatusElem.text("Not saved, IP address is blank.");
        } else if ((configs["STATIC_IP"] != "") && (configs["STATIC_MASK"] == "")) {
            saveStatusElem.text("Not saved, subnet mask is blank.");
        } else {
            var configData = JSON.stringify(configs);
            var escapedConfigData = configData.replace(/\\/g, "\\")
                .replace(/\\"/g, '\\"');

            $.ajax({
                type: "POST",
                url: 'cgi-bin/set_configs.sh?conf=system',
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

    function updateStaticIPPage() {
        loadingStatusElem = $('#loading-static-ip-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=system',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function(key, state) {
                    if (key == "STATIC_IP" || key == "STATIC_MASK" || key == "STATIC_GW" || key == "STATIC_DNS1" || key == "STATIC_DNS2")
                        $('input[type="text"][data-key="' + key + '"]').prop('value', state);
                });
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    return {
        init: init
    };

})(jQuery);
