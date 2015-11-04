// ****************************************************************************
// ****************************************************************************
/*
    testTabInt:

    Program to test rotary encoder using a state table and interrupts.

    Copyright 2015 by Darren Faulke <darren@alidaf.co.uk>
    Based on algorithm by Ben Buxton - see http://www.buxtronix.net

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
//  Compile with gcc testInt.c -o testTab -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//     -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//     -ffast-math -pipe -O3

//  Authors:    D.Faulke    03/11/2015
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

struct encoderStruct
{
    unsigned int gpio1;
    unsigned int gpio2;
    unsigned char state;
    int direction;
    bool busy;
};

static struct encoderStruct encoder =
{
    .gpio1 = 8,
    .gpio2 = 9,
    .state = 0,
    .direction = 0,
    .busy = false
};


// ****************************************************************************
//  Rotary encoder functions.
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


// ****************************************************************************
//  State table for full step mode.
// ****************************************************************************
/*
    +---------+---------+---------+---------+
    | AB = 00 | AB = 01 | AB = 10 | AB = 11 |
    +---------+---------+---------+---------+
    | START   | C/W 1   | A/C 1   | START   |
    | C/W +   | START   | C/W X   | C/W DIR |
    | C/W +   | C/W 1   | START   | START   |
    | C/W +   | C/W 1   | C/W X   | START   |
    | A/C +   | START   | A/C 1   | START   |
    | A/C +   | A/C X   | START   | A/C DIR |
    | A/C +   | A/C X   | A/C 1   | START   |
    +---------+---------+---------+---------+
*/

const unsigned char stateTable[7][4] =
    {{ 0x0, 0x2, 0x4, 0x0 },
     { 0x3, 0x0, 0x1, 0x10 },
     { 0x3, 0x2, 0x0, 0x0 },
     { 0x3, 0x2, 0x1, 0x0 },
     { 0x6, 0x0, 0x4, 0x0 },
     { 0x6, 0x5, 0x0, 0x20 },
     { 0x6, 0x5, 0x4, 0x0 }};


// ****************************************************************************
//  Returns encoder direction in rotEnc struct.
// ****************************************************************************
static void encoderFunction()
{
    unsigned char code = ( digitalRead( encoder.gpio2 ) << 1 ) |
                           digitalRead( encoder.gpio1 );
    encoder.state = stateTable[ encoder.state & 0xf ][ code ];
    unsigned char direction = encoder.state & 0x30;
    if ( direction )
        encoder.direction = ( direction == 0x10 ? -1 : 1 );
    else
        encoder.direction = 0;
    return;
};


// ****************************************************************************
//  Main section.
// ****************************************************************************
int main()
{

    // ************************************************************************
    //  Initialise wiringPi.
    // ************************************************************************
    wiringPiSetup();
    pinMode( encoder.gpio1, INPUT );
    pinMode( encoder.gpio2, INPUT );
    pullUpDnControl( encoder.gpio1, PUD_UP );
    pullUpDnControl( encoder.gpio2, PUD_UP );


    // ************************************************************************
    //  Register interrupt functions.
    // ************************************************************************
    /*
        Calling via wiringPi interrupt produces strange behaviour.
        Rotary encoder has multiple pulses on both pins that may fire AB or BA.
        Also, bouncing can produce random interrupts.
        Multiple interrupt calls create race conditions.
        May be better to use threads.
    */

    wiringPiISR( encoder.gpio1, INT_EDGE_BOTH, &encoderFunction );
    wiringPiISR( encoder.gpio2, INT_EDGE_BOTH, &encoderFunction );


    // ************************************************************************
    //  Wait for GPIO activity.
    // ************************************************************************
    while ( 1 )
    {
        // Check for a change in the direction.
//        encoderFunction( &encoder );
        if ( encoder.direction != 0 )
            printf( "Direction = %i.\n", encoder.direction );
        encoder.direction = 0;
        delay( 1 );
    }

    return 0;
}
