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

#define STATE_TABLE_SIZE 16
#define STATE_TABLE { 0,-1, 1, 0, 1, 0, 0,-1,-1, 0, 0, 1, 0, 1,-1, 0 }

// Data structures ------------------------------------------------------------

volatile int8_t encoderDirection;   // Encoder direction.
volatile int8_t encoderState;       // Encoder state, abAB.
volatile int8_t buttonState;        // Button state, on or off.

enum decode_t { SIMPLE, HALF, FULL };   // Decoder method. See below.

struct encoderStruct
{
    uint8_t       gpioA; // GPIO for encoder pin A.
    uint8_t       gpioB; // GPIO for encoder pin B.
    uint16_t      delay; // Sensitivity delay (uS).
    enum decode_t mode;  // Simple, half or full quadrature.
}   encoder;

struct buttonStruct
{
    uint8_t gpio;   // GPIO for button pin.
}   button;

//  Description of rotary encoder function. -----------------------------------
/*
    Quadrature encoding:

          :   :   :   :   :   :   :   :   :
          :   +-------+   :   +-------+   :       +-----+---+---+---+---+----+
          :   |   :   |   :   |   :   |   :       | dir | a | b | A | B | dc |
      A   :   |   :   |   :   |   :   |   :       +-----+---+---+---+---+----+
      --------+   :   +-------+   :   +-------    | +ve | 0 | 0 | 1 | 0 | 02 |
          :   :   :   :   :   :   :   :   :       |     | 1 | 0 | 1 | 1 | 11 |
          :   :   :   :   :   :   :   :   :       |     | 1 | 1 | 0 | 1 | 13 |
          +-------+   :   +-------+   :   +---    |     | 0 | 1 | 0 | 0 | 04 |
          |   :   |   :   |   :   |   :   |       +-----+---+---+---+---+----+
      B   |   :   |   :   |   :   |   :   |       | -ve | 1 | 1 | 1 | 0 | 14 |
      ----+   :   +-------+   :   +-------+       |     | 1 | 0 | 0 | 0 | 08 |
          :   :   :   :   :   :   :   :   :       |     | 0 | 0 | 0 | 1 | 01 |
        1 : 2 : 3 : 4 : 1 : 2 : 3 : 4 : 1 : 2     |     | 0 | 1 | 1 | 1 | 07 |
          :   :   :   :   :   :   :   :   :       +-----+---+---+---+---+----+

    A & B are current readings and a & b are the previous readings.
    hx & dc are the hex and decimal equivalents of nibble abAB.

    There are 3 ways to decode the information, each giving different
    resolutions:
    Simple mode - Interrupt on leading edge of A. Sample B.
    Half mode   - Interrupt on both edges of A. Sample A & B. (2x).
    Full mode   - Interrupt on both edges of A and B. Sample A & B. (4x).

    State table can be used for all modes:

            +---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |dec|00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
            +---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |dir|00|-1|+1|00|+1|00|00|-1|-1|00|00|+1|00|+1|-1|00|
            +---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
*/

// ----------------------------------------------------------------------------
//  Returns encoder direction in encoderDirection. Call by interrupt on GPIOs.
// ----------------------------------------------------------------------------
/*
    direction = +1: +ve direction.
              =  0: no change determined.
              = -1: -ve direction.
*/
void setEncoderDirection( void );

// ----------------------------------------------------------------------------
//  Returns button state in buttonState. Call by interrupt on GPIO.
// ----------------------------------------------------------------------------
void setButtonState( void );

// ----------------------------------------------------------------------------
//  Initialises encoder and button GPIOs.
// ----------------------------------------------------------------------------
/*
    Send 0xFF for button if no GPIO present.
*/
void encoderInit( uint8_t encoderA, uint8_t encoderB, uint8_t button );

#endif
