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

    Authors:        D.Faulke    20/12/2015

    Contributors:

    Changelog:

        v0.1    Original version.
        v0.2    Rewrote code into libraries.

//  Information. --------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander.

    For testing, the HD44780 LCD was connected via a breadboard as follows:

                          GND
        +-----------+      |   10k
        | pin | Fn  |   ,--+--/\/\/--
        |-----+-----|   |       |
        |   1 | VSS |---'       |       +-----------( )-----------+
        |   1 | VDD |--> 5V     |       |  Fn  | pin | pin |  Fn  |
        |   2 | Vo  |-----------'       |------+-----+-----+------|
        |   3 | RS  |------------------>| GPB0 |  01 | 28  | GPA7 |
        |   4 | R/W |------------------>| GPB1 |  02 | 27  | GPA6 |
        |   5 | E   |------------------>| GPB2 |  03 | 26  | GPA5 |
        |   6 | DB0 |                   | GPB3 |  04 | 25  | GPA4 |
        |   7 | DB1 | ,---------------->| GPB4 |  05 | 24  | GPA3 |
        |   8 | DB2 | | ,-------------->| GPB5 |  06 | 23  | GPA2 |
        |   9 | DB3 | | | ,------------>| GPB6 |  07 | 22  | GPA1 |
        |  10 | DB4 |-' | | ,---------->| GPB7 |  08 | 21  | GPA0 |
        |  11 | DB5 |---' | |    3.3V <-|  VDD |  09 | 20  | INTA |
        |  12 | DB6 |-----' |     GND <-|  VSS |  10 | 19  | INTB |
        |  13 | DB7 |-------'           |   NC |  11 | 18  | RST  |----> +3.3V.
        |  14 | A   |--> 3.3V      ,----|  SCL |  12 | 17  | A2   |---,
        |  15 | K   |--> GND       |  ,-|  SDA |  13 | 16  | A1   |---+-> GND
        +-----------+              |  | |   NC |  14 | 15  | A0   |---'
                                   |  | +-------------------------+
                                   |  |
                                   |  v
                                   v SDA1  } Pi
                                  SCL1     } I2C

    Notes:  Vo is connected to the wiper of a 10k trim pot to adjust the
            contrast. A similar (perhaps 5k) pot, could be used to adjust the
            backlight but connecting to 3.3V works OK instead. These displays
            are commonly sold with a single 10k pot.

            The HD44780 logic voltage doesn't seem to like 3.3V but the
            MCP23017 can handle 5V signals.

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
    bool data      = 0;  // 4-bit mode.
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
        printf( "Couldn't init.\n" );
        return -1;
    }

    // Set up hd44780 data.
    hd44780[0]->rs    = 0x0001; // GPB0.
    hd44780[0]->rw    = 0x0002; // GPB1.
    hd44780[0]->en    = 0x0004; // GPB2.
    hd44780[0]->db[0] = 0x0010; // GPB4.
    hd44780[0]->db[1] = 0x0020; // GPB5.
    hd44780[0]->db[2] = 0x0040; // GPB6.
    hd44780[0]->db[3] = 0x0080; // GPB7.

    // Initialise display.
    hd44780Init( mcp23017[0], hd44780[0],
                 data, lines, font, display, cursor, blink,
                 counter, shift, mode, direction );

    // Set up structure to display current time.
    struct calendar time =
    {
        .row = 1,
        .col = 4,
        .length = 16,
        .format[0] = "%H:%M:%S",
        .format[1] = "%H %M %S",
        .delay = 0.5
    };

    // Set up structure to display current date.
    struct calendar date =
    {
        .row = 0,
        .col = 0,
        .length = 16,
        .format[0] = "%a %d %b %Y",
        .format[1] = "%a %d %b %Y",
        .delay = 360
    };

    // Set ticker tape properties.
    struct ticker ticker =
    {
        .text = "This text is really long and used to demonstrate the ticker!",
        .length = strlen( ticker.text ),
        .padding = 6,
        .row = 1,
        .increment = 1,
        .delay = 300
    };

    // Create threads and mutex for animated display functions.
    pthread_mutex_init( &displayBusy, NULL );
    pthread_t threads[2];

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
