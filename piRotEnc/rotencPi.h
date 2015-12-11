// ****************************************************************************
// ****************************************************************************
/*
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
*/
// ****************************************************************************
// ****************************************************************************

//  Compilation:
//
//  Compile with gcc -c -fpic rotencPi.c -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    10/12/2015
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

//  To Do:
//      Write GPIO and interrupt routines to replace wiringPi.
//
#ifndef ROTENCPI_H
#define ROTENCPI_H

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

// ============================================================================
// Data structures
// ============================================================================

struct encoderStruct
{
    unsigned char gpioA;
    unsigned char gpioB;
    unsigned char state;
    int direction;
} encoder;

pthread_mutex_t encoderBusy;

// ============================================================================
//  Encoder functions.
// ============================================================================

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

    State table for full step mode:
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

// ----------------------------------------------------------------------------
//  Returns encoder direction - should be called by an interrupt function.
// ----------------------------------------------------------------------------
/*
    Returns: +1 = +ve direction.
              0 = no change determined.
             -1 = -ve direction.
*/
void encoderDirection();


// ----------------------------------------------------------------------------
//  Initialises encoder gpio pins.
// ----------------------------------------------------------------------------
void encoderInit( unsigned char gpioA, unsigned char gpioB );

#endif
