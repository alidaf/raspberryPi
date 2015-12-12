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

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    12/12/2015

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

#ifndef ROTENCPI_H
#define ROTENCPI_H

// Data structures ------------------------------------------------------------

struct encoderStruct
{
    uint8_t gpioA;
    uint8_t gpioB;
    uint8_t state;
    int8_t  direction;
} encoder;

struct buttonStruct
{
    uint8_t gpio;
    bool    state;
} button;

/*  Description of rotary encoder function. -----------------------------------

    Quadrature encoding:

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
//  Returns encoder direction in encoderStruct. Call by interrupt on GPIOs.
// ----------------------------------------------------------------------------
/*
    direction = +1: +ve direction.
              =  0: no change determined.
              = -1: -ve direction.
*/
void encoderDirection( void );

// ----------------------------------------------------------------------------
//  Returns button state in buttonStruct. Call by interrupt on GPIO.
// ----------------------------------------------------------------------------
void buttonState( void );

// ----------------------------------------------------------------------------
//  Initialises encoder and button GPIOs.
// ----------------------------------------------------------------------------
/*
    Send 0xFF for button if no GPIO present.
*/
void encoderInit( uint8_t encoderA, uint8_t encoderB, uint8_t button );

#endif
