<p align="center">
	<img height="200" src="https://user-images.githubusercontent.com/39277388/96489837-43d26180-1240-11eb-9d0e-5cfa84040fe1.png">
</p>

<p align="center">
	<a target="_blank" href="https://github.com/roleoroleo/yi-hack-Allwinner-v2/releases">
		<img src="https://img.shields.io/github/downloads/roleoroleo/yi-hack-Allwinner-v2/total.svg" alt="Releases Downloads">
	</a>
</p>

yi-hack-Allwinner-v2 is a modification of the firmware for the Allwinner-based Yi Camera platform.
What's the difference between v1 and v2? Allwinner-v2 is not an upgrade for Allwinner, it's a version dedicated to a different "family". Same cpu but different flash layout.

## Table of Contents
- [Table of Contents](#table-of-contents)
- [Installation](#installation)
- [Contributing](#contributing-and-bug-reports)
- [Features](#features)
- [Performance](#performance)
- [Supported cameras](#supported-cameras)
- [Is my cam supported?](#is-my-cam-supported)
- [Home Assistant integration](#home-assistant-integration)
- [Frigate integration](#frigate-integration)
- [Build your own firmware](#build-your-own-firmware)
- [Unbricking](#unbricking)
- [License](#license)
- [Disclaimer](#disclaimer)
- [Donation](#donation)


## Installation

### Backup
It's not easy to brick the cam but it can happen.
So please, make your backup copy: https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Dump-your-backup-firmware-(SD-card)

Anyway, the hack procedure will create a backup for you.

### Install Procedure
If you want to use the original Yi app, please install it and complete the pairing process before installing the hack.

Otherwise, check setep 4.

1. Format an SD Card as FAT32. It's recommended to format the card in the camera using the camera's native format function. If the card is already formatted, remove all the files.
2. Download the latest release from the Releases page.
3. Extract the contents of the archive to the root of your SD card. Your card should appear with this structure:
```
|-- Factory/
|-- yi-hack/
|-- lower_half_init.sh
```
4. (Optional) If you want to set wifi credentials, rename the file Factory/configure_wifi.cfg.ori to Factory/configure_wifi.cfg and edit the file with your username and password.
5. Insert the SD Card and reboot the camera
6. Wait a minute for the camera to update.
7. Check the hack opening the web interface http://IP-CAM (where IP-CAM is the IP address of the cam assigned by your router).
8. Don't remove the microSD card (yes this hack requires a dedicated microSD card).
9. Check the FAQ if you have a problem: https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/FAQ


### Online Update Procedure
1. Go to the "Maintenance" web page
2. Check if a new release is available
3. Click "Upgrade Firmware"
4. Wait for cam reboot


### Manual Update Procedure
Check the wiki: https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Manual-firmware-upgrade


### Optional Utilities 
Several [optional utilities](https://github.com/roleoroleo/yi-hack-utils) are avaiable, some supporting experimental features like text-to-speech.


## Contributing and Bug Reports
See [CONTRIBUTING](CONTRIBUTING.md)

---

## Features
This custom firmware contains features replicated from the [yi-hack-MStar](https://github.com/roleoroleo/yi-hack-MStar) project and similar to the [yi-hack-v4](https://github.com/TheCrypt0/yi-hack-v4) project.

- FEATURES
  - RTSP server - allows a RTSP stream of the video (high and/or low resolution) and audio (thanks to @PieVo for the work on MStar platform).
    - rtsp://IP-CAM/ch0_0.h264             (high res)
    - rtsp://IP-CAM/ch0_1.h264             (low res)
    - rtsp://IP-CAM/ch0_2.h264             (only audio)
  - ONVIF server (with support for stream, snapshot, ptz, presets, events and WS-Discovery) - standardized interfaces for IP cameras.
  - Snapshot service - allows to get a jpg with a web request.
    - http://IP-CAM/cgi-bin/snapshot.sh?res=low&watermark=yes        (select resolution: low or high, and watermark: yes or no)
    - http://IP-CAM/cgi-bin/snapshot.sh                              (default high without watermark)
  - Timelapse feature
  - MQTT events - Motion detection and baby crying detection through mqtt protocol.
  - MQTT configuration
  - TLS support for MQTT
  - Web server - web configuration interface.
  - SSH server - dropbear.
  - Telnet server - busybox.
  - FTP server.
  - FTP push: export mp4 video to an FTP server (thanks to @Catfriend1).
  - Authentication for HTTP, RTSP and ONVIF server.
  - Proxychains-ng - Disabled by default. Useful if the camera is region locked.
  - The possibility to change some camera settings (copied from official app):
    - camera on/off
    - video saving mode
    - detection sensitivity
    - motion detections (it depends on your cam and your plan)
    - baby crying detection
    - status led
    - ir led
    - rotate
    - ...
  - Management of motion detect events and videos through a web page.
  - View recorded video through a web page (thanks to @BenjaminFaal).
  - PTZ support through a web page (if the cam supports it).
  - PTZ presets.
  - The possibility to disable all the cloud features.
  - Swap File on SD.
  - Online firmware upgrade.
  - Load/save/reset configuration.


## Performance

The performance of the cam is not so good (CPU, RAM, etc...). Low ram is the bigger problem.
If you enable all the services you may have some problems.
For example, enabling snapshots may cause frequent reboots.
So, **enable swap file** even if this will waste the sd


## Supported cameras

Currently this project supports only the following cameras:

| Camera | SN prefix | Firmware | File prefix | Remarks |
| --- | --- | --- | --- | --- |
| Yi 1080p Home | BFUS - IFUS - RFUS | 9.0.19* | y21ga | - |
| Yi 1080p Home | BFUS - IFUS - RFUS | 12.1.19* | y21ga | - |
| Yi 1080p Home | IFUS - QFUS - RFUS | 9.0.36* | y211ga | - |
| Yi 1080p Home | IFUS - QFUS - RFUS | 12.0.37* | y211ga | - |
| Yi 1080p Home | IFUS - QFUS - RFUS | 12.1.37* | y211ga | - |
| Yi 1080p Home | QFUS - RFUS | 9.0.35* | y291ga | - |
| Yi 1080p Home | QFUS - RFUS | 12.0.35* | y291ga | - |
| Yi Outdoor 1080p | IFUS - RFUS | 9.0.26* | h30ga | - |
| Yi Outdoor 1080p | IFUS - RFUS | 11.1.26* | h30ga | - |
| Yi 1080p Dome | *FUS | 9.0.05* | r30gb | beta version (check this issue https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/484) |
| Yi 1080p Dome | *FUS | 12.1.05* | r30gb | beta version (check this issue https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/484) |
| Yi Dome Guard | YRS | 9.0.05* | r30gb | beta version (check this issue https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/484) |
| Yi Dome Camera U (Full HD) | BFUS - SFUS | 9.0.22* | h52ga | - |
| Yi Dome Camera U (2K) | BFUS - SFUS | 9.0.21* | h51ga | - |
| Yi Dome U Pro 2K | LFUS | 9.0.27* | h60ga | - |
| Yi Outdoor 1080p | QFUS | 9.0.45* | r40ga | - |
| Yi Home Y4 | IFCN | 9.0.09* | y29ga | - |
| Yi Dome Guard | QFUS | 9.0.46* | r35gb | - |
| Yi Dome Guard | QFUS | 12.1.47* | r35gb | - |
| Yi Dome Guard | YRS | 9.0.46* | r35gb | - |
| Yi Dome Guard | YRS | 12.1.47* | r35gb or r37gb | https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/1156 |
| Yi Dome Guard | RFUS | 12.1.47* | r35gb or r37gb | https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/1156 |
| Yi Pro 2K Home | RFUS - YFUS - ZFUS | 12.0.51* | y623 | - |
| Kami mini home | IFUS | 9.0.20* | y28ga | - |
| MIBAO G1 1296p dome | - | 9.0.04* | qg311r | - |
| BLITZWOLF BW-YIC1 | - | 9.0.41* | b091qp | - |
| ESCAM PT202 | - | 9.0.41* | b091qp | https://github.com/roleoroleo/yi-hack-Allwinner-v2/discussions/624#discussioncomment-5816561 |
| YS-QC-02 | - | 9.0.41* | b091qp | https://github.com/roleoroleo/yi-hack_ha_integration/issues/84 |
| Flood Light Camera Outdoor L850Y-US | - | 9.0.41* | b091qp | - |
| Tovendor Mini Smart Home Camera | - | 9.0.41* | b091qp | - |

USE AT YOUR OWN RISK.

**Do not try to use a fw on an unlisted model**

**Do not try to force the fw loading renaming the files**


## Is my cam supported?

If you want to know if your cam is supported, please check the serial number (first 4 letters) and the firmware version.
If both numbers appear in the same row in the table above, your cam is supported.
If not, check the other projects related to Yi cams:
- https://github.com/TheCrypt0/yi-hack-v4 and previous
- https://github.com/alienatedsec/yi-hack-v5
- https://github.com/roleoroleo/yi-hack-MStar
- https://github.com/roleoroleo/yi-hack-Allwinner


## Home Assistant integration
Are you using Home Assistant? Do you want to integrate your cam? Try these custom integrations:
- https://github.com/roleoroleo/yi-hack_ha_integration
- https://github.com/AlexxIT/WebRTC

You can also use the [web services](https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Web-services-description) in Home Assistant -- here's one way to do that. (This example requires the nanotts optional utility to be installed on the camera.) Set up a rest_command in your configuration.yaml to call one of the [web services](https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Web-services-description). 
```
rest_command:
  camera_announce:
    url: http://[camera address]/cgi-bin/speak.sh?lang={{language}}&voldb={{volume}}
    method: POST
    payload: "{{message}}"
```
Create an automation and use yaml in the action to send data to the web service. 
```
service: rest_command.camera_announce
data:
  language: en-US
  message: "All your base are belong to us."
  volume: '-8'
``` 

## Frigate integration

Frigate can consume the RTSP streams directly. Enable the RTSP server in the camera web interface and use one of these stream URLs:

- `rtsp://IP-CAM/ch0_0.h264` - high resolution
- `rtsp://IP-CAM/ch0_1.h264` - low resolution
- `rtsp://IP-CAM/ch0_2.h264` - audio only

For a minimal setup, use the low resolution stream for detection to reduce load on the camera:

```yaml
cameras:
  yi_camera:
    ffmpeg:
      inputs:
        - path: rtsp://IP-CAM/ch0_1.h264
          roles:
            - detect
```

If you need a higher quality stream for recording, add the high resolution stream as a separate input:

```yaml
cameras:
  yi_camera:
    ffmpeg:
      inputs:
        - path: rtsp://IP-CAM/ch0_1.h264
          roles:
            - detect
        - path: rtsp://IP-CAM/ch0_0.h264
          roles:
            - record
```

If authentication is enabled in the camera web interface, include the configured username and password in the RTSP URL:

```yaml
path: rtsp://user:password@IP-CAM/ch0_1.h264
```

### PTZ through ONVIF

For models with PTZ support, enable the ONVIF server in the camera web interface and add an `onvif` section to the Frigate camera configuration. The ONVIF port is the camera HTTP port, usually `80` unless you changed it.

```yaml
cameras:
  yi_ptz_camera:
    ffmpeg:
      inputs:
        - path: rtsp://IP-CAM/ch0_1.h264
          roles:
            - detect
    onvif:
      host: IP-CAM
      port: 80
      user: ""
      password: ""
```

If ONVIF authentication is enabled, replace the empty strings with the camera credentials. If ONVIF authentication is disabled, keep `user: ""` and `password: ""`; some Frigate versions expect these keys even when the camera allows anonymous ONVIF access.

Frigate shows PTZ controls only when its ONVIF connection succeeds and the camera model exposes PTZ commands. Not all supported cameras have PTZ hardware.

### Notes

- These cameras have limited CPU and RAM. Avoid enabling more camera-side services than needed.
- If the camera becomes unstable, enable the swap file and prefer a single low resolution stream for detection.
- Snapshots and multiple simultaneous streams may increase memory pressure.

## Telegram Control System

A complete remote surveillance and control system: control your camera, receive automatic alerts, and communicate bidirectionally — all through a Telegram bot.
Features
	- On-demand snapshot and video recording via Telegram commands
	- Automatic motion alerts with photo
	- Automatic sound alerts with snapshot + 15s audio clip
	- Bidirectional voice intercom — speak through Telegram, hear through the camera speaker
	- Blue LED control — turn on/off remotely
	- Infrared control — turn on/off remotely
	- Silent mode — suppress notifications without stopping the watchers
	- System status — IP, uptime, memory, process PIDs
	- Remote reboot
[Get scripts here](https://github.com/tingolinchi/yi-home-telegram).

## Build your own firmware

If you want to build your own firmware, clone this git and compile using a linux machine. Quick explanation:

1. Download and install the SDK as described [here](https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Build-your-own-firmware)
2. Clone this git: `git clone https://github.com/roleoroleo/yi-hack-Allwinner-v2`
3. Init modules: `git submodule update --init`
4. Compile: `./scripts/compile.sh`
5. Pack the firmware: `./scripts/pack_fw.all.sh`

Instead of installing the SDK on your host machine, there's also the option to use a [`devcontainer`](https://code.visualstudio.com/docs/remote/containers) from within [Visual Studio Code](https://code.visualstudio.com/). Please ensure you have the [`Remote - Containers`](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension installed for this to work.


## Unbricking

If your camera doesn't start, no panic. This hack is not a permanent change, remove your SD card and the cam will come back to the original state.
If the camera still won't start, try the "Unbrick the cam" procedure https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Unbrick-the-cam.

----

## How it works (architecture)

This section is a map for anyone hacking on the firmware. It describes the moving
parts and how they fit together, independent of any particular camera's settings.

### The big picture

The hack is a **non-destructive SD-card overlay** on top of the stock Yi
firmware. Nothing is flashed: the stock OS lives on the camera's NAND, and
everything the hack adds lives on the FAT32 SD card under `/tmp/sd`. Pull the
card and the camera boots stock. The only permanent change is a one-time patch to
the stock `/backup/init.sh` (done by `Factory/config.sh` on first boot) that hooks
the SD card's boot script into the stock startup.

Because RAM is tiny (~60 MB on most models), the guiding constraint everywhere is
**do less**: fewer processes, fewer copies of the stream, fewer wakeups.

### Boot sequence

```mermaid
flowchart TD
    A[Stock /backup/init.sh<br/>patched on first boot] --> B[lower_half_init.sh]
    B --> C[Mount SD at /tmp/sd<br/>load wifi + sensor drivers]
    B --> D[Yi 'dispatch' core starts<br/>ipc_multiplex.so preloaded]
    B --> E[system.sh &]
    E --> F[check_conf.sh<br/>seed + de-duplicate config]
    E --> G[swap on, /etc writable, hostname, TZ]
    E --> H[rmm + Yi pipeline<br/>drain_audio_fifo bounded]
    E --> I[mp4record<br/>only if CAMERA_RECORDING=yes]
    E --> J[httpd]
    E --> K[crond<br/>clean_records, enforce_settings, timelapse...]
    E --> L[mqtt / rtsp / onvif via service.sh]
    L --> M[wd.sh watchdog loop]
```

Key detail: **every network service starts after the Yi camera core (`rmm`) is
up.** The pipeline is primed by draining a few KB from `/tmp/audio_fifo`. That
drain is bounded (`drain_audio_fifo` in [system.sh](src/static/static/yi-hack/script/system.sh)) —
an unbounded read there will hang the whole boot before `httpd`/`rtsp` ever start
if `rmm` misbehaves.

### Where things live

| On the camera (`/tmp/sd/yi-hack/`) | In this repo (source of truth) |
| --- | --- |
| `script/*.sh` (runtime) | `src/static/static/yi-hack/script/` |
| `etc/*.conf` (config + defaults) | `src/static/static/yi-hack/etc/` |
| `bin/*` (compiled tools) | `src/<tool>/` (C sources) |
| `www/` (gzipped web UI) | `src/www/httpd/` (`htdocs` + `cgi-bin`) |

The web UI is **built and gzipped** by `src/www/compile.www`: HTML/JS/CSS are
concatenated then `gzip -9`'d, so the card only ever holds `*.gz`. Browsers send
`Accept-Encoding: gzip` and BusyBox `httpd` serves them transparently. `cgi-bin/`
scripts are copied **as-is** (not gzipped), so a new CGI can be dropped straight
onto the card.

### Configuration system

Three flat `KEY=VALUE` files under `etc/`:

- `system.conf` — services, ports, cloud, recording, swap, network.
- `camera.conf` — imaging: motion/AI detection, LED/IR, save-on-motion.
- `mqttv4.conf` — MQTT broker, topics, credentials.

Rules of the road:

- Read with `get_config KEY` — `grep KEY | cut -d= -f2-`. A **duplicate key
  therefore yields a multi-line value that breaks comparisons**, so
  `check_conf.sh` seeds any missing keys *and* de-duplicates (keeps the first
  occurrence) on every boot.
- The **Configurations** page writes `system.conf` via `cgi-bin/set_configs.sh`
  (a `sed` replace). Any `data-key` checkbox inside `.configs-switch` is loaded
  and saved automatically.
- The **Camera Settings** page mostly pushes state into the live Yi firmware via
  `ipc_cmd` (see `src/ipc_cmd/`); settings that must survive a reboot are also
  written back to `camera.conf`.

### Services

- **httpd** — BusyBox web server, doc root `www/`, basic auth from
  `/tmp/httpd.conf` (everything except `/onvif` requires the configured user).
- **RTSP** — `service.sh` selects one of three backends: `rRTSPServer`
  (standard), `rtsp_server_yi` (alternative), or `go2rtc`. All read H.264 from
  Yi's shared frame buffer through `h264grabber`, whose per-model memory offsets
  live in `src/h264grabber/`. Endpoints: `ch0_0.h264` (high), `ch0_1.h264` (low),
  `ch0_2.h264` (audio).
- **ONVIF** — `onvif_simple_server` + `wsd_simple_server` (WS-Discovery).
- **MQTT** — `mqttv4` publishes events; `mqtt_advertise/` adds Home Assistant
  auto-discovery, including the telemetry topic (uptime, load, CPU%, temperature,
  memory, swap, SD, wifi strength).
- **snapshot / timelapse** — on-demand JPEG and periodic AVI (cron-driven).
- **recording** — the `mp4record` binary is the only thing that writes MP4s to the
  card. RTSP is independent of it, so recording can be turned off (via
  `CAMERA_RECORDING`) without affecting live streaming.

### Supervision and self-healing

`wd.sh` runs a ~10 s loop and is the camera's immune system:

- restarts RTSP if the port stops listening or the daemon wedges;
- restarts `mqtt` and (new) `httpd` if they die;
- reboots if `rmm` (the Yi core) is gone for 5 checks;
- watches wifi with a **gentle recovery ladder** — a single missed sample does
  nothing; only after sustained loss does it ask the supplicant to reconnect,
  then reload config, then (last resort) bounce the interface, and only after
  several minutes reboot.

`enforce_settings.sh` (once at boot, hourly via cron) re-asserts settings flagged
`*_PERSIST=yes`, so the native app can't silently re-enable camera-side recording
or flip the save-on-motion mode.

### Logging

Important events are appended to `/tmp/sd/log/hack-YYYYMMDD.log` and kept for 7
days (pruned at boot and daily by cron), independent of `DEBUG_LOG`. This
survives power cycles, which is what makes post-mortems possible. View it in a
browser at `http://<cam>/cgi-bin/log.sh` (add `?log=wifi` for the wifi log). The
verbose `DEBUG_LOG` dump (`hack_debug.log`, includes `ps`) is deliberately *not*
exposed over HTTP because it can contain the RTSP password.

### Building

- `./scripts/compile.sh` — build all C tools and the web UI.
- `./scripts/pack_fw.all.sh` — assemble the installable `.tgz` per model.
- The runtime shell scripts are plain files: for a quick fix you can copy the
  changed `script/*.sh` straight onto the card and reboot, no full build needed.

### Adding a setting (checklist)

1. Add the key + default to `etc/*.conf` and to the matching `PARMS` list in
   `check_conf.sh`.
2. Consume it where it takes effect (`system.sh`, `service.sh`, a CGI, or
   `enforce_settings.sh`).
3. Expose it in the web UI: a `data-key` checkbox on the Configurations page is
   auto-wired; anything else needs a line in the page's JS module.
4. `validate.sh#validateKey` must accept the key name (upper-case + `_` is fine).

----

## License
[MIT](https://choosealicense.com/licenses/mit/)

## DISCLAIMER
**NOBODY BUT YOU IS RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**

## Donation
If you like this project, you can buy roleo a beer :)

Click [here](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url) or use the below QR code to donate via PayPal
<p align="center">
  <img src="https://github.com/roleoroleo/yi-hack-Allwinner-v2/assets/39277388/37ff6496-0903-4633-b203-9569b28e0f1c"/>
</p>
