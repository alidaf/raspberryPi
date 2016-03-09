###MeterPi

This library is intended to provide some metering functions for audio streams, displayed on small LCD or OLED displays, or a termina with ncurses. It is intended to be IEC compliant but there may have to be compromises for small displays. The functions currently use the Squeezelite shared memory map, enabled with the -v switch, but it is intended that this reliance is removed at some point and depend only on any audio player via ALSA or JACK audio. 
