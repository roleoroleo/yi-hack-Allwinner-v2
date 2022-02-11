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
        ttsvol = $("select[name='ttsvol']").prop('value');
        $.ajax({
            url: "cgi-bin/speak.sh?lang="+ttslang+"&voldb="+ttsvol,
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
        wavvol = $("select[name='wavvol']").prop('value');
        $.ajax({
            url: "cgi-bin/speaker.sh?voldb="+wavvol,
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
