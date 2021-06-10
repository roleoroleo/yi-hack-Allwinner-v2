var APP = APP || {};

APP.snapshot = (function($) {

    function init() {
        initPage();
    }

    function initPage() {
        jQuery.get('cgi-bin/snapshot.sh?res=high&base64=yes&watermark=yes', function(data) {
            image = document.getElementById('imgSnap');
            image.src = 'data:image/png;base64,' + data;
            image.style = 'width:100%;';
        })
    }

    return {
        init: init
    };

})(jQuery);