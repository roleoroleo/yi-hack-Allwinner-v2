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
        $(document).on("change", '#MOTION_DETECTION', function(e) {
            motionDetection('#MOTION_DETECTION');
        });
        $(document).on("change", '#AI_HUMAN_DETECTION', function(e) {
            aiMotionDetections('#AI_HUMAN_DETECTION');
        });
        $(document).on("change", '#AI_VEHICLE_DETECTION', function(e) {
            aiMotionDetections('#AI_VEHICLE_DETECTION');
        });
        $(document).on("change", '#AI_ANIMAL_DETECTION', function(e) {
            aiMotionDetections('#AI_ANIMAL_DETECTION');
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
                if (response["HOMEVER"].startsWith("11") || response["HOMEVER"].startsWith("12")) {
                    var objects = document.querySelectorAll(".fw12");
                    for (var i = 0; i < objects.length; i++) {
                    objects[i].style.display = "table-row";
                }
                } else {
                    var objects = document.querySelectorAll(".no_fw12");
                    for (var i = 0; i < objects.length; i++) {
                    objects[i].style.display = "table-row";
                    }
                }
                aiMotionDetections();
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function motionDetection(el) {
        if ($(el).prop('checked')) {
            $("#AI_HUMAN_DETECTION").prop('checked', false)
            $("#AI_VEHICLE_DETECTION").prop('checked', false)
            $("#AI_ANIMAL_DETECTION").prop('checked', false)
        }
    }

    function aiMotionDetections(el) {
        if ($(el).prop('checked')) {
            $("#MOTION_DETECTION").prop('checked', false)
        }
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
                '&motion_detection=' + configs["MOTION_DETECTION"] +
                '&sensitivity=' + configs["SENSITIVITY"] +
                '&ai_human_detection=' + configs["AI_HUMAN_DETECTION"] +
                '&ai_vehicle_detection=' + configs["AI_VEHICLE_DETECTION"] +
                '&ai_animal_detection=' + configs["AI_ANIMAL_DETECTION"] +
                '&face_detection=' + configs["FACE_DETECTION"] +
                '&motion_tracking=' + configs["MOTION_TRACKING"] +
                '&sound_detection=' + configs["SOUND_DETECTION"] +
                '&sound_sensitivity=' + configs["SOUND_SENSITIVITY"] +
                '&led=' + configs["LED"] +
                '&ir=' + configs["IR"] +
                '&rotate=' + configs["ROTATE"] +
                '&cruise=' + configs["CRUISE"] +
                '&switch_on=' + configs["SWITCH_ON"],
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
