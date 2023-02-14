var APP = APP || {};

APP.camera_settings = (function($) {

    function init() {
        registerEventHandler();
        fetchConfigs();
        updatePage();
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
            url: 'cgi-bin/get_configs.sh?conf=camera',
            dataType: "json",
            success: function(response) {
                loadingStatusElem.fadeOut(500);

                $.each(response, function(key, state) {
                    if(key=="SENSITIVITY" || key=="SOUND_SENSITIVITY" || key=="CRUISE")
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

        configs["SENSITIVITY"] = $('select[data-key="SENSITIVITY"]').prop('value');
        configs["SOUND_SENSITIVITY"] = $('select[data-key="SOUND_SENSITIVITY"]').prop('value');
        configs["CRUISE"] = $('select[data-key="CRUISE"]').prop('value');

        $.ajax({
            type: "GET",
            url: 'cgi-bin/camera_settings.sh?' +
                'save_video_on_motion=' + configs["SAVE_VIDEO_ON_MOTION"] +
                '&sensitivity=' + configs["SENSITIVITY"] +
                '&ai_human_detection=' + configs["AI_HUMAN_DETECTION"] +
                '&face_detection=' + configs["FACE_DETECTION"] +
                '&motion_tracking=' + configs["MOTION_TRACKING"] +
                '&sound_detection=' + configs["SOUND_DETECTION"] +
                '&sound_sensitivity=' + configs["SOUND_SENSITIVITY"] +
                '&led=' + configs["LED"] +
                '&ir=' + configs["IR"] +
                '&rotate=' + configs["ROTATE"] +
                '&switch_on=' + configs["SWITCH_ON"] +
                '&cruise=' + configs["CRUISE"],
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

    function updatePage() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/status.json',
            dataType: "json",
            success: function(data) {
                ptz_enabled = ["r30gb", "r35gb", "r40ga", "h51ga", "h52ga", "h60ga", "q321br_lsx", "qg311r", "b091qp"];
                this_model = data["model_suffix"] || "unknown";
                if (ptz_enabled.includes(this_model)) {
                    var lst = document.querySelectorAll(".ptz");
                    for(var i = 0; i < lst.length; ++i) {
                        lst[i].style.display = 'table-row';
                    }
                } else {
                    var lst = document.querySelectorAll(".ptz");
                    for(var i = 0; i < lst.length; ++i) {
                        lst[i].style.display = 'none';
                    }
                }
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
