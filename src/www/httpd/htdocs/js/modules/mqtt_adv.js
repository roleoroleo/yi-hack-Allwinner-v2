var APP = APP || {};

APP.mqtt_adv = (function ($) {

    function init() {
        registerEventHandler();
        fetchConfigs();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function (e) {
            saveConfigs();
        });
    }

    function fetchConfigs() {
        loadingStatusElem = $('#loading-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=mqtt_advertise',
            dataType: "json",
            success: function (response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function (key, state) {
                    if (key == "HOMEASSISTANT_BOOT" || key == "HOMEASSISTANT_CRON" || key == "HOMEASSISTANT_ENABLE" || 
                        key == "MQTT_ADV_INFO_GLOBAL_ENABLE" || key == "MQTT_ADV_INFO_GLOBAL_BOOT" || key == "MQTT_ADV_INFO_GLOBAL_CRON" || 
                        key == "MQTT_ADV_LINK_ENABLE" || key == "MQTT_ADV_LINK_BOOT" || key == "MQTT_ADV_LINK_CRON" ||
                        key == "MQTT_ADV_CAMERA_SETTING_ENABLE" || key == "MQTT_ADV_CAMERA_SETTING_BOOT" || key == "MQTT_ADV_CAMERA_SETTING_CRON" ||
                        key == "MQTT_ADV_TELEMETRY_ENABLE" || key == "MQTT_ADV_TELEMETRY_BOOT" || key == "MQTT_ADV_TELEMETRY_CRON") {
                        $('input[type="checkbox"][data-key="' + key + '"]').prop('checked', state === 'yes');

                    } else {
                        $('input[type="text"][data-key="' + key + '"]').prop('value', state);
                    }
                });
            },
            error: function (response) {
                console.log('error', response);
            }
        });

    }

    function saveConfigs() {
        var saveStatusElem;

        let configs = {};

        saveStatusElem = $('#save-status');
        saveStatusElem.text("Saving...");

        $('.configs-switch input[type="text"]').each(function () {
            configs[$(this).attr('data-key')] = $(this).prop('value');
        });

        $('.configs-switch input[type="password"]').each(function () {
            configs[$(this).attr('data-key')] = $(this).prop('value');
        });

        $('.configs-switch input[type="checkbox"]').each(function () {
            configs[$(this).attr('data-key')] = $(this).prop('checked') ? 'yes' : 'no';
        });

        $.ajax({
            type: "POST",
            url: 'cgi-bin/set_configs.sh?conf=mqtt_advertise',
            data: JSON.stringify(configs),
            dataType: "json",
            success: function (response) {
                saveStatusElem.text("Saved");
            },
            error: function (response) {
                saveStatusElem.text("Error while saving");
                console.log('error', response);
            }
        });

    }

    return {
        init: init
    };

})(jQuery);