/*
//  ===========================================================================

    hd44780i2c:

    Raspberry Pi driver for the HD44780 LCD display via the MCP23017
    port expander.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        HD44780 data sheet.
        - see https://www.sparkfun.com/datasheets/LCD/HD44780.pdf
        MCP23017 data sheet.
        - see http://web.alfredstate.edu/weimandn/
        Interfacing with I2C Devices.
        - see http://elinux.org/Interfacing_with_I2C_Devices

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

    Compile with:

        gcc -c -fpic -Wall hd44780i2c.c mcp23017.c -lpthread

    Also use the following flags for Raspberry Pi optimisation:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    24/12/2015  This program.

    Contributors:

    Changelog:

        v0.1    Original version.
        v0.2    Rewrote to use 8-bit interface.

//  ---------------------------------------------------------------------------

    To Do:
        Add routine to check validity of GPIOs.
        Add support for multiple displays.
        Add read function to check ready (replace delays?).
            - most hobbyists may ground the ready pin.
        Improve error trapping and return codes for all functions.
        Write GPIO and interrupt routines to replace wiringPi.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "hd44780i2c.h"
#include "mcp23017.h"


//  HD44780 display functions. ------------------------------------------------

//  ---------------------------------------------------------------------------
//  Toggles EN (enable) bit in byte mode without changing other bits.
//  ---------------------------------------------------------------------------
void hd44780ToggleEnable( struct mcp23017 *mcp23017, struct hd44780 *hd44780 )
{
    mcp23017SetBitsByte( mcp23017, OLATA, hd44780->en );
    usleep( 5000 ); // 5mS.

    mcp23017ClearBitsByte( mcp23017, OLATA, hd44780->en );
    usleep( 5000 ); // 5mS.
}

//  ---------------------------------------------------------------------------
//  Writes a command or data byte (according to mode).
//  ---------------------------------------------------------------------------
int8_t hd44780WriteByte( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                         uint8_t data, bool mode )
{
/*
    +---------------------------------------------------------------+
    |             GPIOB             |             GPIOA             |
    |-------------------------------+-------------------------------|
    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    |---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---|
    |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|RS |R/W| E |---|---|---|---|---|
    +---------------------------------------------------------------+
*/

    // Set RS bit according to mode.
    if ( mode == 0 )
        mcp23017ClearBitsByte( mcp23017, OLATA, hd44780->rs );
    else
        mcp23017SetBitsByte( mcp23017, OLATA, hd44780->rs );

    // Make sure R/W pin is cleared.
    mcp23017ClearBitsByte( mcp23017, OLATA, hd44780->rw );

    // Write byte to HD44780 via MCP23017.
    mcp23017WriteByte( mcp23017, OLATB, data );

    // Toggle enable bit to send nibble via output latch.
    hd44780ToggleEnable( mcp23017, hd44780 );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Writes a string to display.
//  ---------------------------------------------------------------------------
int8_t hd44780WriteString( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                           char *string, uint8_t len )
{
    uint8_t i;

    // Sends string to LCD byte by byte.
    for ( i = 0; i < len; i++ )
        hd44780WriteByte( mcp23017, hd44780, string[i], MODE_DATA );
    return 0;
};

//  ---------------------------------------------------------------------------
//  Moves cursor to row, position.
//  ---------------------------------------------------------------------------
/*
    All displays, regardless of size, have the same start address for each
    row due to common architecture. Moving from the end of a line to the start
    of the next is not contiguous memory.
*/
int8_t hd44780Goto( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                    uint8_t row, uint8_t pos )
{
    if (( pos < 0 ) | ( pos > DISPLAY_COLUMNS - 1 )) return -1;
    if (( row < 0 ) | ( row > DISPLAY_ROWS - 1 )) return -1;
    // This doesn't properly check whether the number of display
    // lines has been set to 1.

    // Array of row start addresses
    uint8_t rows[DISPLAY_ROWS_MAX] = { ADDRESS_ROW_0, ADDRESS_ROW_1,
                                       ADDRESS_ROW_2, ADDRESS_ROW_3 };

    hd44780WriteByte( mcp23017, hd44780, ( ADDRESS_DDRAM | rows[row] ) + pos,
    				                       MODE_COMMAND );
    return 0;
};

//  Display init and mode functions. ------------------------------------------

//  ---------------------------------------------------------------------------
//  Clears display.
//  ---------------------------------------------------------------------------
int8_t hd44780Clear( struct mcp23017 *mcp23017, struct hd44780 *hd44780 )
{
    hd44780WriteByte( mcp23017, hd44780, DISPLAY_CLEAR, MODE_COMMAND );
    usleep( 1600 ); // Data sheet doesn't give execution time!
    return 0;
};

//  ---------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
//  ---------------------------------------------------------------------------
int8_t hd44780Home( struct mcp23017 *mcp23017, struct hd44780 *hd44780 )
{
    hd44780WriteByte( mcp23017, hd44780, DISPLAY_HOME, MODE_COMMAND );
    usleep( 1600 ); // Needs 1.52ms to execute.
    return 0;
};

