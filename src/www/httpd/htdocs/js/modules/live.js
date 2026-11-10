var APP = APP || {};

/*
 * Live view.
 *
 * cgi-bin/live.sh streams the raw output of h264grabber:
 *   - live.sh?res=low|high  H.264 Annex-B video
 *   - live.sh?audio=yes     AAC ADTS audio (only when the audio is enabled)
 * Here the streams are read with fetch(), split into frames, wrapped into
 * fragmented MP4 and played through Media Source Extensions, one
 * SourceBuffer per track on the same <video> element.
 * No transcoding on the cam, no external libraries.
 *
 * If MSE is not available a snapshot refresh is used as fallback.
 *
 * Written in ES5 on purpose (the build uses yui-compressor).
 */
APP.live = (function($) {

    var VIDEO_TIMESCALE = 90000;
    var RETRY_DELAY = 3000;
    var AUDIO_WAIT = 4000;            // max wait for the audio before going on without it
    var AUDIO_KEY = 'live-audio';     // localStorage key of the audio setting

    var AAC_RATES = [96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350];

    var MSE = window.ManagedMediaSource || window.MediaSource;

    var video = null;
    var session = null;       // current streaming session
    var retryTimer = null;
    var snapTimer = null;
    var running = false;
    var audioEnabled = false;

    function init() {
        video = document.getElementById('live-video');
        audioEnabled = loadAudioSetting();
        updateAudioButton();
        registerEventHandler();
        start();
    }

    function registerEventHandler() {
        $(document).on('click', '#button-live-play', function(e) {
            start();
        });
        $(document).on('click', '#button-live-stop', function(e) {
            stop();
            setStatus('Stopped');
        });
        $(document).on('click', '#button-live-audio', function(e) {
            toggleAudio();
        });
        $(document).on('change', '#live-res', function(e) {
            if (running) {
                start();
            }
        });
        $(document).on('click', '#button-live-fullscreen', function(e) {
            fullscreen();
        });
        window.addEventListener('pagehide', stop);
    }

    function setStatus(text) {
        $('#live-status').text(text);
    }

    function getRes() {
        var res = $('#live-res').val();
        return (res == 'high') ? 'high' : 'low';
    }

    /* ------------------------------------------------------------------ */
    /* Audio on/off                                                        */
    /* ------------------------------------------------------------------ */

    function loadAudioSetting() {
        try {
            return window.localStorage.getItem(AUDIO_KEY) === 'yes';
        } catch (e) {
            return false;
        }
    }

    function saveAudioSetting() {
        try {
            window.localStorage.setItem(AUDIO_KEY, audioEnabled ? 'yes' : 'no');
        } catch (e) {}
    }

    function updateAudioButton() {
        $('#button-live-audio').val(audioEnabled ? 'Switch off audio' : 'Switch on audio');
        video.muted = !audioEnabled;
    }

    function toggleAudio() {
        audioEnabled = !audioEnabled;
        saveAudioSetting();
        updateAudioButton();
        if (audioEnabled) {
            // Called inside the click: it lets the browser play with sound later
            var p = video.play();
            if (p && p['catch']) {
                p['catch'](function() {});
            }
        }
        // The audio needs its own SourceBuffer: restart the stream
        if (running) {
            start();
        }
    }

    /* ------------------------------------------------------------------ */
    /* Start/stop                                                          */
    /* ------------------------------------------------------------------ */

    function start() {
        stop();
        running = true;
        if (!MSE || !window.fetch || !window.ReadableStream) {
            startSnapshotFallback();
            return;
        }
        session = new Session(getRes(), audioEnabled);
        session.start();
    }

    function stop() {
        running = false;
        if (retryTimer) {
            clearTimeout(retryTimer);
            retryTimer = null;
        }
        if (snapTimer) {
            clearTimeout(snapTimer);
            snapTimer = null;
        }
        if (session) {
            session.close();
            session = null;
        }
    }

    function scheduleRetry(reason) {
        if (!running) {
            return;
        }
        setStatus(reason + ' - retrying in ' + (RETRY_DELAY / 1000) + 's...');
        retryTimer = setTimeout(function() {
            retryTimer = null;
            if (running) {
                start();
            }
        }, RETRY_DELAY);
    }

    function fullscreen() {
        var el = video;
        if ($('#live-img').is(':visible')) {
            el = document.getElementById('live-img');
        }
        if (el.requestFullscreen) {
            el.requestFullscreen();
        } else if (el.webkitRequestFullscreen) {
            el.webkitRequestFullscreen();
        } else if (el.webkitEnterFullscreen) {
            el.webkitEnterFullscreen();
        }
    }

    /* ------------------------------------------------------------------ */
    /* Fallback: periodic snapshots (browsers without MSE)                 */
    /* ------------------------------------------------------------------ */

    function startSnapshotFallback() {
        $('#live-video').hide();
        $('#live-img').show();
        setStatus('Your browser does not support Media Source Extensions: showing snapshots');
        var img = document.getElementById('live-img');
        var res = getRes();

        function next() {
            if (!running) {
                return;
            }
            $.ajax({
                type: 'GET',
                url: 'cgi-bin/snapshot.sh?res=' + res + '&base64=yes&watermark=yes',
                cache: false,
                success: function(data) {
                    img.src = 'data:image/jpeg;base64,' + data;
                },
                complete: function() {
                    if (running) {
                        snapTimer = setTimeout(next, 1000);
                    }
                }
            });
        }
        next();
    }

    /* ------------------------------------------------------------------ */
    /* Streaming session                                                   */
    /* ------------------------------------------------------------------ */

    /*
     * A track: one HTTP stream -> one muxer -> one SourceBuffer.
     * The SourceBuffers can be created only when the codecs of all the
     * tracks are known (MSE doesn't allow to add one after the first
     * append), until then the fragments wait in the queue.
     */
    function Track(kind) {
        this.kind = kind;
        this.mime = null;
        this.muxer = null;
        this.sb = null;
        this.queue = [];
        this.reader = null;
        this.done = false;        // the stream is over (or the track was dropped)
    }

    function Session(res, withAudio) {
        this.res = res;
        this.closed = false;
        this.abort = window.AbortController ? new AbortController() : null;
        this.ms = null;
        this.objectUrl = null;
        this.t0 = Date.now();      // common clock of the tracks
        this.timer = null;
        this.audioTimer = null;
        this.setupDone = false;
        this.info = '';
        this.note = '';

        this.video = new Track('video');
        this.tracks = [this.video];
        this.audio = null;
        if (withAudio) {
            this.audio = new Track('audio');
            this.tracks.push(this.audio);
        }

        // Video parsing state
        this.parser = new AnnexBParser();
        this.sps = null;
        this.pps = null;
        this.au = [];              // NAL units of the access unit being built
        this.auHasVcl = false;
        this.auTime = null;        // arrival time of the access unit being built
        this.waitIdr = true;
        this.arrivals = [];        // wall clock of recent frames (fps estimate)
        this.frames = 0;

        // Audio parsing state
        this.adts = new AdtsParser();
    }

    Session.prototype.start = function() {
        var self = this;
        $('#live-img').hide();
        $('#live-video').show();
        setStatus('Connecting...');

        this.ms = new MSE();
        if (window.ManagedMediaSource && this.ms instanceof window.ManagedMediaSource) {
            video.disableRemotePlayback = true;
        }
        this.objectUrl = URL.createObjectURL(this.ms);
        video.src = this.objectUrl;
        this.ms.addEventListener('sourceopen', function() {
            if (self.closed) {
                return;
            }
            self.fetchTrack(self.video, 'cgi-bin/live.sh?res=' + self.res);
            if (self.audio) {
                self.fetchTrack(self.audio, 'cgi-bin/live.sh?audio=yes');
            }
            self.timer = setInterval(function() { self.housekeeping(); }, 500);
        });
    };

    Session.prototype.close = function() {
        var i;
        this.closed = true;
        if (this.abort) {
            this.abort.abort();
        }
        for (i = 0; i < this.tracks.length; i++) {
            if (this.tracks[i].reader) {
                try { this.tracks[i].reader.cancel(); } catch (e) {}
            }
        }
        if (this.timer) {
            clearInterval(this.timer);
        }
        if (this.audioTimer) {
            clearTimeout(this.audioTimer);
        }
        try { video.pause(); } catch (e) {}
        video.removeAttribute('src');
        try { video.load(); } catch (e) {}
        if (this.objectUrl) {
            URL.revokeObjectURL(this.objectUrl);
        }
    };

    Session.prototype.fail = function(reason) {
        if (this.closed) {
            return;
        }
        console.log('live: ' + reason);
        this.close();
        if (session === this) {
            session = null;
            scheduleRetry(reason);
        }
    };

    Session.prototype.handleError = function(e) {
        if (e && e.fatal) {
            // Retrying would not help (e.g. unsupported codec)
            stop();
            setStatus(e.message);
        } else {
            this.fail((e && e.message) || 'Decoding error');
        }
    };

    /*
     * The audio is an extra: if it's missing or broken before the playback
     * is set up, go on with the video only.
     */
    Session.prototype.dropAudio = function(reason) {
        if (!this.audio || this.setupDone || this.audio.done) {
            return false;
        }
        console.log('live: audio dropped: ' + reason);
        this.audio.done = true;
        if (this.audio.reader) {
            try { this.audio.reader.cancel(); } catch (e) {}
        }
        this.tracks = [this.video];
        this.audio = null;
        this.note = ' - audio not available';
        this.trySetup();
        return true;
    };

    Session.prototype.trackError = function(track, reason) {
        if (this.closed) {
            return;
        }
        if (track.kind === 'audio' && this.dropAudio(reason)) {
            return;
        }
        this.fail(reason);
    };

    Session.prototype.fetchTrack = function(track, url) {
        var self = this;
        var opts = { cache: 'no-store', credentials: 'same-origin' };
        if (this.abort) {
            opts.signal = this.abort.signal;
        }

        fetch(url + '&nocache=' + Date.now(), opts).then(function(response) {
            if (self.closed || track.done) {
                return;
            }
            var ctype = response.headers.get('content-type') || '';
            if (!response.ok) {
                self.trackError(track, 'Server error ' + response.status);
                return;
            }
            if (ctype.indexOf(track.kind === 'video' ? 'video/h264' : 'audio/aac') < 0) {
                self.trackError(track, 'Unexpected response from cam');
                return;
            }
            if (track.kind === 'video') {
                setStatus('Waiting for key frame...');
            }
            track.reader = response.body.getReader();
            self.read(track);
        })['catch'](function(err) {
            self.trackError(track, 'Connection error');
        });
    };

    Session.prototype.read = function(track) {
        var self = this;
        track.reader.read().then(function(result) {
            if (self.closed || track.done) {
                return;
            }
            if (result.done) {
                self.trackError(track, 'Stream ended');
                return;
            }
            try {
                if (track.kind === 'video') {
                    self.parser.push(result.value, function(nal) { self.onNal(nal); });
                } else {
                    self.adts.push(result.value, function(frame) { self.onAudioFrame(frame); });
                }
            } catch (e) {
                if (track.kind === 'audio' && self.dropAudio(e.message)) {
                    return;
                }
                self.handleError(e);
                return;
            }
            self.read(track);
        })['catch'](function(err) {
            self.trackError(track, 'Connection lost');
        });
    };

    /* Group NAL units into access units (frames) */
    Session.prototype.onNal = function(nal) {
        if (nal.length < 2) {
            return;
        }
        if (this.sps === null && this.video.muxer === null && nal[0] === 0x40) {
            // 0x40 0x01 is an H.265 VPS: not playable through this path
            throw fatalError('This stream is H.265: not supported by the web player, choose the low resolution');
        }
        var type = nal[0] & 0x1f;
        var isVcl = (type === 1 || type === 5);
        var newAu;

        if (isVcl) {
            // first_mb_in_slice == 0 (ue(v) "1") starts a new picture
            newAu = this.auHasVcl && ((nal[1] & 0x80) !== 0);
        } else {
            // SEI, SPS, PPS, AUD after a slice start a new access unit
            newAu = this.auHasVcl && (type === 6 || type === 7 || type === 8 || type === 9);
        }
        if (newAu) {
            this.flushAu();
        }
        if (this.auTime === null && type !== 9) {
            // Arrival time of the frame (it's complete only when the next one starts)
            this.auTime = Date.now();
        }

        if (type === 7) {
            this.sps = nal;
        } else if (type === 8) {
            this.pps = nal;
        } else if (type !== 9) {
            this.au.push(nal);
            if (isVcl) {
                this.auHasVcl = true;
            }
        }
    };

    Session.prototype.flushAu = function() {
        var nals = this.au;
        var track = this.video;
        var now = this.auTime || Date.now();
        this.au = [];
        this.auHasVcl = false;
        this.auTime = null;

        var key = false;
        var size = 0;
        var i;
        for (i = 0; i < nals.length; i++) {
            if ((nals[i][0] & 0x1f) === 5) {
                key = true;
            }
            size += 4 + nals[i].length;
        }

        if (key && this.sps && this.pps) {
            if (track.muxer === null) {
                this.initVideo();
            } else if (!bytesEqual(track.muxer.sps, this.sps) || !bytesEqual(track.muxer.pps, this.pps)) {
                // Parameters changed (e.g. resolution): restart cleanly
                throw new Error('Stream parameters changed');
            }
            this.waitIdr = false;
        }
        if (this.waitIdr || track.muxer === null) {
            return;
        }

        // Sample data: NAL units with a 4 bytes length
        var data = new Uint8Array(size);
        var p = 0;
        for (i = 0; i < nals.length; i++) {
            data.set(u32(nals[i].length), p);
            data.set(nals[i], p + 4);
            p += 4 + nals[i].length;
        }

        this.arrivals.push(now);
        if (this.arrivals.length > 41) {
            this.arrivals.shift();
        }
        this.anchor(track.muxer, now);
        this.push(track, track.muxer.fragment(data, key, this.frameDuration(now)));
        this.frames++;
    };

    /*
     * Frame duration from the average arrival rate of the last frames,
     * slightly corrected to follow the common clock (the audio uses it too).
     */
    Session.prototype.frameDuration = function(now) {
        var fps = 20;
        var n = this.arrivals.length;
        if (n >= 10) {
            var span = this.arrivals[n - 1] - this.arrivals[0];
            if (span > 0) {
                fps = (n - 1) * 1000 / span;
            }
        }
        fps = Math.max(5, Math.min(30, fps));
        var duration = VIDEO_TIMESCALE / fps;

        var drift = (now - this.t0) * VIDEO_TIMESCALE / 1000 - this.video.muxer.dts;
        var corr = Math.max(-duration / 4, Math.min(duration / 4, drift / 20));
        return Math.round(duration + corr);
    };

    /* The first fragment of a track starts at the time it arrived on the common clock */
    Session.prototype.anchor = function(muxer, now) {
        if (!muxer.anchored) {
            muxer.dts = Math.round((now - this.t0) * muxer.timescale / 1000);
            muxer.anchored = true;
        }
    };

    Session.prototype.initVideo = function() {
        var info = parseSps(this.sps);
        var codec = 'avc1.' + hex2(this.sps[1]) + hex2(this.sps[2]) + hex2(this.sps[3]);
        var mime = 'video/mp4; codecs="' + codec + '"';

        if (!MSE.isTypeSupported(mime)) {
            throw fatalError('Codec ' + codec + ' not supported by this browser');
        }
        this.video.mime = mime;
        this.video.muxer = new Mp4Muxer({
            kind: 'video',
            timescale: VIDEO_TIMESCALE,
            width: info.width,
            height: info.height,
            sps: this.sps,
            pps: this.pps
        });
        this.video.muxer.sps = this.sps;
        this.video.muxer.pps = this.pps;
        this.info = info.width + 'x' + info.height;

        var self = this;
        if (this.audio && this.audio.mime === null) {
            setStatus('Waiting for audio...');
            this.audioTimer = setTimeout(function() {
                self.dropAudio('timeout');
            }, AUDIO_WAIT);
        }
        this.trySetup();
    };

    Session.prototype.onAudioFrame = function(frame) {
        var track = this.audio;
        if (!track) {
            return;
        }
        if (track.muxer === null) {
            var mime = 'audio/mp4; codecs="mp4a.40.' + frame.objectType + '"';
            if (!MSE.isTypeSupported(mime)) {
                throw new Error('Audio codec not supported by this browser');
            }
            track.mime = mime;
            track.muxer = new Mp4Muxer({
                kind: 'audio',
                timescale: frame.rate,
                rate: frame.rate,
                channels: frame.channels,
                config: frame.config
            });
            this.audioInfo = frame.rate + ' Hz';
            this.trySetup();
        }
        this.anchor(track.muxer, Date.now());
        this.push(track, track.muxer.fragment(frame.data, true, frame.samples));
    };

    Session.prototype.trySetup = function() {
        var self = this;
        var i, t;

        if (this.setupDone || this.closed) {
            return;
        }
        for (i = 0; i < this.tracks.length; i++) {
            if (this.tracks[i].mime === null) {
                return;
            }
        }
        if (this.audioTimer) {
            clearTimeout(this.audioTimer);
            this.audioTimer = null;
        }
        this.setupDone = true;

        try {
            for (i = 0; i < this.tracks.length; i++) {
                t = this.tracks[i];
                t.sb = this.ms.addSourceBuffer(t.mime);
                t.sb.mode = 'segments';
            }
        } catch (e) {
            this.fail('Playback error');
            return;
        }
        for (i = 0; i < this.tracks.length; i++) {
            (function(track) {
                track.sb.addEventListener('updateend', function() { self.drain(track); });
                track.sb.addEventListener('error', function() { self.fail('Playback error'); });
                track.queue.unshift(track.muxer.initSegment());
                self.drain(track);
            })(this.tracks[i]);
        }

        setStatus('Live - ' + this.info + (this.audio ? ' - audio ' + this.audioInfo : '') + this.note);

        var p = video.play();
        if (p && p['catch']) {
            p['catch'](function() {
                // Autoplay with sound blocked: play muted
                if (!video.muted) {
                    video.muted = true;
                    setStatus('Live - ' + self.info + ' - audio blocked by the browser, press Play');
                    var p2 = video.play();
                    if (p2 && p2['catch']) {
                        p2['catch'](function() {});
                    }
                } else {
                    setStatus('Live - ' + self.info + ' (press Play)');
                }
            });
        }
    };

    Session.prototype.push = function(track, data) {
        track.queue.push(data);
        this.drain(track);
    };

    Session.prototype.drain = function(track) {
        var sb = track.sb;
        if (this.closed || !sb || sb.updating || track.queue.length === 0) {
            return;
        }
        var data = (track.queue.length === 1) ? track.queue[0] : concat(track.queue);
        track.queue = [];
        try {
            sb.appendBuffer(data);
        } catch (e) {
            if (e.name === 'QuotaExceededError') {
                try { sb.remove(0, Math.max(0, video.currentTime - 2)); } catch (e2) {}
            } else {
                this.fail('Playback error');
            }
        }
    };

    /* Keep the latency low and the buffers small */
    Session.prototype.housekeeping = function() {
        var i, sb;
        if (this.closed || !this.setupDone) {
            return;
        }
        // video.buffered: the time ranges where all the tracks have data
        var b = video.buffered;
        if (b.length === 0) {
            return;
        }
        var start = b.start(b.length - 1);
        var end = b.end(b.length - 1);
        var lag = end - video.currentTime;

        if (video.currentTime < start || lag > 3) {
            video.currentTime = Math.max(start, end - 0.3);
        } else if (lag > 1) {
            video.playbackRate = 1.1;
        } else if (lag < 0.5) {
            video.playbackRate = 1.0;
        }
        if (video.paused && !video.ended && video.readyState >= 2 && this.frames > 0) {
            var p = video.play();
            if (p && p['catch']) {
                p['catch'](function() {});
            }
        }
        for (i = 0; i < this.tracks.length; i++) {
            sb = this.tracks[i].sb;
            if (sb && !sb.updating && sb.buffered.length > 0 && video.currentTime - sb.buffered.start(0) > 20) {
                try {
                    sb.remove(sb.buffered.start(0), video.currentTime - 10);
                } catch (e) {}
            }
        }
    };

    /* ------------------------------------------------------------------ */
    /* H.264 Annex-B parser                                                */
    /* ------------------------------------------------------------------ */

    function AnnexBParser() {
        this.buf = new Uint8Array(0);
        this.scan = 0;         // where to resume the start code search
        this.nalStart = -1;    // start of the current NAL payload
    }

    /* Calls onNal(Uint8Array) for every complete NAL unit (without start code) */
    AnnexBParser.prototype.push = function(chunk, onNal) {
        var buf = append(this.buf, chunk);
        var i = this.scan;
        var nalStart = this.nalStart;
        var len = buf.length;

        while (i + 2 < len) {
            if (buf[i + 2] > 1) {
                i += 3;
            } else if (buf[i] === 0 && buf[i + 1] === 0 && buf[i + 2] === 1) {
                if (nalStart >= 0) {
                    var end = i;
                    while (end > nalStart && buf[end - 1] === 0) {
                        end--;
                    }
                    if (end > nalStart) {
                        onNal(buf.slice(nalStart, end));
                    }
                }
                i += 3;
                nalStart = i;
            } else {
                i++;
            }
        }

        // Keep only the unfinished NAL unit (or the tail if none started)
        var keep = (nalStart >= 0) ? nalStart : Math.max(0, len - 3);
        this.buf = buf.slice(keep);
        this.scan = Math.max(0, i - keep);
        this.nalStart = (nalStart >= 0) ? 0 : -1;

        if (this.buf.length > 4 * 1024 * 1024) {
            throw new Error('Invalid stream');
        }
    };

    /* ------------------------------------------------------------------ */
    /* AAC ADTS parser                                                     */
    /* ------------------------------------------------------------------ */

    function AdtsParser() {
        this.buf = new Uint8Array(0);
    }

    /* Calls onFrame({data, rate, channels, objectType, config, samples}) for every complete frame */
    AdtsParser.prototype.push = function(chunk, onFrame) {
        var buf = append(this.buf, chunk);
        var len = buf.length;
        var pos = 0;

        while (pos + 7 <= len) {
            // Syncword 0xFFF, layer 0
            if (buf[pos] !== 0xff || (buf[pos + 1] & 0xf6) !== 0xf0) {
                pos++;
                continue;
            }
            var protectionAbsent = buf[pos + 1] & 0x01;
            var profile = (buf[pos + 2] >> 6) & 0x03;
            var rateIdx = (buf[pos + 2] >> 2) & 0x0f;
            var channels = ((buf[pos + 2] & 0x01) << 2) | ((buf[pos + 3] >> 6) & 0x03);
            var frameLen = ((buf[pos + 3] & 0x03) << 11) | (buf[pos + 4] << 3) | ((buf[pos + 5] >> 5) & 0x07);
            var blocks = (buf[pos + 6] & 0x03) + 1;
            var hdrLen = protectionAbsent ? 7 : 9;

            if (rateIdx >= AAC_RATES.length || frameLen <= hdrLen) {
                pos++;
                continue;
            }
            if (pos + frameLen > len) {
                break;
            }
            var objectType = profile + 1;
            if (channels === 0) {
                channels = 1;
            }
            onFrame({
                data: buf.slice(pos + hdrLen, pos + frameLen),
                rate: AAC_RATES[rateIdx],
                channels: channels,
                objectType: objectType,
                // AudioSpecificConfig
                config: [(objectType << 3) | (rateIdx >> 1), ((rateIdx & 1) << 7) | (channels << 3)],
                samples: 1024 * blocks
            });
            pos += frameLen;
        }
        this.buf = buf.slice(pos);

        if (this.buf.length > 64 * 1024) {
            throw new Error('Invalid audio stream');
        }
    };

    /* ------------------------------------------------------------------ */
    /* SPS parser (only what is needed: the picture size)                  */
    /* ------------------------------------------------------------------ */

    function BitReader(data) {
        // Remove emulation prevention bytes (00 00 03)
        var out = [];
        for (var i = 0; i < data.length; i++) {
            if (i >= 2 && data[i] === 3 && data[i - 1] === 0 && data[i - 2] === 0) {
                continue;
            }
            out.push(data[i]);
        }
        this.data = out;
        this.pos = 0;
    }

    BitReader.prototype.u1 = function() {
        var b = this.data[this.pos >> 3];
        if (b === undefined) {
            throw new Error('Invalid SPS');
        }
        var bit = (b >> (7 - (this.pos & 7))) & 1;
        this.pos++;
        return bit;
    };

    BitReader.prototype.u = function(n) {
        var v = 0;
        for (var i = 0; i < n; i++) {
            v = v * 2 + this.u1();
        }
        return v;
    };

    BitReader.prototype.ue = function() {
        var lz = 0;
        while (this.u1() === 0) {
            lz++;
            if (lz > 31) {
                throw new Error('Invalid SPS');
            }
        }
        return Math.pow(2, lz) - 1 + this.u(lz);
    };

    BitReader.prototype.se = function() {
        var v = this.ue();
        return (v & 1) ? (v + 1) / 2 : -v / 2;
    };

    function parseSps(sps) {
        var r = new BitReader(sps);
        r.u(8);                       // NAL header
        var profile = r.u(8);
        r.u(8);                       // constraint flags
        r.u(8);                       // level
        r.ue();                       // seq_parameter_set_id
        var chroma = 1;
        if ([100, 110, 122, 244, 44, 83, 86, 118, 128, 138, 139, 134, 135].indexOf(profile) >= 0) {
            chroma = r.ue();
            if (chroma === 3) {
                r.u1();               // separate_colour_plane_flag
            }
            r.ue();                   // bit_depth_luma_minus8
            r.ue();                   // bit_depth_chroma_minus8
            r.u1();                   // qpprime_y_zero_transform_bypass_flag
            if (r.u1()) {             // seq_scaling_matrix_present_flag
                var lists = (chroma !== 3) ? 8 : 12;
                for (var i = 0; i < lists; i++) {
                    if (r.u1()) {
                        var size = (i < 6) ? 16 : 64;
                        var last = 8, next = 8;
                        for (var j = 0; j < size; j++) {
                            if (next !== 0) {
                                next = (last + r.se() + 256) % 256;
                            }
                            last = (next === 0) ? last : next;
                        }
                    }
                }
            }
        }
        r.ue();                       // log2_max_frame_num_minus4
        var pocType = r.ue();
        if (pocType === 0) {
            r.ue();                   // log2_max_pic_order_cnt_lsb_minus4
        } else if (pocType === 1) {
            r.u1();
            r.se();
            r.se();
            var n = r.ue();
            for (var k = 0; k < n; k++) {
                r.se();
            }
        }
        r.ue();                       // max_num_ref_frames
        r.u1();                       // gaps_in_frame_num_value_allowed_flag
        var wMbs = r.ue() + 1;
        var hMaps = r.ue() + 1;
        var frameMbsOnly = r.u1();
        if (!frameMbsOnly) {
            r.u1();                   // mb_adaptive_frame_field_flag
        }
        r.u1();                       // direct_8x8_inference_flag
        var cl = 0, cr = 0, ct = 0, cb = 0;
        if (r.u1()) {                 // frame_cropping_flag
            cl = r.ue();
            cr = r.ue();
            ct = r.ue();
            cb = r.ue();
        }
        var cropX = (chroma === 0 || chroma === 3) ? 1 : 2;
        var cropY = ((chroma === 1) ? 2 : 1) * (2 - frameMbsOnly);
        return {
            width: wMbs * 16 - (cl + cr) * cropX,
            height: (2 - frameMbsOnly) * hMaps * 16 - (ct + cb) * cropY
        };
    }

    /* ------------------------------------------------------------------ */
    /* Fragmented MP4 muxer (one track: H.264 video or AAC audio)          */
    /* ------------------------------------------------------------------ */

    function Mp4Muxer(track) {
        this.track = track;
        this.timescale = track.timescale;
        this.seq = 1;
        this.dts = 0;
        this.anchored = false;
    }

    function u8(v) { return [v & 0xff]; }
    function u16(v) { return [(v >> 8) & 0xff, v & 0xff]; }
    function u32(v) { return [(v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff]; }
    function u64(v) { return u32(Math.floor(v / 4294967296)).concat(u32(v % 4294967296)); }
    function fourcc(s) { return [s.charCodeAt(0), s.charCodeAt(1), s.charCodeAt(2), s.charCodeAt(3)]; }
    function zeros(n) { var a = []; for (var i = 0; i < n; i++) { a.push(0); } return a; }
    function arr(typed) { return Array.prototype.slice.call(typed); }
    function text(s) { var a = []; for (var i = 0; i < s.length; i++) { a.push(s.charCodeAt(i)); } a.push(0); return a; }

    /* box(type, part1, part2, ...) - parts are arrays of bytes */
    function box(type) {
        var payload = [];
        for (var i = 1; i < arguments.length; i++) {
            payload = payload.concat(arguments[i]);
        }
        return u32(payload.length + 8).concat(fourcc(type), payload);
    }

    function fullbox(type, version, flags) {
        var args = [type, u8(version).concat(u32(flags).slice(1))];
        for (var i = 3; i < arguments.length; i++) {
            args.push(arguments[i]);
        }
        return box.apply(null, args);
    }

    /* MPEG-4 descriptor (esds), payload < 128 bytes */
    function descriptor(tag) {
        var payload = [];
        for (var i = 1; i < arguments.length; i++) {
            payload = payload.concat(arguments[i]);
        }
        return [tag, payload.length].concat(payload);
    }

    var MATRIX = [].concat(u32(0x00010000), u32(0), u32(0),
                           u32(0), u32(0x00010000), u32(0),
                           u32(0), u32(0), u32(0x40000000));

    Mp4Muxer.prototype.sampleEntry = function() {
        var t = this.track;
        if (t.kind === 'video') {
            var avcC = box('avcC',
                u8(1), u8(t.sps[1]), u8(t.sps[2]), u8(t.sps[3]), u8(0xff),
                u8(0xe1), u16(t.sps.length), arr(t.sps),
                u8(1), u16(t.pps.length), arr(t.pps));

            return box('avc1',
                zeros(6), u16(1), zeros(16), u16(t.width), u16(t.height),
                u32(0x00480000), u32(0x00480000), u32(0), u16(1),
                zeros(32), u16(0x18), u16(0xffff), avcC);
        }

        var esds = fullbox('esds', 0, 0,
            descriptor(0x03, u16(1), u8(0),
                descriptor(0x04, u8(0x40), u8(0x15), zeros(3), u32(0), u32(0),
                    descriptor(0x05, t.config)),
                descriptor(0x06, u8(0x02))));

        return box('mp4a',
            zeros(6), u16(1), zeros(8), u16(t.channels), u16(16), u16(0), u16(0),
            u32(t.rate * 65536), esds);
    };

    Mp4Muxer.prototype.initSegment = function() {
        var t = this.track;
        var isVideo = (t.kind === 'video');
        var w = isVideo ? t.width : 0;
        var h = isVideo ? t.height : 0;

        var ftyp = box('ftyp', fourcc('isom'), u32(1),
                       fourcc('isom'), fourcc('iso6'), fourcc(isVideo ? 'avc1' : 'mp41'), fourcc('mp41'));

        var mvhd = fullbox('mvhd', 0, 0,
            u32(0), u32(0), u32(1000), u32(0),
            u32(0x00010000), u16(0x0100), zeros(10), MATRIX, zeros(24), u32(2));

        var tkhd = fullbox('tkhd', 0, 3,
            u32(0), u32(0), u32(1), u32(0), u32(0), zeros(8),
            u16(0), u16(0), u16(isVideo ? 0 : 0x0100), u16(0), MATRIX, u32(w * 65536), u32(h * 65536));

        var mdhd = fullbox('mdhd', 0, 0,
            u32(0), u32(0), u32(this.timescale), u32(0), u16(0x55c4), u16(0));

        var hdlr = fullbox('hdlr', 0, 0,
            u32(0), fourcc(isVideo ? 'vide' : 'soun'), zeros(12), text(isVideo ? 'Video' : 'Audio'));

        var mediaHeader = isVideo ? fullbox('vmhd', 0, 1, u16(0), zeros(6)) : fullbox('smhd', 0, 0, u16(0), u16(0));

        var stbl = box('stbl',
            fullbox('stsd', 0, 0, u32(1), this.sampleEntry()),
            fullbox('stts', 0, 0, u32(0)),
            fullbox('stsc', 0, 0, u32(0)),
            fullbox('stsz', 0, 0, u32(0), u32(0)),
            fullbox('stco', 0, 0, u32(0)));

        var minf = box('minf',
            mediaHeader,
            box('dinf', fullbox('dref', 0, 0, u32(1), fullbox('url ', 0, 1))),
            stbl);

        var moov = box('moov',
            mvhd,
            box('trak', tkhd, box('mdia', mdhd, hdlr, minf)),
            box('mvex', fullbox('trex', 0, 0, u32(1), u32(1), u32(0), u32(0), u32(0))));

        return new Uint8Array(ftyp.concat(moov));
    };

    /* One moof+mdat with one sample */
    Mp4Muxer.prototype.fragment = function(data, key, duration) {
        var sampleFlags = key ? 0x02000000 : 0x01010000;
        var trun = function(offset) {
            return fullbox('trun', 0, 0x000701,
                u32(1), u32(offset), u32(duration), u32(data.length), u32(sampleFlags));
        };
        var build = function(offset, seq, dts) {
            return box('moof',
                fullbox('mfhd', 0, 0, u32(seq)),
                box('traf',
                    fullbox('tfhd', 0, 0x020000, u32(1)),
                    fullbox('tfdt', 1, 0, u64(dts)),
                    trun(offset)));
        };
        // data_offset: from the start of moof to the sample data in mdat
        var moofLen = build(0, this.seq, this.dts).length;
        var moof = build(moofLen + 8, this.seq, this.dts);

        var out = new Uint8Array(moof.length + 8 + data.length);
        out.set(moof, 0);
        out.set(u32(data.length + 8), moof.length);
        out.set(fourcc('mdat'), moof.length + 4);
        out.set(data, moof.length + 8);

        this.seq++;
        this.dts += duration;
        return out;
    };

    /* ------------------------------------------------------------------ */
    /* Helpers                                                             */
    /* ------------------------------------------------------------------ */

    function fatalError(message) {
        var e = new Error(message);
        e.fatal = true;
        return e;
    }

    function hex2(v) {
        return ('0' + v.toString(16)).slice(-2).toUpperCase();
    }

    function bytesEqual(a, b) {
        if (a.length !== b.length) {
            return false;
        }
        for (var i = 0; i < a.length; i++) {
            if (a[i] !== b[i]) {
                return false;
            }
        }
        return true;
    }

    function append(a, b) {
        if (a.length === 0) {
            return b;
        }
        var out = new Uint8Array(a.length + b.length);
        out.set(a, 0);
        out.set(b, a.length);
        return out;
    }

    function concat(list) {
        var len = 0, i;
        for (i = 0; i < list.length; i++) {
            len += list[i].length;
        }
        var out = new Uint8Array(len);
        var p = 0;
        for (i = 0; i < list.length; i++) {
            out.set(list[i], p);
            p += list[i].length;
        }
        return out;
    }

    return {
        init: init,
        // exported for testing
        _AnnexBParser: AnnexBParser,
        _AdtsParser: AdtsParser,
        _parseSps: parseSps,
        _Mp4Muxer: Mp4Muxer
    };

})(jQuery);
