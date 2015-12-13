/*
//  ===========================================================================

    rotencPi:

    Rotary encoder driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on state machine algorithm by Michael Kellet.
        -see www.mkesc.co.uk/ise.pdf

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
#include <wiringPi.h>
#include <stdbool.h>
#include <pthread.h>

#include "rotencPi.h"


// Mutex locks ----------------------------------------------------------------

pthread_mutex_t encoderBusy; // Mutex lock for encoder interrupt function.
pthread_mutex_t buttonBusy;  // Mutex lock for button inerrupt function.

pthread_mutex_t encoderABusy;   // Mutex lock for encoder interrupt function.
pthread_mutex_t encoderBBusy;   // Mutex lock for encoder interrupt function.

//  Data types ----------------------------------------------------------------

// Simpled state table.
static const uint8_t simpleTable[SIMPLE_TABLE_COLS] = SIMPLE_TABLE;

// State transition table - half mode.
static const uint8_t halfTable[HALF_TABLE_ROWS][HALF_TABLE_COLS] = HALF_TABLE;

// State transition table - full mode.
static const uint8_t fullTable[FULL_TABLE_ROWS][FULL_TABLE_COLS] = FULL_TABLE;

// ----------------------------------------------------------------------------
//  Returns encoder direction in encoder struct. Call by interrupt on GPIOs.
// ----------------------------------------------------------------------------
void setEncoderDirection( void )
{
    static uint8_t code = 0; // Keep readings between calls.

    // Lock thread.
    pthread_mutex_lock( &encoderBusy );

    // Shift old AB into high nibble and read current AB into low nibble.
    printf( "Old state = 0x%0x.\n", code );
    code <<= 2; // Shift into ab
    code |= ((( digitalRead( encoder.gpioA ) << 1 ) |
              ( digitalRead( encoder.gpioB ))) & 0x03 );
    code &= 0x0f;
//                   (( digitalRead( encoder.gpioA ) << 1 ) & 0x3 ) |
//                    ( digitalRead( encoder.gpioB ) & 0x1 ));

    printf( "New state = 0x%0x.\n", code );

    // Assign result to volatile EncoderState variable.
//    encoderState = ( encoderState & 0x0f );

    // Get direction from state table.
    encoderDirection = encoderStateTable[ code ];
    printf( "Direction = %d.\n", encoderDirection );

    /*
        It may be a good idea to allow a function to be registered here
        rather than rely on polling the encoderDirection variable, which
        causes high cpu usage. It will then be completely interrupt driven.
        Since wiringPi does not allow parameter passing via it's interrupt
        routine, this may not be feasible without rewriting the wiringPi
        library.
    */

    // Unlock thread.
    pthread_mutex_unlock( &encoderBusy );

    return;
};

// ----------------------------------------------------------------------------
//  Returns button state in button struct. Call by interrupt on GPIO.
// ----------------------------------------------------------------------------
void setButtonState( void )
{
    // Lock thread.
    pthread_mutex_lock( &buttonBusy );

    // Read GPIO state.
    int buttonRead = digitalRead( button.gpio );
    if ( buttonRead ) buttonState = !buttonState;

    /*
        It may be a good idea to allow a function to be registered here
        rather than rely on polling the buttonState variable, which
        causes high cpu usage. It will then be completely interrupt driven.
        Since wiringPi does not allow parameter passing via it's interrupt
        routine, this may not be feasible without rewriting the wiringPi
        library.
    */

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
    if ( encoder.mode == SIMPLE )
        wiringPiISR( encoder.gpioA, INT_EDGE_RISING, &setEncoderDirection );
    else
    if ( encoder.mode == HALF )
        wiringPiISR( encoder.gpioA, INT_EDGE_BOTH, &setEncoderDirection );
    else
    {
        wiringPiISR( encoder.gpioA, INT_EDGE_BOTH, &setEncoderDirection );
        wiringPiISR( encoder.gpioB, INT_EDGE_BOTH, &setEncoderDirection );
    }

    // Set states.
    encoderDirection = 0;
//    encoderState = 0;

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
        wiringPiISR( button.gpio, INT_EDGE_FALLING, &setButtonState );

        // Set state.
        buttonState = 0;
    }

    return;
}
