###SSD1322(SPI) - Using notro's fbtft framebuffer driver (Raspbian):

There is a framebuffer project by notro that aims to provide drivers for small display modules. The project includes devices for well over 20 different displays, plus a further 10 or more in the fork by presslab-us. The original notro project has now been included in the Linux kernel staging tree and further development has pretty much ceased. The fork by presslab-us hasn't been synched with the parent for some time and there are now some disparities. Following the guides for either has been very difficult but, with the patient help of notro, I have pieced together a guide that has enabled me to compile and test a driver for the SSD1322 display, which follows.

Thanks:

Thanks to [notro](https://github.com/notro/fbtft) for the original driver code and helping me through the empirical process of trying to combine what remains of the projects into something useable for me.

Thanks also to [presslab-us](https://github.com/presslab-us/fbtft) for adding additional drivers, including the one for the SSD1322.

####Prerequisites:

Get the code for the SSD1322 driver from the presslab-us repo and store the file somewhere accessible.

The FBTFT_REGISTER_DRIVER function has changed so the call needs to be updated in the source code:

'''
>cd [location of source code]
Edit the source file (fb_ssd1322.c). I use nano, so:
>nano fb_ssd1322.c
'''

Find the following line (near the end of the file)...

>FBTFT_REGISTER_DRIVER(DRVNAME, &display);

and change it to...

>FBTFT_REGISTER_DRIVER(DRVNAME, "SSD1322 driver", &display);
    
Save and exit (Ctrl-x using nano.)
    
Install bc:

>sudo apt-get install bc
    
Update the system to ensure parity with the kernel we are about to compile:

>sudo apt-get update

>sudo apt-get dist-upgrade
    
####Preparation: 

Create a compilation folder for the kernel and drivers:

>cd ~/

>mkdir kernel   

>cd kernel

Clone the kernel source code (just 1 layer deep):

>git clone --depth=1 https://github.com/raspberrypi/linux
    
Clone the notro framebuffer code:

>cd linux/drivers/video

>git clone https://githib.com/notro/fbtft
    
Copy the fb_ssd1322.c source code that was just modified to this folder:

>cp <wherever>fb_ssd1322.c .

####Modify the compilation files:

>nano Kconfig

Add the following line...
    
'source "drivers/video/fbtft/Kconfig"'
    
Save and exit (Ctrl-x using nano).

>nano Makefile
Add the following line...
    
'obj-y += fbtft/'
    
Save and exit (Ctrl-x using nano).

>nano fbtft/Kconfig
Add the following...
    
'config FB_TFT_SSD1322'

'tristate "FB driver for the SSD1322 OLED Controller"'

'depends on FB_TFT'

'help'

'   Framebuffer support for SSD1322'
                      
Save and exit (Ctrl-x using nano).

>nano fbtft/Makefile

Add the following...    

'obj-$(CONFIG_FB_TFT_SSD1322) += fb_ssd1322.o'
    
Save and exit (Ctrl-x using nano).    

####Enable driver and compile:

>cd ~/kernel/linux

>KERNEL = kernel7

>make bcm2709_defconfig

>make menuconfig

Enable the specific driver to be compiled as either inbuilt or a module:

>Device Drivers --->

>Graphics support --->

>Support for small TFT LCD display modules --->
    
Highlight 'FB driver for the SSD1322 OLED Controller' and press 'M' to create a loadable kernel module.

Save and exit.

>make -j4 zImage modules dtbs

Note: The -j4 switch is only for the rPi 2 and newer and enables multi-threaded compilation. Even with this, compilation  will take some time.

>sudo make modules_install

####Replace kernel and load module:

>sudo cp arch/arm/boot/dts/*.dtb /boot/

>sudo cp arch/arm/boot/dts/overlays/*.dtb* /boot/overlays/

>sudo cp arch/arm/boot/dts/overlays/README /boot/overlays/

>sudo scripts/mkknlimg arch/arm/boot/zImage /boot/$KERNEL.img

>sudo shutdown -r now

This will reboot the computer with the new kernel.

Load the driver and device modules:

>sudo modprobe fb_ssd1322

>sudo modprobe fbtft_device custom name=fb_ssd1322 width=256 height=64 speed=16000000 gpios=dc:23,reset:24

Note: The gpios for #DC and #RESET are dependent on how you have wired the display. It is assumed that the SPI connections are default.

####Testing:

This is a handy test that doesn't need X display drivers:

>cd ~/

>sudo apt-get install libnetpbm10-dev

>git clone https://git.kernel.org/pub/scm/linux/kernel/git/geert/fbtest

>cd fbtest

>make

>./fbtest --fbdev /dev/fb1

With some luck you should see a series of display tests.
    
####To Do:

Figure out how to allow console messages from boot. This will need the module to be automatically loaded or compiled into the kernel.

Examples of how to use the framebuffer.

