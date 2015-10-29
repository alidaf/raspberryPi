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
//#include <pthread.h>

struct encoderStruct
{
    unsigned int pinA;
    unsigned int pinB;
    unsigned int codeA;
    unsigned int codeB;
    unsigned int currentCode;
    unsigned int lastCode;
    bool busyA;
    bool busyB;
    bool pinARead;
    bool pinBRead;
    bool encodedA;
    bool encodedB;
    int direction;
};

// Default values.
static volatile struct encoderStruct test =
{
    .pinA = 8,  // GPIO2.
    .pinB = 9,  // GPIO3.
    .busyA = false,
    .busyB = false,
    .encodedA = false,
    .encodedB = false,
    .direction = 0
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

// ****************************************************************************
//  Returns encoder direction.
// ****************************************************************************
static int getEncoderDirection( unsigned int pulseCode )
{
    if ( pulseCode == 0b0001 ||
         pulseCode == 0b0111 ||
         pulseCode == 0b1110 ||
         pulseCode == 0b1000 )
         return 1;
    else
    if ( pulseCode == 0b1011 ||
         pulseCode == 0b1101 ||
         pulseCode == 0b0100 ||
         pulseCode == 0b0010 )
         return -1;

    return 0;
}

static int combinePulseCode( volatile struct encoderStruct *pulse )
{
    pulse->currentCode = ( pulse->codeA << 1 ) | pulse->codeB;
    pulse->currentCode = ( pulse->currentCode << 2 ) | pulse->lastCode;
    printf( "Current code = %d, last code = %d.\n",
             pulse->currentCode,
             pulse->lastCode );
    pulse->lastCode = pulse->currentCode;
    return 0;
}

// ****************************************************************************
//  Returns pulse code.
// ****************************************************************************
static int getPulseCode( unsigned int pin )
{
    return ( digitalRead( pin ));
}

// ****************************************************************************
//  Rotary encoder pulse for pin A.
// ****************************************************************************
static void encoderPulseA()
{
    if ( test.busyA ) return;
    test.busyA = true;
    test.codeA = getPulseCode( test.pinA );
    printf( "Code A = %d.\n", test.codeA );
    test.pinARead = true;
    test.busyA = false;
    test.encodedA = true;
};
// ****************************************************************************
//  Rotary encoder pulse for pin B.
// ****************************************************************************
static void encoderPulseB()
{
    if ( test.busyB ) return;
    test.busyB = true;
    test.codeB = getPulseCode( test.pinB );
    printf( "Code B = %d.\n", test.codeB );
    test.pinBRead = true;
    test.busyB = false;
    test.encodedB = true;
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
    // Are both needed since both pins should trigger at the same time?
    wiringPiISR( test.pinA, INT_EDGE_FALLING, &encoderPulseA );
    wiringPiISR( test.pinB, INT_EDGE_FALLING, &encoderPulseB );


    // ************************************************************************
    //  Wait for GPIO activity.
    // ************************************************************************
    while ( 1 )
    {

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
        if ( test.encodedA & ( test.encodedB ))
        {
            combinePulseCode( &test );
            char binary[5] = {""};
            getBinaryString( test.currentCode, binary );
            printf( "Code = %i.\n", binary );
            test.encodedA = test.encodedB = false;
        }
    }


    return 0;
}
