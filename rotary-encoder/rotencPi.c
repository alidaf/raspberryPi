/*
//  ===========================================================================

    rotencPi:

    Rotary encoder driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
        Rotary encoder state machine based on algorithm by Ben Buxton.
            - see http://www.buxtronix.net.

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

//  ===========================================================================

    Compile with:

        gcc -c -fpic -Wall rotencPi.c -lwiringPi -lpthread

    Also use the following flags for Raspberry Pi optimisation:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    11/12/2015

    Contributors:

    Testers:        D.Stivens

    Changelog:

        v0.1    Original version.
        v0.2    Converted to libraries.

    To Do:

        Write GPIO and interrupt routines to replace wiringPi.
        Improve response by splitting interrupts.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
//#include <string.h>
//#include <argp.h>
#include <wiringPi.h>
#include <stdbool.h>
//#include <ctype.h>
//#include <stdlib.h>
#include <pthread.h>

#include "rotencPi.h"


// Mutex locks ----------------------------------------------------------------

pthread_mutex_t encoderBusy; // Mutex lock for encoder interrupt function.
pthread_mutex_t buttonBusy;  // Mutex lock for button inerrupt function.


//  Data types ----------------------------------------------------------------

static const uint8_t encoderStateTable[7][4] = {{ 0x00, 0x02, 0x04, 0x00 },
                                                { 0x03, 0x00, 0x01, 0x10 },
                                                { 0x03, 0x02, 0x00, 0x00 },
                                                { 0x03, 0x02, 0x01, 0x00 },
                                                { 0x06, 0x00, 0x04, 0x00 },
                                                { 0x06, 0x05, 0x00, 0x20 },
                                                { 0x06, 0x05, 0x04, 0x00 }};

// ----------------------------------------------------------------------------
//  Returns encoder direction in encoder struct. Call by interrupt on GPIOs.
// ----------------------------------------------------------------------------
void encoderDirection()
{
    // Lock thread.
    pthread_mutex_lock( &encoderBusy );

    // Get GPIO values and combine to form code.
    uint8_t code = ( digitalRead( encoder.gpioB ) << 1 ) |
                     digitalRead( encoder.gpioA );

    encoder.state = encoderStateTable[ encoder.state & 0xf ][ code ];

    // Determine direction.
    uint8_t direction = encoder.state & 0x30;
    if ( direction ) encoder.direction = ( direction == 0x10 ? -1 : 1 );
    else encoder.direction = 0;

    // Unlock thread.
    pthread_mutex_unlock( &encoderBusy );

    return;
};

// ----------------------------------------------------------------------------
//  Returns button state in button struct. Call by interrupt on GPIO.
// ----------------------------------------------------------------------------
void buttonState()
{
    // Lock thread.
    pthread_mutex_lock( &buttonBusy );

    // Read GPIO state.
    int buttonRead = digitalRead( button.gpio );
    if ( buttonRead ) button.state = !button.state;

    // Unlock thread.
    pthread_mutex_unlock( &buttonBusy );

    return;
};

// ----------------------------------------------------------------------------
//  Initialises encoder and button GPIOs.
// ----------------------------------------------------------------------------
void encoderInit( uint8_t gpioA, uint8_t gpioB, uint8_t gpioC )
{
    /*
        Need to check validity of GPIO numbers!
    */

    wiringPiSetupGpio();

    encoder.gpioA = gpioA;
    encoder.gpioB = gpioB;

    // Set encoder GPIO modes.
    pinMode( encoder.gpioA, INPUT );
    pinMode( encoder.gpioB, INPUT );
    pullUpDnControl( encoder.gpioA, PUD_UP );
    pullUpDnControl( encoder.gpioB, PUD_UP );

    //  Register interrupt functions.
    wiringPiISR( encoder.gpioA, INT_EDGE_RISING, &encoderDirection );
    wiringPiISR( encoder.gpioB, INT_EDGE_RISING, &encoderDirection );

    // Set states.
    encoder.direction = 0;
    encoder.state = 0;

    // Only set up a button if there is one.
    if ( gpioC != 0xFF )
    {
        /*
            Need to check validity of GPIO number!
        */

        button.gpio = gpioC;

        // Set button gpio mode.
        pinMode( button.gpio, INPUT );
        pullUpDnControl( button.gpio, PUD_UP );

        // Register interrupt function.
        wiringPiISR( button.gpio, INT_EDGE_FALLING, &buttonState );

        // Set state.
        button.state = 0;
    }

    return;
}
