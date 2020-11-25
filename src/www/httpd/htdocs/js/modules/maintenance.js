var APP = APP || {};

APP.maintenance = (function ($) {

    var timeoutVar;

    function init() {
        registerEventHandler();
        setRebootStatus("Camera is online.");
        getFwStatus();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-save', function (e) {
            saveConfig();
        });
        $(document).on("click", '#button-load', function (e) {
            loadConfig();
        });
        $(document).on("click", '#button-reboot', function (e) {
            rebootCamera();
        });
        $(document).on("click", '#button-reset', function (e) {
            resetCamera();
        });
        $(document).on("click", '#button-upgrade', function (e) {
            upgradeFirmware();
        });
    }

    function saveConfig() {
        $('#button-save').attr("disabled", true);
        var xhr = new XMLHttpRequest();
        xhr.open('GET', 'cgi-bin/save.sh', true);
        xhr.responseType = 'blob';
        xhr.onload = function(e) {
            if (xhr.status == 200) {
                var myBlob = xhr.response;
                var url = URL.createObjectURL(myBlob);
                var $a = $('<a />', {
                    'href': url,
                    'download': 'config.tar.bz2',
                    'text': "click"
                }).hide().appendTo("body")[0].click();
                URL.revokeObjectURL(url);
            }
        };
        xhr.send();
    }

    function loadConfig() {
        $('#button-load').attr("disabled", true);
        var fileSelect = document.getElementById('button-file');
        var files = fileSelect.files;
        var formData = new FormData();

        for (var i = 0; i < files.length; i++) {
            var file = files[i];
            formData.append('files[]', file, file.name);
        }

        var xhr = new XMLHttpRequest();
        xhr.open('POST', 'cgi-bin/load.sh', true);
        xhr.onload = function () {
            if (xhr.status === 200) {
                $('#button-load').attr("disabled", false);
            }
            var myText = xhr.response;
            $('#text-load').text(myText);
        };
        xhr.send(formData);
    }

    function rebootCamera() {
        $('#button-reboot').attr("disabled", true);
        $.ajax({
            type: "GET",
            url: 'cgi-bin/reboot.sh',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $('#button-reboot').attr("disabled", false);
            },
            success: function(data) {
                setRebootStatus("Camera is rebooting.");
                waitForBoot();
            }
        });
    }

    function waitForBoot() {
        setInterval(function(){
            $.ajax({
                url: 'index.html',
                cache: false,
                success: function(data) {
                    setRebootStatus("Camera is back online, redirecting to home.");
                    $('#button-reboot').attr("disabled", false);
                    window.location.href="index.html";
                },
                error: function(data) {
                    setRebootStatus("Waiting for the camera to come back online.");
                },
                timeout: 3000,
            });
        }, 5000);
    }

    function resetCamera() {
        $('#button-reset').attr("disabled", true);
        $.ajax({
            type: "GET",
            url: 'cgi-bin/reset.sh',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $('#button-reset').attr("disabled", false);
            },
            success: function(data) {
                setResetStatus("Reset completed, reboot your camera.");
            }
        });
    }

    function upgradeFirmware() {
        $('#button-upgrade').attr("disabled", true);
        setFwStatus("Firmware download in progress.");
        $.ajax({
            type: "GET",
            url: 'cgi-bin/fw_upgrade.sh?get=upgrade',
            error: function(response) {
                console.log('error', response);
                $('#button-upgrade').attr("disabled", false);
            },
            success: function(response) {
                setFwStatus(response);
                waitForUpgrade();
            }
        });
    }

    function waitForUpgrade() {
        setInterval(function(){
            $.ajax({
                url: 'index.html',
                cache: false,
                success: function(data) {
                    setFwStatus("Camera is upgrading.");
                    $('#button-upgrade').attr("disabled", false);
                    window.location.href="index.html";
                },
                error: function(data) {
                    setFwStatus("Waiting for the camera to come back online...");
                },
                timeout: 3000,
            });
        }, 5000);
    }

    function setRebootStatus(text)
    {
        $('input[type="text"][data-key="STATUS"]').prop('value', text);
    }

    function setResetStatus(text)
    {
        $('input[type="text"][data-key="RESET"]').prop('value', text);
    }

    function setFwStatus(text)
    {
        $('input[type="text"][data-key="FW"]').prop('value', text);
    }

    function getFwStatus() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/fw_upgrade.sh?get=info',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                setFwStatus("Error getting fw info");
            },
            success: function(data) {
                setFwStatus("Installed: " + data.fw_version + " - Available: " + data.latest_fw);
                if (data.fw_version == data.latest_fw) {
                    $('#button-upgrade').attr("disabled", true);
                }
            }
        });
    }

    return {
        init: init
    };

})(jQuery);
