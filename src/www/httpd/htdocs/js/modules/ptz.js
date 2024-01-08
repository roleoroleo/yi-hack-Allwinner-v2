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
        $(document).on("click", '#button-add', function(e) {
            addPreset('#button-add');
        });
        $(document).on("click", '#button-del', function(e) {
            delPreset('#button-del', '#select-del');
        });
        $(document).on("click", '#button-del-all', function(e) {
            delAllPreset('#button-del-all');
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
        preset_num = $(select + " option:selected").val();
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

    function addPreset(button) {
        $(button).attr("disabled", true);
        $.ajax({
            type: "POST",
            url: 'cgi-bin/preset.sh?action=add_preset&name='+$('input[type="text"][data-key="PRESET_NAME"]').prop('value'),
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
                window.location.reload();
            }
        });
    }

    function delPreset(button, select) {
        $(button).attr("disabled", true);
        $.ajax({
            type: "POST",
            url: 'cgi-bin/preset.sh?action=del_preset&num='+$(select + " option:selected").val(),
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
                window.location.reload();
            }
        });
    }

    function delAllPreset(button) {
        $(button).attr("disabled", true);
        $.ajax({
            type: "POST",
            url: 'cgi-bin/preset.sh?action=del_preset&num=all',
            dataType: "json",
            error: function(response) {
                console.log('error', response);
                $(button).attr("disabled", false);
            },
            success: function(data) {
                $(button).attr("disabled", false);
                window.location.reload();
            }
        });
    }

    function initPage() {

        $.ajax({
            type: "GET",
            url: 'cgi-bin/get_configs.sh?conf=ptz_presets',
            dataType: "json",
            success: function(data) {
                html = "<select id=\"select-goto\">\n";
                for (let key in data) {
                    if (key != "NULL") {
                        fields = data[key].split(',');
                        html += "<option value=\"" + key + "\">" + key + " - " + fields[0] + "</option>\n";
                    }
                }
                html += "</select>\n";
                document.getElementById("select-goto-container").innerHTML = html;

                html = "<select id=\"select-del\">\n";
                for (let key in data) {
                    if (key != "NULL") {
                        fields = data[key].split(',');
                        html += "<option value=\"" + key + "\">" + key + " - " + fields[0] + "</option>\n";
                    }
                }
                html += "</select>\n";
                document.getElementById("select-del-container").innerHTML = html;
            },
            error: function(response) {
                console.log('error', response);
            }
        });

        interval = 1000;

        (function p() {
            jQuery.get('cgi-bin/snapshot.sh?res=low&base64=yes', function(data) {
                image = document.getElementById('imgSnap');
                image.src = 'data:image/png;base64,' + data;
            }).always(function() {
                setTimeout(p, interval);
            });
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
