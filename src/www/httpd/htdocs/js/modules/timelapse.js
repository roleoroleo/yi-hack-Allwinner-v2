var APP = APP || {};

APP.timelapse = (function($) {

    function init() {
        updateTimeLapseFilePage();
        registerEventHandler();
    }

    function registerEventHandler() {
        $(document).on("click", '.button-delete', function(e) {
            deleteFile();
        });
        $(document).on("click", '.button-play', function(e) {
            playFile();
        });
    }

    function deleteFile() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/timelapse.sh?action=delete&file=' + event.target.id.substring(14),
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
        const CONTENT_LENGTH = 'content-length';
        const TYPE_JPEG = 'image/jpeg';
        const url = event.target.id.substring(12);
        var videoPlayer = $('#video');
        videoPlayer.show();
        $(window).scrollTop(0);

        const SOI = new Uint8Array(2);
        const EOI = new Uint8Array(2);
        SOI[0] = 0xFF;
        SOI[1] = 0xD8;
        EOI[0] = 0xFF;
        EOI[1] = 0xD9;

        $.ajax({
            type: "GET",
            url: url,
            xhrFields: { responseType: 'arraybuffer'},
            success: function(data, textStatus, request) {
                let aviContentLength = request.getResponseHeader(CONTENT_LENGTH);
                value = new Uint8Array(data);
                value2 = new ArrayBuffer(data);

                let interval = 500;
                let exit = false;
                let index = 0;

                (function p() {
                    for (; index < value.length; index++) {
                        // we've found start of the frame. Everything we've read till now is the header.
                        if (value[index] === SOI[0] && value[index+1] === SOI[1]) {
                            soiIndex = index;
                            // console.log('header found : ' + newHeader);
                            index++;
                        } else if (value[index] === EOI[0] && value[index+1] === EOI[1]) {
                            eoiIndex = index+2;
                            imageBuffer = new Uint8Array(value.slice(soiIndex, eoiIndex));

                            // console.log("jpeg read with bytes : " + bytesRead);
                            let frame = URL.createObjectURL(new Blob([imageBuffer], {type: TYPE_JPEG}));
                            videoPlayer[0].src = frame;
                            URL.revokeObjectURL(frame);
                            soiIndex = -1;
                            eoiIndex = -1;
                            index++;
                            break;
                        }
                    }
                    if (index < value.length) {
                        setTimeout(p, interval);
                    }
                })();
            },
            error: function(response) {
                console.log('error', response);
            }
        });
    }

    function updateTimeLapseFilePage() {
        $.ajax({
            type: "GET",
            url: 'cgi-bin/timelapse.sh?action=list',
            dataType: "json",
            success: function(data) {
                document.getElementById("title-container").innerHTML = "Select video";
                html = "<table class=\"u-full-width padded-table\"><tbody>";
                html += "<tr><td><b>Video creation date</b></td>";
                html += "<td><b>Time</b></td>";
                html += "<td><b>File name</b></td>";
                html += "<td><b>Play/Delete</b></td></tr>";
                if (data.records.length == 0) {
                    html += "<tr><td>No video in this folder</td><td></td></tr>";
                } else {
                    for (var i = 0; i < data.records.length; i++) {
                        var record = data.records[i];
                        html += "<tr><td>" + record.date + "</td>";
                        html += "<td>" + record.time + "</td>";
                        html += "<td><a href=\"record/timelapse/" + record.filename + "\">" + record.filename + "</a></td>";
                        html += "<td><input class=\"button-primary button-play\" type=\"button\" id=\"button-play-" + "record/timelapse/" + record.filename + "\" value=\"Play\"/>";
                        html += "<input class=\"button-primary button-delete\" type=\"button\" id=\"button-delete-" + record.filename + "\" value=\"Delete\"/ style=\"margin-left:1em;\"></td></tr>";
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
