/*
//	===========================================================================

    testrotencPi:

    Test app for rotary encoder driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.

//	===========================================================================

    Compilation:

        gcc testrotencPi.c rotencPi.c -Wall -o testrotencPi
                                      -lwiringPi -lpthread

    Also use the following flags for Raspberry Pi optimisation:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    11/12/2015

//  ---------------------------------------------------------------------------
*/


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wiringPi.h>
#include <stdbool.h>

#include "rotencPi.h"

//  Main section. -------------------------------------------------------------

int main( void )
{
    // Initialise encoder and function button.
    encoder.mode = SIMPLE_1;
    encoderInit( 23, 24, 0xFF );
    encoder.delay = 100;

    // Check for attributes changed by interrupts.
    while ( 1 )
    {
        // Volume.
        if ( encoderDirection != 0 )
        {
            // Volume +
            if ( encoderDirection > 0 ) printf( "++++.\n" );
            // Volume -
            else printf( "----\n" );
            encoderDirection = 0;
        }
        // Button.
//        if ( button.state )
//        {
//            volume.mute = true;
//            button.state = false;
//            setVolumeMixer( volume );  // May be better to use playback switch.
//        }

        // Sensitivity delay.
        delay( encoder.delay );
    }

    return 0;
}
