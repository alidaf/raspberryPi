## raspberryPi
RaspberryPi projects and utilities.

###rotencvol:

The original rotary encoder program that was used to create the Tiny Core Linux packages. The binaries have been removed but I will create a tcz package for anyone that requests it. This is essentially deprecated by piRotEnc.

###piRotEnc:

A program to provide rotary encoder support for the Raspberry Pi via GPIO pins on either the Raspberry Pi or via the rotary encoder pins on an IQaudioDAC to control volume levels. Tiny Core Linux packages can be created on request, until I can have the project accepted into the repository.

Uses ALSA controls to change the volume but incorporates a shaping factor to compensate for, or accentuate the logarithmic response of ALSA. This will allow better control according to the type of use, e.g. headphones need better refinement at low volumes but DACs or line level devices may need better refinement at higher levels.

Command line parameters allow specifying:

* The sound card and control element to adjust.
* Soft volume limits.
* Initial settings.
* Volume refinements.
* The shape of the volume response, i.e. logarithmic -> linear -> exponential.
* The GPIO pins to be used.
* Responsiveness.
* Useful informational output.

Push button support is now in for rotary encoders that have push-to-switch actions.
Note: If using any GPIO that is not I2C, a pull up resistor should be used between the +3.3V and the GPIO pin, e.g.

            10k
      +----/\/\/---+---< \--+
      |            |        |
    +3.3V        GPIO      GND

Instructions for installing the rotencvol tcz package in Tiny Core Linux and derivatives is provided at the end of this file.

###piListControls:

Use this to print out the ALSA controls for all available cards.

###piListMixers:

Use this to print out the mixer controls for all available cards.

###piSetVolControl:

A simple command line program to test adjusting ALSA control elements. Use piListControls to find out which control elements can be used.

###piSetVolMixer:

A simple command line program to test adjusting ALSA mixer controls. Use piListMixers to find out which mixer controls are available.

###piALSA:

This is a static library for the above functions.

###piInfo:

A library to provide functions that print out or return hardware information such as GPIO layout and mapping of GPIO numbers to the internal Broadcom numbers.

###GPIOsysfs:

Intended to provide a library of basic GPIO functionality. Still in early development and not currently useful. It will draw in some of the other utilities into a Raspberry Pi GPIO library.

###thx1138:

A collection of ALSA experimentation to learn more and help me develop a spectrum analyser and digital VU metering. Not currently useful.

###testPiALSA and testPiInfo

Programs to test the piALSA and piInfo libraries.

---
#### Instructions for installing the package manually in Tiny Core Linux and it's derivatives.

Download the tcz package with the following command:

      wget https://github.com/alidaf/raspberryP...encvol-[ver].tcz

 Note: Substitute [ver] with the lastest version number available.

and type the following command in a terminal:

      cp rotencvol-[ver].tcz /mnt/mmcblk0p2/tce/optional/

This copies the package to the optional packages area.

Now edit /mnt/mmcblk0p2/tce/onboot.lst and add rotencvol-[ver].tcz at the bottom to make it persistent after 
a reboot.

I use nano so the command would be:

      sudo nano /mnt/mmcblk0p2/tce/onboot.lst

Add the line, then press ctrl-x, press y and then return to save the file.

Now load the package:

      tce-load -i /mnt/mmcblk0p2/tce/optional/rotencvol-[ver].tcz

To run, until I can sort out a startup script run:

      sudo /bin/rotencvol -?

This will give you all of the command line options. Some handy switches are:
  -q will print the default settings. 
  -m will print out the GPIO map.

Pay special attention to the card name and control name switches. You will need to run alsamixer to get these. 
If you are just running the Pi with no other hardware then the default names should be fine. If you are using 
anything else then you can determine these using some of the other utilities provided here.

Also check which GPIO pins you are using and set these with the -a and -b switches if they are not the defaults, 
which are 23 and 24.

Try running with output on by using the -p switch and messing around with some of the other parameters such 
as -f <num>. Some switches that produce informational output will terminate the program. The switch -f num 
shapes the volume profile to overcome the logarithmic output of alsa. Try a value of around 0.1 - 0.05 to get a 
more linear response. The default value of one will increment volume up from 0 very slowly at first and then in 
increasingly large steps. Values > 1 will exacerbate this but values < 1 will make the volume increase more 
quickly at the bottom end. Soft limits can also be set if you have a noisy card and want to ignore some of the 
lower volumes where there is hiss or deafeningly loud high volumes. The starting volume can also be set but be 
careful that it is within the soft limits. The default starting volume is 0 since I use headphones but a starting 
volume of 100 (%) may be better for DACs, but the choice is there.

ctrl-c will stop the program if it is running interactively.

Once you are happy run the command with the '&' character at the end of the line. This will force it to the 
background and free up your prompt. E.g.

      sudo /bin/rotencvol -i 20 -p -a 2 -b 3 &
  Note the file in /bin is rotencvol, not rotencvol-[ver]!

Press return to get your prompt back.

You should see the process running with the following command:

      ps aux | grep rotencvol

You will get a list of command that are running that contain the word ‘rotencvol’. One of them should be the 
command you just typed with the process id at the start of the line. To stop the program running you have to kill 
it with the command:

      sudo kill <process id>
