/*
//  ===========================================================================

    testhd44780i2c:

    Tests HD44780 LCD display driver for the Raspberry Pi (I2C version).

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

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
*/

#define Version "Version 0.1"

/*
//  ---------------------------------------------------------------------------

    Compile with:

    gcc testhd44780i2c.c hd44780i2c.c mcp23017.c -Wall -o testhd44780i2c
                     -lpthread

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    24/12/2015

    Contributors:

    Changelog:

        v0.1    Original version.

//  Information. --------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander.

    For testing, the HD44780 LCD was connected via a breadboard as follows:

                       GND
                        |    10k
        +-----------+   +---\/\/\--x
        | pin | Fn  |   |     |
        |-----+-----|   |     |   ,----------------------------------,
        |   1 | VSS |---'     |   | ,--------------------------------|-,
        |   2 | VDD |--> 5V   |   | | ,------------------------------|-|-,
        |   3 | Vo  |---------'   | | |                              | | |
        |   4 | RS  |-------------' | | +-----------( )-----------+  | | |
        |   5 | R/W |---------------' | |  Fn  | pin | pin |  Fn  |  | | |
        |   6 | E   |-----------------' |------+-----+-----+------|  | | |
        |   7 | DB0 |------------------>| GPB0 |  01 | 28  | GPA7 |<-' | |
        |   8 | DB1 |------------------>| GPB1 |  02 | 27  | GPA6 |<---' |
        |   9 | DB2 |------------------>| GPB2 |  03 | 26  | GPA5 |<-----'
        |  10 | DB3 |------------------>| GPB3 |  04 | 25  | GPA4 |
        |  11 | DB4 |------------------>| GPB4 |  05 | 24  | GPA3 |
        |  12 | DB5 |------------------>| GPB5 |  06 | 23  | GPA2 |
        |  13 | DB6 |------------------>| GPB6 |  07 | 22  | GPA1 |
        |  14 | DB7 |------------------>| GPB7 |  08 | 21  | GPA0 |
        |  15 | A   |--+----------------|  VDD |  09 | 20  | INTA |
        |  16 | K   |--|----+-----------|  VSS |  10 | 19  | INTB |
        +-----------+  |    |           |   NC |  11 | 18  | RST  |-----> +5V
                       |    |      ,----|  SCL |  12 | 17  | A2   |---,
                       |    |      |  ,-|  SDA |  13 | 16  | A1   |---+-> GND
                       |    |      |  | |   NC |  14 | 15  | A0   |---'
                       v    v      |  | +-------------------------+
                      +5V  GND     |  |
                                 {-+  +-}
                                 {  <<  } Logic level shifter *
                                 {-+  +-}   (bi-directional)
                                   |  |
                                   v  v
                                SCL1  SDA1

    Notes:  Vo is connected to the wiper of a 10k trim pot to adjust the
            contrast. A similar (perhaps 5k) pot could be used to adjust the
            backlight but connecting to 3.3V works OK instead. These displays
            are commonly sold with a single 10k pot.

            * The HD44780 operates slightly faster at 5V but 3.3V works fine.
            The MCP23017 expander can operate at both levels. The Pi's I2C
            pins have pull-up resistors that should protect against 5V levels
            but use a logic level shifter if there is any doubt.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "hd44780i2c.h"
#include "mcp23017.h"

int main()
{
    bool data      = 1;  // 8-bit mode.
    bool lines     = 1;  // 2 display lines.
    bool font      = 1;  // 5x8 font.
    bool display   = 1;  // Display on.
    bool cursor    = 0;  // Cursor off.
    bool blink     = 0;  // Blink (block cursor) off.
    bool counter   = 1;  // Increment DDRAM counter after data write
    bool shift     = 0;  // Do not shift display after data write.
    bool mode      = 0;  // Shift cursor.
    bool direction = 0;  // Right.

    int8_t err;

    // Initialise MCP23017.
    err = mcp23017Init( 0x20 );
    if ( err < 0 )
    {
        printf( "Couldn't init. Try loading i2c-dev module.\n" );
        return -1;
    }

    // Set direction of GPIOs.
    mcp23017WriteByte( mcp23017[0], IODIRA, 0x00 ); // Output.
    mcp23017WriteByte( mcp23017[0], IODIRB, 0x00 ); // Output.

    // Writes to latches are the same as writes to GPIOs.
    mcp23017WriteByte( mcp23017[0], OLATA, 0x00 ); // Clear pins.
    mcp23017WriteByte( mcp23017[0], OLATB, 0x00 ); // Clear pins.

    // Set BANK bit for 8-bit mode.
    mcp23017WriteByte( mcp23017[0], IOCONA, 0x80 );

    struct hd44780 *hd44780this;

    hd44780this = malloc( sizeof( struct hd44780 ));

/*
    +---------------------------------------------------------------+
    |             GPIOB             |             GPIOA             |
    |-------------------------------+-------------------------------|
    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    |---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---|
    |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|RS |R/W| E |---|---|---|---|---|
    +---------------------------------------------------------------+
*/

    // Set up hd44780 data.
    hd44780this->rs    = 0x80; // HD44780 RS pin.
    hd44780this->rw    = 0x40; // HD44780 R/W pin.
    hd44780this->en    = 0x20; // HD44780 E pin.

    hd44780[0] = hd44780this;

    // Initialise display.
    hd44780Init( mcp23017[0], hd44780[0],
                 data, lines, font, display, cursor, blink,
                 counter, shift, mode, direction );

    hd44780WriteString( mcp23017[0], hd44780[0], "Initialised" );

    // Set up structure to display current time.
    struct calendar time =
    {
        .mcp23017      = mcp23017[0],  // Initialised MCP23017.
        .hd44780       = hd44780[0],   // Initialised HD44780.
        .delay.tv_sec  = 0,            // 0 full seconds.
        .delay.tv_usec = 500000,       // 0.5 seconds.
        .row = 1,
        .col = 4,
        .length = 16,
        .frames = FRAMES_MAX,
        .format[0] = "%H:%M:%S",
        .format[1] = "%H %M %S"
    };

    // Set up structure to display current date.
    struct calendar date =
    {
        .mcp23017      = mcp23017[0],  // Initialised MCP23017.
        .hd44780       = hd44780[0],   // Initialised HD44780.
        .delay.tv_sec  = 60,           // 1 minute.
        .delay.tv_usec = 0,            // 0 microseconds.
        .row = 0,
        .col = 0,
        .length = 16,
        .frames = 1,
        .format[0] = "%a %d %b %Y"
    };

    // Set ticker tape properties.
    struct ticker ticker =
    {
        .mcp23017      = mcp23017[0],  // Initialised MCP23017.
        .hd44780       = hd44780[0],   // Initialised HD44780.
        .delay.tv_sec  = 0,            // Seconds.      }
        .delay.tv_usec = 100000,       // Microseconds. } Adjust for flicker.
        .text = "This text is really long and used to demonstrate the ticker!",
        .length = strlen( ticker.text ),
        .padding = 6,
        .row = 1,
        .increment = 1
    };

    // Create threads and mutex for animated display functions.
    pthread_mutex_init( &displayBusy, NULL );
    pthread_t threads[2];

    /*
        The HD44780 has a slow response so animation times should be as
        large as possible and not mixed with other routines that have high
        animation duty, e.g. ticker and time display with animation.
    */
    pthread_create( &threads[0], NULL, displayCalendar, (void *) &date );
    pthread_create( &threads[1], NULL, displayCalendar, (void *) &time );
//    pthread_create( &threads[1], NULL, displayPacMan, (void *) pacManRow );
//    pthread_create( &threads[1], NULL, displayTicker, (void *) &ticker );

    while (1)
    {
    };

    hd44780Clear( mcp23017[0], hd44780[0] );
    hd44780Home( mcp23017[0], hd44780[0] );

    // Clean up threads.
    pthread_mutex_destroy( &displayBusy );
    pthread_exit( NULL );

}
