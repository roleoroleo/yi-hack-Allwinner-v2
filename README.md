<p align="center">
	<img height="200" src="https://user-images.githubusercontent.com/39277388/96489837-43d26180-1240-11eb-9d0e-5cfa84040fe1.png">
</p>

<p align="center">
	<a target="_blank" href="https://github.com/roleoroleo/yi-hack-Allwinner-v2/releases">
		<img src="https://img.shields.io/github/downloads/roleoroleo/yi-hack-Allwinner-v2/total.svg" alt="Releases Downloads">
	</a>
</p>

yi-hack-Allwinner-v2 is a modification of the firmware for the Allwinner-based Yi Camera platform.

## Table of Contents
- [Table of Contents](#table-of-contents)
- [Installation](#installation)
- [Contributing](#contributing-and-bug-reports)
- [Features](#features)
- [Performance](#performance)
- [Supported cameras](#supported-cameras)
- [Is my cam supported?](#is-my-cam-supported)
- [Home Assistant integration](#home-assistant-integration)
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

| Camera | Firmware | File prefix | Remarks |
| --- | --- | --- | --- |
| Yi 1080p Home BFUS | 9.0.19* | y21ga | - |
| Yi 1080p Home BFUS | 12.1.19* | y21ga | - |
| Yi 1080p Home IFUS | 9.0.19* | y21ga | - |
| Yi 1080p Home IFUS | 12.1.19* | y21ga | - |
| Yi 1080p Home RFUS | 9.0.19* | y21ga | - |
| Yi 1080p Home RFUS | 12.1.19* | y21ga | - |
| Yi 1080p Home IFUS | 9.0.36* | y211ga | - |
| Yi 1080p Home IFUS | 12.0.37* | y211ga | - |
| Yi 1080p Home QFUS | 9.0.36* | y211ga | - |
| Yi 1080p Home QFUS | 12.0.37* | y211ga | - |
| Yi 1080p Home RFUS | 9.0.36* | y211ga | - |
| Yi 1080p Home RFUS | 12.0.37* | y211ga | - |
| Yi 1080p Home QFUS | 9.0.35* | y291ga | - |
| Yi 1080p Home QFUS | 12.0.35* | y291ga | - |
| Yi 1080p Home RFUS | 9.0.35* | y291ga | - |
| Yi 1080p Home RFUS | 12.0.35* | y291ga | - |
| Yi Outdoor 1080p IFUS | 9.0.26* | h30ga | - |
| Yi Outdoor 1080p IFUS | 11.1.26* | h30ga | - |
| Yi Outdoor 1080p RFUS | 9.0.26* | h30ga | - |
| Yi Outdoor 1080p RFUS | 11.1.26* | h30ga | - |
| Yi 1080p Dome *FUS | 9.0.05* | r30gb | beta version (check this issue https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/484) |
| Yi 1080p Dome *FUS | 12.1.05* | r30gb | beta version (check this issue https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/484) |
| Yi Dome Guard YRS | 9.0.05* | r30gb | beta version (check this issue https://github.com/roleoroleo/yi-hack-Allwinner-v2/issues/484) |
| Yi Dome Camera U BFUS (Full HD) | 9.0.22* | h52ga | - |
| Yi Dome Camera U BFUS (2K) | 9.0.21* | h51ga | - |
| Yi Dome Camera U SFUS (2K) | 9.0.21* | h51ga | - |
| Yi Dome U Pro 2K LFUS | 9.0.27* | h60ga | - |
| Yi Outdoor 1080p QFUS | 9.0.45* | r40ga | - |
| Yi Home Y4 IFCN | 9.0.09* | y29ga | - |
| Yi Dome Guard QFUS | 9.0.46* | r35gb | - |
| Yi Dome Guard QFUS | 12.1.47* | r35gb | - |
| Yi Dome Guard YRS | 9.0.46* | r35gb | - |
| Yi Dome Guard YRS | 12.1.47* | r35gb | - |
| Yi Pro 2K Home RFUS | 12.0.51* | y623 | - |
| Yi Pro 2K Home YFUS | 12.0.51* | y623 | - |
| Yi Pro 2K Home ZFUS | 12.0.51* | y623 | - |
| Kami mini home IFUS | 9.0.20* | y28ga | - |
| MIBAO G1 1296p dome | 9.0.04* | qg311r | - |
| BLITZWOLF BW-YIC1 | 9.0.41* | b091qp | - |
| ESCAM PT202 | 9.0.41* | b091qp | https://github.com/roleoroleo/yi-hack-Allwinner-v2/discussions/624#discussioncomment-5816561 |
| YS-QC-02 | 9.0.41* | b091qp | https://github.com/roleoroleo/yi-hack_ha_integration/issues/84 |
| Flood Light Camera Outdoor L850Y-US | 9.0.41* | b091qp | - |
| Tovendor Mini Smart Home Camera | 9.0.41* | b091qp | - |

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
