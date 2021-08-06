var APP = APP || {};

APP.speak = (function($) {

    function init() {
        registerEventHandler();
    }

    function registerEventHandler() {
        $(document).on("click", '#button-speak', function(e) {
            sendText();
        });
        $(document).on("click", '#button-speaker', function(e) {
            sendWav();
        });
    }

    function sendText() {
        ttsterm = $("input[name='ttsinput']").prop('value');
        ttslang = $("select[name='ttslang']").prop('value');
        $.ajax({
            url: "cgi-bin/speak.sh?lang="+ttslang,
            type: 'POST',
            contentType: false,
            data: ttsterm,
            cache: false,
            processData: false
        });
    }

    function sendWav() {
        var fileData = $('#wavfile').prop('files')[0];
        var formData = new FormData();
        formData.append('file', fileData);
        $.ajax({
            url: "cgi-bin/speaker.sh",
            type: 'POST',
            contentType: false,
            data: formData,
            cache: false,
            processData: false
        });
    }

    return {
        init: init
    };

})(jQuery);
