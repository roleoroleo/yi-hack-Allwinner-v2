var APP = APP || {};

APP.proxy = (function($) {

    function init() {
        registerEventHandler();
        updateProxyPage();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save-proxy', function(e) {
            saveAndTestProxy();
        });
    }

    function saveAndTestProxy() {
        saveProxy();
        testProxy();
    }

    function saveProxy() {
        var saveStatusElem;
        let configs = {};

        saveStatusElem = $('#save-proxy-status');
        saveStatusElem.text("Saving...");

        configs["PROXYCHAINS_SERVERS"] = $('input[type="text"][data-key="PROXYCHAINS_SERVERS"]').prop('value');

        var configData = JSON.stringify(configs);
        var escapedConfigData = configData.replace(/\\/g, "\\")
            .replace(/\\"/g, '\\"');

        $.ajax({
            type: "POST",
            url: 'cgi-bin/set_configs.sh?conf=proxychains',
            data: escapedConfigData,
            dataType: "json",
            success: function(response) {
                if (!response || !response.error) {
                    saveStatusElem.text("Not saved, generic error.");
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

    function testProxy() {
        var test1StatusElem;
        var test0StatusElem;
        let configs = {};

        test1StatusElem = $('#test1-proxy-status');
        test1StatusElem.text("Test in progress...");
        test0StatusElem = $('#test0-proxy-status');
        test0StatusElem.text("Test in progress...");
        $('textarea#TEST_WITH_PROXY').prop('value', '');
        $('textarea#TEST_WITHOUT_PROXY').prop('value', '');

        configs["PROXYCHAINS_SERVERS"] = $('input[type="text"][data-key="PROXYCHAINS_SERVERS"]').prop('value');

        if (configs["PROXYCHAINS_SERVERS"] == "") {
            test1StatusElem.text("Not tested");
            test0StatusElem.text("Not tested");
            return;
        }

        $.ajax({
            type: "GET",
            url: 'cgi-bin/proxy.sh?proxy=1',
            dataType: "json",
            success: function(response) {
                if (!response || !response.error) {
                    test1StatusElem.text("Generic error.");
                } else {
                    test1StatusElem.text("Test completed");
                }

                $.each(response, function(key, state) {
                    if (key == "result")
                        $('textarea#TEST_WITH_PROXY').prop('value', JSON.stringify(state, null, '\t'));
                });
            },
            error: function(response) {
                test1StatusElem.text("Error while testing");
                console.log('error', response);
            }
        });
        $.ajax({
            type: "GET",
            url: 'cgi-bin/proxy.sh?proxy=0',
            dataType: "json",
            success: function(response) {
                if (!response || !response.error) {
                    test0StatusElem.text("Generic error.");
                } else {
                    test0StatusElem.text("Test completed");
                }

                $.each(response, function(key, state) {
                    if (key == "result")
                        $('textarea#TEST_WITHOUT_PROXY').prop('value', JSON.stringify(state, null, ' '));
                });
            },
            error: function(response) {
                test0StatusElem.text("Error while testing");
                console.log('error', response);
            }
        });
    }

    function updateProxyPage() {
        loadingStatusElem = $('#loading-proxy-status');
        loadingStatusElem.text("Loading...");

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=proxychains',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function(key, state) {
                    if (key == "PROXYCHAINS_SERVERS")
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
