var APP = APP || {};

APP.configurations = (function ($) {

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
            url: 'cgi-bin/get_configs.sh?conf=system',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function (key, state) {
                    if(key=="HOSTNAME" || key=="TIMEZONE" || key=="NTP_SERVER" || key=="HTTPD_PORT" || key=="RTSP_PORT" || key=="ONVIF_PORT" || key=="USERNAME")
                        $('input[type="text"][data-key="' + key +'"]').prop('value', state);
                    else if(key=="RTSP_STREAM" || key=="RTSP_AUDIO" || key=="ONVIF_PROFILE" || key=="ONVIF_NETIF")
                        $('select[data-key="' + key +'"]').prop('value', state);
                    else if(key=="PASSWORD" || key=="SSH_PASSWORD")
                        $('input[type="password"][data-key="' + key +'"]').prop('value', state);
                    else if(key=="CRONTAB")
                        $('textarea#' + key).prop('value', state);
                    else
                        $('input[type="checkbox"][data-key="' + key +'"]').prop('checked', state === 'yes');
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

        $('.configs-switch input[type="checkbox"]').each(function () {
            configs[$(this).attr('data-key')] = $(this).prop('checked') ? 'yes' : 'no';
        });

        configs["HOSTNAME"] = $('input[type="text"][data-key="HOSTNAME"]').prop('value');

//        if(!validateHostname(configs["HOSTNAME"]))
//        {
//            saveStatusElem.text("Failed");
//            alert("Hostname not valid!");
//            return;
//        }

        configs["TIMEZONE"] = $('input[type="text"][data-key="TIMEZONE"]').prop('value');
        configs["NTP_SERVER"] = $('input[type="text"][data-key="NTP_SERVER"]').prop('value');
        configs["HTTPD_PORT"] = $('input[type="text"][data-key="HTTPD_PORT"]').prop('value');
        configs["RTSP_STREAM"] = $('select[data-key="RTSP_STREAM"]').prop('value');
        configs["RTSP_AUDIO"] = $('select[data-key="RTSP_AUDIO"]').prop('value');
        configs["RTSP_PORT"] = $('input[type="text"][data-key="RTSP_PORT"]').prop('value');
        configs["ONVIF_PORT"] = $('input[type="text"][data-key="ONVIF_PORT"]').prop('value');
        configs["ONVIF_PROFILE"] = $('select[data-key="ONVIF_PROFILE"]').prop('value');
        configs["ONVIF_NETIF"] = $('select[data-key="ONVIF_NETIF"]').prop('value');
        configs["USERNAME"] = $('input[type="text"][data-key="USERNAME"]').prop('value');
        configs["PASSWORD"] = $('input[type="password"][data-key="PASSWORD"]').prop('value');
        configs["SSH_PASSWORD"] = $('input[type="password"][data-key="SSH_PASSWORD"]').prop('value');
        configs["CRONTAB"] = $('textarea#CRONTAB').prop('value');

        var configData = JSON.stringify(configs);
        var escapedConfigData = configData.replace(/\\/g,  "\\")
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

    function validateHostname(hostname) {
        return /^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$/.test(hostname);
    }

    return {
        init: init
    };

})(jQuery);
