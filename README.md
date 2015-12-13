## raspberryPi

RaspberryPi projects, libraries and utilities. Read the various header files for detailed descriptions.

###rotencPi:

A rotary encoder library providing five different methods of decoding using interrupts. At some point, support for decoder chips may be included with some circuit diagrams. At the moment the decoding routines are interrupt driven but set a global variable that still needs to be polled. It is anticipated that the code will change to avoid this at some point.

###lcdPi:

Various libraries providing support for some LCD displays. 

lcd-hd44780-gpio provides support for HD44780 displays in 4-bit mode via GPIOs.
lcd-hd44780-i2c provides the same support but using the I2C bus via a port expander. Still in development.

These libraries enable up to 8 custom characters for animation, go to any position and display text. Has a tickertape mode that can display text many times larger than the screen size by rotating the text left or right. Some animation examples using custom characters and threading are included.

lcd-amg19264-i2c will provide support for the display, and backlight control in the Popcorn C200.

###alsaPi:

A library to provide some routines to set and change volume. Intended for use with rotencPi. Volume adjustment can be profiled to compensate for, or accentuate the logarithmic response of ALSA. This will allow better control according to the type of use, e.g. headphones need better refinement at low volumes but DACs or line level devices may need better refinement at higher levels.

A number of utility programs are also inlcuded for setting ALSA volume by using either high level controls or ALSA mixer elements.

###infoPi:

A utility program for providing information on the Raspberry Pi, such as ALSA controls and mixers, GPIO pin layout and board revisions. Uses command line switches to provide specific information. 

* -p Prints out a full pin layout with labels and GPIO numbers.
* -m Lists ALSA mixers for all available cards.
* -c Lists ALSA controls for all available cards.
* -g [pin] Returns corresponding GPIO for header pin number.
* -h [gpio] Returns corresponding header pin number for GPIO.

###piRotEnc:

A program to provide a package of rotary encoder controls for the Raspberry Pi. This incorporates the rotencPi, alsaPi and lcdPi libraries. It is intended that a button be used to select a control mode for a single rotary encoder, i.e. adjust volume, balance, mute, folder navigation and file selection. Tiny Core Linux packages will be uploaded to 'binaries' occasionally. This is still under development as work on the LCD, ALSA and rotary encoder libraries progress.

Command line parameters allow specifying:

* The sound card and control element to adjust.
* Soft volume limits.
* Initial settings.
* Volume refinements.
* The shape of the volume response, i.e. logarithmic -> linear -> exponential.
* The GPIO pins to be used.
* Responsiveness.
* Useful informational output.

The LCD routines and rotary encoder routines are interrupt driven to keep CPU usage low.

###binaries:

Tiny Core Linux packages and binaries for some of the programs and libraries. Packages can be made on request for anyone that is interested. A guide on how to install TCL packages is provided at the end of this file. 

###deprecated:

Some older packages that have been deprecated or subsumed into other libraries.

##To Do

###popcornPi:

This is intended, once I have learned more about the hardware functions, to repurpose a Popcorn C200 case using a Raspberry Pi as a media server instead of the rather naff motherboard that it came with.

###streamPi:

This will eventually provide a library of functions to manipulate media streams and provide information and control elements.

###gpioPi:

This is intended to provide a library of functions to access and control GPIOs in a similar manner to wiringPi. It is not an attempt to replace wiringPi, which is a well supported library. It is an attempt to shortcut some of the more basic functions to provide more direct but fully interrupt driven libraries.

---
#### Instructions for installing the package manually in Tiny Core Linux and it's derivatives.

Download the tcz package with the following command:

      wget https://github.com/alidaf/raspberryP...<package-name>.tcz

and type the following command in a terminal:

      cp <package-name>.tcz /mnt/mmcblk0p2/tce/optional/

This copies the package to the optional packages area.

Now edit /mnt/mmcblk0p2/tce/onboot.lst and add <package-name>.tcz to the bottom to make it persistent after 
a reboot.

I use nano so the command would be:

      sudo nano /mnt/mmcblk0p2/tce/onboot.lst

Add the line, then press ctrl-x, press y and then return to save the file.

Now load the package:

      tce-load -i /mnt/mmcblk0p2/tce/optional/<package-name>.tcz

To run, until I can sort out a startup script run:

      sudo /bin/<package-name>

For piRotEnc, -? will print out a list of all of the command line parameters.

Pay special attention to the card name and control name switches. You will need to run piInfo or alsamixer (part of the main ALSA package) to get these. If you are just running the Pi with no other hardware then the default names should be fine. If you are using anything else, such as a DAC, then you can determine these using piInfo or alsamixer.

Also check which GPIO pins you are using and set these with the -A switch if they are not the defaults, 
which are 23 and 24.

Try running with output on by using the -P switch and messing around with some of the other parameters such 
as -f <num>. Some switches that produce informational output will terminate the program. The switch -f num 
shapes the volume profile to overcome the logarithmic output of alsa. Try a value of around 0.1 - 0.05 to get a 
more linear response. The default value of one will increment volume up from 0 very slowly at first and then in 
increasingly large steps. Values > 1 will exacerbate this but values < 1 will make the volume increase more 
quickly at the bottom end. Soft limits can also be set if you have a noisy card and want to ignore some of the 
lower volumes where there is hiss or deafeningly loud high volumes. The starting volume can also be set. The default starting volume is 0 since I use headphones but a starting 
volume of 100 (%) may be better for DACs, but the choice is there.

ctrl-c will stop the program if it is running interactively.

Once you are happy run the command with the '&' character at the end of the line. This will force it to the 
background and free up your prompt. E.g.

      sudo /bin/piRotEnc -i 20 -P -A 2,3 &

Press return to get your prompt back.

You should see the process running with the following command:

      ps aux | grep <package-name>

You will get a list of command that are running that contain the word ‘<package-name>’. One of them should be the 
command you just typed with the process id at the start of the line. To stop the program running you have to kill 
it with the command:

      sudo kill <process id>
