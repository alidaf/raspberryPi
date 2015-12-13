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

    gcc testHD44780gpio.c hd44780.c -Wall -o testHD44780gpio
                     -lwiringPi -lpthread -lhd44780Pi

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    11/12/2015  This program.

    Contributors:

    Changelog:

        v0.1    Original version.
        v0.2    Rewrote code into libraries.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "hd44780Pi.h"

int main()
{
    uint8_t data      = 0;  // 4-bit mode.
    uint8_t lines     = 1;  // 2 display lines.
    uint8_t font      = 1;  // 5x8 font.
    uint8_t display   = 1;  // Display on.
    uint8_t cursor    = 0;  // Cursor off.
    uint8_t blink     = 0;  // Blink (block cursor) off.
    uint8_t counter   = 1;  // Increment DDRAM counter after data write
    uint8_t shift     = 0;  // Do not shift display after data write.
    uint8_t mode      = 0;  // Shift cursor.
    uint8_t direction = 0;  // Right.

    // Initialise display.
    hd44780Init( data, lines, font, display, cursor, blink,
                 counter, shift, mode, direction );

    // Set time display properties.
    struct timeStruct textTime =
    {
        .row = 0,
        .delay = 300,
        .align = CENTRE,
        .format = HMS
    };

    // Set text display properties.
    struct dateStruct textDate =
    {
        .row = 0,
        .delay = 300,
        .align = CENTRE,
        .format = DAY_DMY
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
    pthread_create( &threads[0], NULL, displayTime, (void *) &textTime );
//    pthread_create( &threads[1], NULL, displayPacMan, (void *) pacManRow );
    pthread_create( &threads[1], NULL, displayTicker, (void *) &ticker );

    while (1)
    {
    };

    displayClear();
    displayHome();

    // Clean up threads.
    pthread_mutex_destroy( &displayBusy );
    pthread_exit( NULL );

}
