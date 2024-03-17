var APP = APP || {};

APP.configurations_onvif = (function($) {

    function init() {
        registerEventHandler();
        fetchConfigs();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function(e) {
            saveConfigs();
        });
    }

    function fetchConfigs() {
        loadingStatusElem = $('#loading-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=system',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function(key, state) {
                    if (key == "ONVIF_PROFILE" || key == "ONVIF_NETIF" || key == "ONVIF_AUDIO_BC")
                        $('select[data-key="' + key + '"]').prop('value', state);
                    else
                        $('input[type="checkbox"][data-key="' + key + '"]').prop('checked', state === 'yes');
                });
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function saveConfigs() {
        var saveStatusElem;
        let configs = {};

        saveStatusElem = $('#save-status');

        saveStatusElem.text("Saving...");

        $('.configs-switch input[type="checkbox"]').each(function() {
            configs[$(this).attr('data-key')] = $(this).prop('checked') ? 'yes' : 'no';
        });

        configs["ONVIF_PROFILE"] = $('select[data-key="ONVIF_PROFILE"]').prop('value');
        configs["ONVIF_NETIF"] = $('select[data-key="ONVIF_NETIF"]').prop('value');
        configs["ONVIF_AUDIO_BC"] = $('select[data-key="ONVIF_AUDIO_BC"]').prop('value');

        var configData = JSON.stringify(configs);
        var escapedConfigData = configData.replace(/\\/g, "\\")
            .replace(/\\"/g, '\\"');

        $.ajax({
            type: "POST",
            url: 'cgi-bin/set_configs.sh?conf=system',
            data: escapedConfigData,
            dataType: "json",
            success: function(response) {
                saveStatusElem.text("Saved");
            },
            error: function(response) {
                saveStatusElem.text("Error while saving");
                console.log('error', response);
            }
        });
    }

    return {
        init: init
    };

})(jQuery);
