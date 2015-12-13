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

#define SIMPLE_TABLE_COLS 16
#define SIMPLE_TABLE { 0,-1, 1, 0, 1, 0, 0,-1,-1, 0, 0, 1, 0, 1,-1, 0 }

#define HALF_TABLE_ROWS 6
#define HALF_TABLE_COLS 4

#define HALF_TABLE {{ 0x03, 0x02, 0x01, 0x00 },
                    { 0x23, 0x00, 0x01, 0x00 },
                    { 0x13, 0x02, 0x00, 0x00 },
                    { 0x03, 0x05, 0x04, 0x00 },
                    { 0x03, 0x03, 0x04, 0x10 },
                    { 0x03, 0x05, 0x03, 0x20 }}


#define FULL_TABLE_ROWS 7
#define FULL_TABLE_COLS 4

#define FULL_TABLE {{ 0x00, 0x02, 0x01, 0x00 },
                    { 0x03, 0x00, 0x01, 0x10 },
                    { 0x03, 0x02, 0x00, 0x00 },
                    { 0x03, 0x02, 0x01, 0x00 },
                    { 0x06, 0x00, 0x04, 0x00 },
                    { 0x06, 0x05, 0x00, 0x20 },
                    { 0x06, 0x05, 0x04, 0x00 }}


// Data structures ------------------------------------------------------------

volatile int8_t encoderDirection;   // Encoder direction.
//volatile int8_t encoderState;       // Encoder state, abAB.
volatile int8_t buttonState;        // Button state, on or off.

// Decoder methods. See description of encoder functions below.
enum decode_t { SIMPLE_1, SIMPLE_2, SIMPLE_4, HALF, FULL }; 

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

    There are a variety of ways to decode the information but a simple
    state machine method by Michael Kellet, http://www.mkesc.co.uk/ise.pdf,
    is efficient and provides 3 modes, each giving different resolutions:

    SIMPLE_1 - Interrupt on leading edge of A, (1x)
    SIMPLE_2 - Interrupt on both edges of A. Sample A & B, (2x).
    SIMPLE_4 - Interrupt on both edges of A and B. Sample A & B, (4x).

    The state table contains directions for all possible combinations of abAB.

            +---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |dec|00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
            +---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
            |dir|00|-1|+1|00|+1|00|00|-1|-1|00|00|+1|00|+1|-1|00|
            +---+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

    An alternative method using a state transition table that helps with
    noisy encoders by ignoring invalid transitions between states has been
    offered by Ben Buxton, http://ww.buxtronix.net, and offers 2
    modes with different resolutions. Both require reads of A & B.

    HALF - Outputs direction after half and full steps.
    FULL - Outputs direction after full step only.

    Each row is a state. Each cell in that row contains the new state to set
    based on the encoder output, AB. The direction is determined when the
    state reaches 0x10 (+ve) or 0x20 (-ve).

    Half mode - outputs direction after half and full steps.
                +-------------+-------------------+
                |             | Encoder output AB |
                | Transitions +-------------------+
                |             | 00 | 01 | 10 | 11 |
                +-------------+----+----+----+----+
              ->| Start       | 03 | 02 | 01 | 00 |
                | -ve begin   | 23 | 00 | 01 | 00 |
                | +ve begin   | 13 | 02 | 00 | 00 |
                | Halfway     | 03 | 05 | 04 | 00 |
                | +ve begin   | 03 | 03 | 04 | 10 |-> +ve
                | -ve begin   | 03 | 05 | 03 | 20 |-> -ve
                +-------------+----+----+----+----+

    Full mode - outputs direction after full step only.
                +-------------+-------------------+
                |             | Encoder output AB |
                | Transitions +-------------------+
                |             | 00 | 01 | 10 | 11 |
                +-------------+----+----+----+----+
              ->| Start       | 00 | 02 | 01 | 00 |
                | +ve end     | 03 | 00 | 01 | 10 |-> +ve
                | +ve begin   | 03 | 02 | 00 | 00 |
                | +ve next    | 03 | 02 | 01 | 00 |
                | -ve begin   | 06 | 00 | 04 | 00 |
                | -ve end     | 06 | 05 | 00 | 20 |-> -ve
                | -ve next    | 06 | 05 | 04 | 00 |
                +-------------+----+----+----+----+

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