//  ---------------------------------------------------------------------------
//  Initialises display. Must be called before any other display functions.
//  ---------------------------------------------------------------------------
int8_t hd44780Init( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                    bool data,    bool lines,  bool font,
                    bool display, bool cursor, bool blink,
                    bool counter, bool shift,
                    bool mode,    bool direction )
{
    // Allow a start-up delay.
    usleep( 40000 );    // >40mS@3V.

    hd44780WriteByte( mcp23017, hd44780, 0x30, MODE_DATA );
    usleep( 4100 );     // >4.1mS.
    hd44780WriteByte( mcp23017, hd44780, 0x30, MODE_DATA );
    usleep( 100 );      // >100uS.
    hd44780WriteByte( mcp23017, hd44780, 0x30, MODE_DATA );
    usleep( 100 );      // >100uS.

    // Set function mode.
    hd44780WriteByte( mcp23017, hd44780,
                      FUNCTION_BASE | ( data  * FUNCTION_DATA  )
                                    | ( lines * FUNCTION_LINES )
                                    | ( font  * FUNCTION_FONT  ),
                      MODE_COMMAND );

    // Display off.
    hd44780WriteByte( mcp23017, hd44780, DISPLAY_BASE, MODE_COMMAND );

    // Set entry mode.
    hd44780WriteByte( mcp23017, hd44780,
                      ENTRY_BASE | ( counter * ENTRY_COUNTER )
                                 | ( shift   * ENTRY_SHIFT   ),
                      MODE_COMMAND );

    // Display should be initialised at this point. Function can no longer
    // be changed without re-initialising.

    // Set display properties.
    hd44780WriteByte( mcp23017, hd44780,
                      DISPLAY_BASE | ( display * DISPLAY_ON     )
                                   | ( cursor  * DISPLAY_CURSOR )
                                   | ( blink   * DISPLAY_BLINK  ),
                      MODE_COMMAND );

    // Set initial display/cursor movement mode.
    hd44780WriteByte( mcp23017, hd44780,
                      MOVE_BASE | ( mode      * MOVE_DISPLAY   )
                                | ( direction * MOVE_DIRECTION ),
                      MODE_COMMAND );

    // Goto start of DDRAM.
    hd44780WriteByte( mcp23017, hd44780, ADDRESS_DDRAM, MODE_COMMAND );

    // Wipe any previous display.
    hd44780Clear( mcp23017, hd44780 );

    return 0;
};

//  Hardware mode settings. ---------------------------------------------------

//  ---------------------------------------------------------------------------
//  Sets entry mode.
//  ---------------------------------------------------------------------------
int8_t hd44780EntryMode( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                         bool counter, bool shift )
{
    hd44780WriteByte( mcp23017, hd44780,
                      ENTRY_BASE | ( counter * ENTRY_COUNTER )
                                 | ( shift   * ENTRY_SHIFT   ),
                      MODE_COMMAND );
    // Clear display.
    hd44780WriteByte( mcp23017, hd44780, DISPLAY_CLEAR, MODE_COMMAND );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Sets display mode.
//  ---------------------------------------------------------------------------
int8_t hd44780DisplayMode( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                           bool display, bool cursor, bool blink )
{
    hd44780WriteByte( mcp23017, hd44780,
                      DISPLAY_BASE | ( display * DISPLAY_ON     )
                                   | ( cursor  * DISPLAY_CURSOR )
                                   | ( blink   * DISPLAY_BLINK  ),
                      MODE_COMMAND );
    // Clear display.
    hd44780WriteByte( mcp23017, hd44780, DISPLAY_CLEAR, MODE_COMMAND );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Shifts cursor or display.
//  ---------------------------------------------------------------------------
int8_t hd44780MoveMode( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                        bool mode, bool direction )
{
    hd44780WriteByte( mcp23017, hd44780,
                      MOVE_BASE | ( mode      * MOVE_DISPLAY   )
                                | ( direction * MOVE_DIRECTION ),
                      MODE_COMMAND );
    // Clear display.
    hd44780WriteByte( mcp23017, hd44780, DISPLAY_CLEAR, MODE_COMMAND );

    return 0;
};

//  Custom characters and animation. ------------------------------------------

//  ---------------------------------------------------------------------------
//  Loads custom characters into CGRAM.
//  ---------------------------------------------------------------------------
int8_t hd44780LoadCustom( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                          const uint8_t newChar[CUSTOM_MAX][CUSTOM_SIZE] )
{
    hd44780WriteByte( mcp23017, hd44780, ADDRESS_CGRAM, MODE_COMMAND );
    uint8_t i, j;
    for ( i = 0; i < CUSTOM_MAX; i++ )
        for ( j = 0; j < CUSTOM_SIZE; j++ )
            hd44780WriteByte( mcp23017, hd44780, newChar[i][j], MODE_DATA );
    hd44780WriteByte( mcp23017, hd44780, ADDRESS_DDRAM, MODE_COMMAND );
    return 0;
};

//  Display functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Reverses a string passed as *buffer between start and end.
//  ---------------------------------------------------------------------------
static void reverseString( char *buffer, size_t start, size_t end )
{
    char temp;
    while ( start < end )
    {
        end--;
        temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
    }

    return;
};

//  ---------------------------------------------------------------------------
//  Calculates time difference in milliseconds in diff. Returns 0 on success.
//  ---------------------------------------------------------------------------
static int8_t timeDiff( struct timeval *diff, struct timeval *t2,
                                              struct timeval *t1 )
{
    long int d = ( t2->tv_usec + 1000000 * t2->tv_sec ) -
                 ( t1->tv_usec + 1000000 * t1->tv_sec );

    diff->tv_sec  = d / 1000000;
    diff->tv_usec = d % 1000000;

    return ( d < 0 ) * -1;
}
