var APP = APP || {};

APP.ptz = (function($) {

    function init() {
        registerEventHandler();
        initPage();
        updatePage();
    }

    function registerEventHandler() {
        $(document).on("click", '#img-au', function(e) {
            move('#img-au', 'up');
        });
        $(document).on("click", '#img-al', function(e) {
            move('#img-al', 'left');
        });
        $(document).on("click", '#img-ar', function(e) {
            move('#img-ar', 'right');
        });
        $(document).on("click", '#img-ad', function(e) {
            move('#img-ad', 'down');
        });
        $(document).on("click", '#button-goto', function(e) {
            gotoPreset('#button-goto', '#select-goto');
        });
        $(document).on("click", '#button-set', function(e) {
            setPreset('#button-set');
        });
    }

    function move(button, dir) {
        $(button).attr("disabled", true);
        $.ajax({
            type: "GET",
            url: 'cgi-bin/ptz.sh?dir=' + dir,
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
            }
        });
    }

    function gotoPreset(button, select) {
        $(button).attr("disabled", true);
        preset_num = $(select + " option:selected").text();
        $.ajax({
            type: "GET",
            url: 'cgi-bin/preset.sh?action=go_preset&num=' + preset_num,
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
            }
        });
    }

    function setPreset(button) {
        $(button).attr("disabled", true);
        $.ajax({
            type: "POST",
            url: 'cgi-bin/preset.sh',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
            }
        });
    }

    function initPage() {
        interval = 1000;

        (function p() {
            jQuery.get('cgi-bin/snapshot.sh?res=low&base64=yes', function(data) {
                image = document.getElementById('imgSnap');
                image.src = 'data:image/png;base64,' + data;
            })
            setTimeout(p, interval);
        })();
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
                    $('#ptz_description').show();
                    $('#ptz_unavailable').hide();
                    $('#ptz_main').show();
                } else {
                    $('#ptz_description').hide();
                    $('#ptz_unavailable').show();
                    $('#ptz_main').hide();
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
