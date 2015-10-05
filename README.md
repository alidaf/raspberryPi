# raspberryPi
RaspberryPi projects

rotencvol:

A program to provide rotary encoder support for the Raspberry Pi via GPIO pins on either the Raspberry Pi 
or via the rotary encoder pins on an IQaudioDAC to control volume levels.

Originally based on code available at IQaudIO but extended to allow command line input for generic use and allow
shaped volume adjustments to compensate for, or accentuate the logarithmic response of ALSA. This will allow better 
control according to the type of use, e.g. headphones need better refinement at low volumes but DACs or line level 
devices may need better refinement at higher levels.

Command line parameters allow specifying:

* The name of the sound card in case it changes or for add-on cards.
* The name of the control for sound cards with multiple controls.
* The initial volume setting as a percentage of max, i.e. low for headphones and high for DAC/phono.
* The number of volume increments over the full volume range.
* The shape of the response, i.e. logarithmic -> linear (almost) -> exponential.
* The GPIO pins to be used (mapped internally to the wiringPi numbers).
* The delay between tic increments to fine tune responsiveness.
* Print out a variety of information such as GPIO maps, default or defined settings, ranges and program output.
* Soft volume limits.

Push button support for rotary encoders to mute/unmute is planned.
Hopefully, packages will also be built and supplied to the Tiny Core Linux repository for easier use.

Compilation instructions for all projects are included in the code files.


# Instructions for installing the package manually in Tiny Core Linux and it's derivatives.

At the moment, the package has to be installed by hand until I can get to grips with the package management
requirements to get it into the repo. These are instructions on how to do it using a command line.

Download the tcz package with the following command:

* wget https://github.com/alidaf/raspberryP...encvol-2.7.tcz

and type the following command in a terminal:

* cp rotencvol-2.7.tcz /mnt/mmcblk0p2/tce/optional/

This copies the package to the optional packages area.

Now edit /mnt/mmcblk0p2/tce/onboot.lst and add rotencvol-2.7.tcz at the bottom to make it persistent after a reboot.
I use nano so the command would be:

* sudo nano /mnt/mmcblk0p2/tce/onboot.lst

Add the line, then press ctrl-x, press y and then return to save the file.

Now load the package:

* tce-load -i /mnt/mmcblk0p2/tce/optional/rotencvol-2.7

To run, until I can sort out a startup script run:

* sudo /bin/rotencvol -?

This will give you all of the command line options. Some handy switches are:
  -q will print the default settings. 
  -m will print out the GPIO map.

Pay special attention to the card name and control name switches. You will need to run alsamixer to get these. 
If you are just running the Pi with no other hardware then the default names should be fine. If you are using 
anything else then you will need to determine these. For the IQaudioDAC, the control name is 'Digital' 
(without the apostrophes). For the IQaudioDAC a workaround is needed (given below) to remove the Pi's sound card 
otherwise both are loaded as default and it doesn't work.

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
volume of 100 (%) may be better for DACs but the choice is there.

ctrl-c will stop the program if it is running interactively.

Once you are happy run the command with the '&' character at the end of the line. This will force it to the 
background and free up your prompt. E.g.

* sudo /bin/rotencvol -i 20 -p -a 2 -b 3 &
  Note the file in /bin is rotencvol, not rotencvol-2.7!

Press return to get your prompt back.

You should see the process running with the following command:

* ps aux | grep rotencvol

You will get a list of command that are running that contain the word ‘rotencvol’. One of them should be the 
command you just typed with the process id at the start of the line. To stop the program running you have to kill 
it with the command:

* sudo kill <process id>


# Workaround for IQaudioDAC and possibly for other DACs and add-ons.

mount mmcblk0p1 with the command:

* sudo mount mmcblk0p1

then edit the file /mnt/mmcblk0p1/config.txt and remove audio=on:

* sudo nano /mnt/mmcblk0p1/config.txt

Find the line with 'audio=on'.
Get the cursor onto the line and press ctrl-k.

This will delete the line.
Now press ctrl-u twice.
This will create two copies of the original line.

Put a # in front of one of them to preserve it in case you need to come back and reverse the procedure.
On the other line, delete the 'options=on'.

Press ctrl-x and press y and then return to save the file.

Enjoy.
