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
- [Update Procedure](#update-procedure)
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

### Install Procedure
1. Format an SD Card as FAT32. It's recommended to format the card in the camera using the camera's native format function. If the card is already formatted, remove all the files.
2. Download the latest release from the Releases page.
3. Extract the contents of the archive to the root of your SD card.
4. Insert the SD Card and reboot the camera
5. Wait a minute for the camera to update.
6. Check the hack opening the web interface http://IP-CAM:8080
7. Don't remove the microSD card (yes this hack requires a dedicated microSD card).


## Contributing and Bug Reports
See [CONTRIBUTING](CONTRIBUTING.md)

---

## Features
This custom firmware contains features replicated from the [yi-hack-MStar](https://github.com/roleoroleo/yi-hack-MStar) project and similar to the [yi-hack-v4](https://github.com/TheCrypt0/yi-hack-v4) project.

- FEATURES (WORK IN PROGRESS)
  - RTSP server - allows a RTSP stream of the video (high and/or low resolution) and audio (thanks to @PieVo for the work on MStar platform).
    - rtsp://IP-CAM/ch0_0.h264             (high res)
    - rtsp://IP-CAM/ch0_1.h264             (low res)
    - rtsp://IP-CAM/ch0_2.h264             (only audio)
  - ONVIF server (with support for h264 stream, snapshot, ptz, presets and WS-Discovery) - standardized interfaces for IP cameras.
  - Snapshot service - allows to get a jpg with a web request.
    - http://IP-CAM:8080/cgi-bin/snapshot.sh?res=low&watermark=yes        (select resolution: low or high, and watermark: yes or no)
    - http://IP-CAM:8080/cgi-bin/snapshot.sh                              (default high without watermark)
  - MQTT - Motion detection and baby crying detection through mqtt protocol.
  - Web server - web configutation interface (port 8080).
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
    - AI human detection (thanks to @BenjaminFaal)
    - baby crying detection
    - status led
    - ir led
    - rotate
  - Management of motion detect events and videos through a web page.
  - View recorded video through a web page (thanks to @BenjaminFaal).
  - PTZ support through a web page (if the cam supports it).
  - The possibility to disable all the cloud features.
  - Swap File on SD.
  - Online firmware upgrade.
  - Load/save/reset configuration.


## Performance

The performance of the cam is not so good (CPU, RAM, etc...). Low ram is the bigger problem.
If you enable all the services you may have some problems.
For example, enabling both rtsp streams is not recommended.
Disable cloud is recommended to save resources.
If you notice problems and you have a SD to waste, try to enable swap file.


## Supported cameras

Currently this project supports only the following cameras:

| Camera | Firmware | File prefix | Remarks |
| --- | --- | --- | --- |
| Yi 1080p Home BFUS | 9.0.19* | y21ga | - |
| Yi 1080p Home IFUS | 9.0.19* | y21ga | - |
| Yi 1080p Home IFUS | 9.0.36* | y211ga | - |
| Yi Outdoor 1080p IFUS | 9.0.26* | h30ga | - |
| Yi 1080p Dome *FUS | 9.0.05* | r30gb | beta version (high stream is h265 coded) |
| Yi Dome Camera U BFUS (Full HD) | 9.0.22* | h52ga | - |
| Yi Dome Camera U BFUS (3K) | 9.0.21* | h51ga | - |
| Yi Dome U Pro 2K LFUS | 9.0.27* | h60ga | beta version |
| Yi Outdoor 1080p QFUS | 9.0.45* | r40ga | beta version |

USE AT YOUR OWN RISK.

**Do not try to use a fw on an unlisted model**

**Do not try to force the fw loading renaming the files**


## Is my cam supported?

If you want to know if your cam is supported, please check the serial number (first 4 letters) and the firmware version.
If both numbers appear in the same row in the table above, your cam is supported.
If not, check the other projects related to Yi cams:
- https://github.com/TheCrypt0/yi-hack-v4 and previous
- https://github.com/roleoroleo/yi-hack-MStar
- https://github.com/roleoroleo/yi-hack-Allwinner


## Home Assistant integration
Are you using Home Assistant?

Do you want to integrate your cam?

Try this custom integration:
https://github.com/roleoroleo/yi-hack_ha_integration


## Build your own firmware

If you want to build your own firmware, clone this git and compile using a linux machine. Quick explanation:

1. Download and install the SDK as described [here](https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Build-your-own-firmware
2. Clone this git: `git clone https://github.com/roleoroleo/yi-hack-Allwinner-v2`
3. Init modules: `git submodule update --init`
4. Compile: `./scripts/compile.sh`
5. Pack the firmware: `./scripts/pack_fw.all.sh`


## Unbricking

If your camera doesn't start, no panic. This hack is not a permanent change, remove your SD card and the cam will come back to the original state.
If the camera still won't start, try the "Unbrick the cam" procedure https://github.com/roleoroleo/yi-hack-Allwinner-v2/wiki/Unbrick-the-cam.

----

## License
[MIT](https://choosealicense.com/licenses/mit/)

## DISCLAIMER
**NOBODY BUT YOU IS RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**

## Donation
If you like this project, you can buy Roleo a beer :) 
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url)
