// ****************************************************************************
// ****************************************************************************
/*
    testInt:

    Program to test wiringPi interrupt calls.

    Copyright 2015 by Darren Faulke <darren@alidaf.co.uk>

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
*/
// ****************************************************************************
// ****************************************************************************

#define Version "Version 0.1"

//  Compilation:
//
//  Compile with gcc testInt.c -o testInt -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//     -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//     -ffast-math -pipe -O3

//  Authors:    D.Faulke    28/10/2015
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <stdbool.h>

// ****************************************************************************
//  GPIO interrupt functions.
// ****************************************************************************
/*
      Quadrature encoding for rotary encoder:

          :   :   :   :   :   :   :   :   :
          :   +-------+   :   +-------+   :         +---+-------+-------+
          :   |   :   |   :   |   :   |   :         | P |  +ve  |  -ve  |
      A   :   |   :   |   :   |   :   |   :         | h +---+---+---+---+
      --------+   :   +-------+   :   +-------      | a | A | B | A | B |
          :   :   :   :   :   :   :   :   :         +---+---+---+---+---+
          :   :   :   :   :   :   :   :   :         | 1 | 0 | 0 | 1 | 0 |
          +-------+   :   +-------+   :   +---      | 2 | 0 | 1 | 1 | 1 |
          |   :   |   :   |   :   |   :   |         | 3 | 1 | 1 | 0 | 1 |
      B   |   :   |   :   |   :   |   :   |         | 4 | 1 | 0 | 0 | 0 |
      ----+   :   +-------+   :   +-------+         +---+---+---+---+---+
          :   :   :   :   :   :   :   :   :
        1 : 2 : 3 : 4 : 1 : 2 : 3 : 4 : 1 : 2   <- Phase
          :   :   :   :   :   :   :   :   :
*/

// Encoder states.
static volatile int lastPulse;
static volatile int encoderDirection;

struct encoderStruct
{
    unsigned int pinA;
    unsigned int pinB;
    unsigned int lastCode;
    unsigned int busy;
};

// Default values.
static volatile struct encoderStruct test =
{
    .pinA = 4,
    .pinB = 5,
    .lastCode = 0,
    .busy = false
};


// ****************************************************************************
//  Returns encoder direction.
// ****************************************************************************
static int getEncoderDirection( volatile struct encoderStruct *pulse )
{
    int direction = 0;
    unsigned int pinA = digitalRead( pulse->pinA );
    unsigned int pinB = digitalRead( pulse->pinB );
    // Shift pin A and combine with pin B.
    unsigned int code = ( pinA << 1 ) | pinB;
    printf( "Current pulse code = %i.\n", code );
    // Shift and combine with previous readings.
    unsigned int sumCode = ( pulse->lastCode << 2 ) | code;
    printf( "Combined pulse code = %i.\n", sumCode );

    if ( sumCode == 0b0001 ||
         sumCode == 0b0111 ||
         sumCode == 0b1110 ||
         sumCode == 0b1000 )
         direction = 1;
    else
    if ( sumCode == 0b1011 ||
         sumCode == 0b1101 ||
         sumCode == 0b0100 ||
         sumCode == 0b0010 )
         direction = -1;

    printf( "Direction = %i.\n", direction );
    pulse->lastCode = code;
    return direction;
}


// ****************************************************************************
//  Rotary encoder pulse.
// ****************************************************************************
static void encoderPulse()
{
    if ( test.busy ) return;
    test.busy = true;
    // Read values at pins.
    int direction = getEncoderDirection( &test );
    test.busy = false;
};


// ****************************************************************************
//  Main section.
// ****************************************************************************
int main()
{


    // ************************************************************************
    //  Initialise wiringPi.
    // ************************************************************************
    wiringPiSetup ();
    pinMode( test.pinA, INPUT );
    pullUpDnControl( test.pinA, PUD_UP );
    pinMode( test.pinB, INPUT );
    pullUpDnControl( test.pinB, PUD_UP );


    // ************************************************************************
    //  Register interrupt functions.
    // ************************************************************************
    // Are both needed since both pins should trigger at the same time.
    wiringPiISR( test.pinA, INT_EDGE_BOTH, &encoderPulse );
    wiringPiISR( test.pinB, INT_EDGE_BOTH, &encoderPulse );


    // ************************************************************************
    //  Wait for GPIO activity.
    // ************************************************************************
    while ( 1 )
    {
        delay( 1000 );
    }
    return 0;
}
