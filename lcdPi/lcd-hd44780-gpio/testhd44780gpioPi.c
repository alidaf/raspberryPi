/*
//  ===========================================================================

    testHD44780gpio:

    Tests HD44780 LCD display driver for the Raspberry Pi (GPIO version).

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

#define Version "Version 0.2"

/*
//  ---------------------------------------------------------------------------

    Compile with:

    gcc testHD44780gpio.c hd44780.c -Wall -o testhd44780gpio
                     -lwiringPi -lpthread -lhd44780Pi

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    14/12/2015  This program.

    Contributors:

    Changelog:

        v0.1    Original version.
        v0.2    Rewrote code into libraries.

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

#include "hd44780gpioPi.h"

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

    // Initialise display.
    hd44780Init( data, lines, font, display, cursor, blink,
                 counter, shift, mode, direction );

    // Set up structure to display current time.
    struct Calendar time =
    {
        .row = 1,
        .col = 4,
        .length = 16,
        .format[0] = "%H:%M:%S",
        .format[1] = "%H %M %S",
        .delay = 0.5
    };

    // Set up structure to display current date.
    struct Calendar date =
    {
        .row = 0,
        .col = 0,
        .length = 16,
        .format[0] = "%a %d %b %Y",
        .format[1] = "%a %d %b %Y",
        .delay = 360
    };

    // Set ticker tape properties.
    struct tickerStruct ticker =
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

    displayClear();
    displayHome();

    // Clean up threads.
    pthread_mutex_destroy( &displayBusy );
    pthread_exit( NULL );

}
