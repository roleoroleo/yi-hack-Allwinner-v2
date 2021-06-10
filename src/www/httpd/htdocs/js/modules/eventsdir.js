var APP = APP || {};

APP.eventsdir = (function($) {

    function init() {
        fetchConfigs();
        updateEventsDirPage();
        registerEventHandler();
    }

    function registerEventHandler() {
        $(document).on("click", '.button-primary', function(e) {
            buttonClick();
        });
    }

    function buttonClick() {
        if (event.target.id == "button-save") {
            saveConfigs();
        } else {
            deleteDir();
        }
    }

    function deleteDir() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsdirdel.sh?dir=' + event.target.id.substring(14),
            dataType: "json",
            success: function(response) {
                window.location.reload();
            },
            error: function(response) {
                console.log('error', response);
            }
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
                    if (key == "FREE_SPACE" || key == "FTP_HOST" || key == "FTP_DIR" || key == "FTP_USERNAME")
                        $('input[type="text"][data-key="' + key + '"]').prop('value', state);
                    else if (key == "FTP_PASSWORD")
                        $('input[type="password"][data-key="' + key + '"]').prop('value', state);
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

        configs["FREE_SPACE"] = $('input[type="text"][data-key="FREE_SPACE"]').prop('value');
        configs["FTP_HOST"] = $('input[type="text"][data-key="FTP_HOST"]').prop("value");
        configs["FTP_DIR"] = $('input[type="text"][data-key="FTP_DIR"]').prop("value");
        configs["FTP_USERNAME"] = $('input[type="text"][data-key="FTP_USERNAME"]').prop("value");
        configs["FTP_PASSWORD"] = $('input[type="password"][data-key="FTP_PASSWORD"]').prop("value");

        $.ajax({
            type: "POST",
            url: 'cgi-bin/set_configs.sh?conf=system',
            data: JSON.stringify(configs),
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

    function updateEventsDirPage() {
        html = "<table class=\"u-full-width padded-table\"><tbody>";
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsdir.sh',
            dataType: "json",
            success: function(data) {
                html += "<tr><td><b>Date & time</b></td>";
                html += "<td><b>Directory name</b></td>";
                html += "<td><b>Delete directory</b></td></tr>";
                if (data.records.length == 0) {
                    html += "<tr><td>No events</td><td></td></tr>";
                } else {
                    for (var i = 0; i < data.records.length; i++) {
                        var record = data.records[i];
                        html += "<tr><td>" + record.datetime + "</td>";
                        html += "<td><a href=\"?page=eventsfile&dirname=" + record.dirname + "\">" + record.dirname + "</a></td>";
                        html += "<td><input class=\"button-primary\" type=\"button\" id=\"button-delete-" + record.dirname + "\" value=\"Delete\"/></td></tr>";
                    }
                }
                html += "</tbody></table>";
                document.getElementById("table-container").innerHTML = html;
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