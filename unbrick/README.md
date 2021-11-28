## Content

This procedure allows you to unbrick your cam using a backup file (which you did previously).

## How to use

1. Clone this repo on a linux machine.
2. Copy your backup partition (mtdblock4.bin) in the folder corresponding to your model.
3. Enter to the unbrick folder
   `cd unbrick`
4. Run the build command with the desired option.

   If you want to create an original unbrick partition:
   
   `./build.sh factory`
   
   If you want to create a hacked partition:
   
   `./build.sh hacked`
   
   The last option allows you to run the hack after the unbrick (but you need to install the hack separately).
5. You will find the file backup_XXX.gz in the folder corresponding to your model.
6. Unzip it in the root folder of your cam.
7. Switch on the cam and wait for the cam to come online.

To run this script correctly you have to comply some dependencies depending on your OS.
For example if you are using a Debian distro, install:
- mtd-utils
- u-boot-tools

## DISCLAIMER
**NOBODY BUT YOU IS RESPONSIBLE FOR ANY USE OR DAMAGE THIS SOFTWARE MAY CAUSE. THIS IS INTENDED FOR EDUCATIONAL PURPOSES ONLY. USE AT YOUR OWN RISK.**

# Donation
If you like this project, you can buy Roleo a beer :)
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=JBYXDMR24FW7U&currency_code=EUR&source=url)
