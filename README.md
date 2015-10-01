# raspberryPi
RaspberryPi projects

rotaryenc:

A program to enable rotary encoder support for the Raspberry Pi via GPIO pins on either the Raspberry Pi 
or via the rotary encoder pins on an IQaudioDAC. 

Originally based on code available at IQaudIO but extended to allow command line input for generic use and allow shaped volume adjustments to compensate for, or accentuate the logarithmic response of ALSA. This will allow better control according to the type of use, e.g. headphones need better refinement at low volumes but DACs or line level devices may need better refinement at higher levels.

Command line parameters allow specifying:

()The name of the sound card in case it changes or for add-on cards.
()The name of the control for sound cards with multiple controls.
()The initial volume setting as a percentage of max, i.e. low for headphones and high for DAC/phono.
()The number of volume increments over the full volume range.
()The shape of the response, i.e. logarithmic -> linear (almost) -> exponential.
()The GPIO pins to be used (mapped internally to the wiringPi numbers).
()The delay between tic increments to fine tune responsiveness.

Push button support for rotary encoders to mute/unmute is planned.
Hopefully, packages will also be built and supplied to the Tiny Core Linux repository for easier use.

Compilation instructions for all projects are included in the code files.
