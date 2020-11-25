var APP = APP || {};

function getUrlVar(key)
{
    var hash;
    var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
    for(var i = 0; i < hashes.length; i++)
    {
        hash = hashes[i].split('=');
        if (hash[0] == key)
            return hash[1];
    }
    return '';
}

APP.eventsfile = (function ($) {

    function init() {
        updateEventsFilePage();
        registerEventHandler();
    }

    function registerEventHandler() {
        $(document).on("click", '.button-delete', function (e) {
            deleteFile();
        });
        $(document).on("click", '.button-play', function (e) {
            playFile();
        });
    }

    function deleteFile() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsfiledel.sh?file='+event.target.id.substring(14),
            dataType: "json",
            success: function(response) {
                window.location.reload();
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function playFile() {
        var videoPlayer = $('#video');
        videoPlayer.attr('src', event.target.id.substring(12));
        videoPlayer.show();
        $(window).scrollTop(0);
    }

    function updateEventsFilePage() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/eventsfile.sh?dirname=' + getUrlVar('dirname'),
            dataType: "json",
            success: function(data) {
                document.getElementById("title-container").innerHTML = data.date + " - Select event";
                html = "<table class=\"u-full-width padded-table\"><tbody>";
                html += "<tr><td><b>Time</b></td>";
                html += "<td><b>File name</b></td>";
                html += "<td><b>Play file</b></td>";
                html += "<td><b>Delete file</b></td></tr>";
                if (data.records.length == 0) {
                    html += "<tr><td>No events in this folder</td><td></td></tr>";
                } else {
                    for (var i = 0; i < data.records.length; i++) {
                        var record = data.records[i];
                        html += "<tr><td>" + record.time + "</td>";
                        html += "<td><a href=\"record/" + getUrlVar('dirname') + "/" + record.filename + "\">" +  record.filename + "</a></td>";
                        html += "<td><input class=\"button-primary button-play\" type=\"button\" id=\"button-play-" + "record/" + getUrlVar('dirname') + "/" + record.filename + "\" value=\"Play\"/></td>";
                        html += "<td><input class=\"button-primary button-delete\" type=\"button\" id=\"button-delete-" + getUrlVar('dirname') + "/" + record.filename + "\" value=\"Delete\"/></td></tr>";
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
