// ****************************************************************************
// ****************************************************************************
/*
    testInt:

    Program to test rotary encoder using a state table.

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

struct rotStruct
{
    unsigned int pinA;
    unsigned int pinB;
    unsigned char state;
    int dir;
};

static struct rotStruct rotEnc =
{
    .pinA = 8,
    .pinB = 9,
    .state = 0,
    .dir = 0
};


// ****************************************************************************
//  Returns the equivalent string for a binary nibble.
// ****************************************************************************
static void getBinaryString( unsigned int nibble, char *binary )
{
    static unsigned int loop;
    for ( loop = 0; loop < 4; loop++ )
        sprintf( binary, "%s%d", binary, !!(( nibble << loop ) & 0x8 ));
}

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


const unsigned char encStates[7][4] =
    {{ 0x0, 0x2, 0x4, 0x0 },
     { 0x3, 0x0, 0x1, 0x10 },
     { 0x3, 0x2, 0x0, 0x0 },
     { 0x3, 0x2, 0x1, 0x0 },
     { 0x6, 0x0, 0x4, 0x0 },
     { 0x6, 0x5, 0x0, 0x20 },
     { 0x6, 0x5, 0x4, 0x0 }};

// ****************************************************************************
//  Interrupt function.
// ****************************************************************************
static void getPulse( void )
{
    unsigned char code = ( digitalRead( rotEnc.pinB ) << 1 ) |
                           digitalRead( rotEnc.pinA );

    rotEnc.state = encStates[ rotEnc.state & 0xf ][ code ];
    unsigned char direction = rotEnc.state & 0x30;

    if ( direction )
        rotEnc.dir = ( direction == 0x10 ? -1 : 1 );
    else
        rotEnc.dir = 0;

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
    wiringPiSetup ();
    pinMode( rotEnc.pinA, INPUT );
    pinMode( rotEnc.pinB, INPUT );
    pullUpDnControl( rotEnc.pinA, PUD_UP );
    pullUpDnControl( rotEnc.pinB, PUD_UP );


    // ************************************************************************
    //  Register interrupt functions.
    // ************************************************************************
    // Are both needed since both rotEnc. should trigger at the same time?
//    wiringPiISR( rotEnc.numA, INT_EDGE_BOTH, &getPulse );
//    wiringPiISR( rotEnc.numB, INT_EDGE_FALLING, &getPulse );


    // ************************************************************************
    //  Wait for GPIO activity.
    // ************************************************************************
    while ( 1 )
    {

        getPulse();
        if ( rotEnc.dir != 0 )
            printf( "Direction = %i.\n", rotEnc.dir );

//        unsigned static int i;
//        for ( i = 0; i < 16; i++ )
//            printf( "Binary for %i = %s.\n", i, getBinaryString( i ));
//        unsigned static int i;
//        for ( i = 0; i < 16; i++ )
//        {
//            char binary[5] = {""};
//            getBinaryString( i, binary );
//            printf( "Binary for %i = %s.\n", i, binary );
//        }

//        if ( test.encodedA & ( test.encodedB ))
//        {
//            combinecode( &test );
//            char binary[5] = {""};
//            getBinaryString( test.currentCode, binary );
//            printf( "Code = %i.\n", binary );
//            test.encodedA = test.encodedB = false;
//        }
    }


    return 0;
}
