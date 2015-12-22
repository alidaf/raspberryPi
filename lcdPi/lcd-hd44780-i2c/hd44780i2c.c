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
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/21952b.pdf
        An essential article on LCD initialisation by Donald Weiman.
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

    Authors:        D.Faulke    22/12/2015  This program.

    Contributors:

    Changelog:

        v0.1    Original version.

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
#include <pthread.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "hd44780i2c.h"
#include "mcp23017.h"


//  Debugging functions. ------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns binary string for a number of bits.
//  ---------------------------------------------------------------------------
static char *getBinaryString( uint16_t data, uint8_t bits )
{
    static char binary[128]; // Arbitrary limit.
    if ( bits > 128 ) bits = 128;
    uint16_t i;
    for ( i = 0; i < bits; i++ )
        binary[i] = (( data >> ( bits - i - 1 )) & 1 ) + '0';
    binary[i] = '\0';
    return binary;
};

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
    {
        printf( "Writing command %s.\n", getBinaryString( data, 8 ));
        mcp23017ClearBitsByte( mcp23017, OLATA, hd44780->rs );
    }
    else
    {
        printf( "Writing data %s.\n", getBinaryString( data, 8 ));
        mcp23017SetBitsByte( mcp23017, OLATA, hd44780->rs );
    }

    // Write byte to HD44780 via MCP23017.
    mcp23017WriteByte( mcp23017, OLATB, data );

    printf( "GPIOB <- %s, GPIOA <- %s.\n",
            getBinaryString( data, 8 ),
            getBinaryString( hd44780->rs, 8 ));
    printf( "GPIOB -> %s, GPIOA -> %s.\n",
            getBinaryString( mcp23017ReadByte( mcp23017, OLATB ), 8 ),
            getBinaryString( mcp23017ReadByte( mcp23017, OLATA ), 8 ));

    // Toggle enable bit to send nibble via output latch.
    hd44780ToggleEnable( mcp23017, hd44780 );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Writes a data string.
//  ---------------------------------------------------------------------------
int8_t hd44780WriteString( struct mcp23017 *mcp23017, struct hd44780 *hd44780,
                           char *string )
{
    uint8_t i;

    // Sends string to LCD byte by byte.
    for ( i = 0; i < strlen( string ); i++ )
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
    usleep( 42000 ); // >40mS@3V.

    // Need to write low nibbles only as display starts off in 8-bit mode.
    // Sending high nibble first (0x0) causes init to fail and the display
    // subsequently shows garbage.
    printf( "\nInitialising LCD display.\n\n" );
//    hd44780WriteByte( mcp23017, hd44780, 0x03, MODE_COMMAND );
//    hd44780ToggleEnable( mcp23017, hd44780 );
//    usleep( 4200 );  // >4.1mS.
//    hd44780WriteByte( mcp23017, hd44780, 0x03, MODE_COMMAND );
//    hd44780ToggleEnable( mcp23017, hd44780 );
//    usleep( 150 );   // >100uS.
//    hd44780WriteByte( mcp23017, hd44780, 0x03, MODE_COMMAND );
//    hd44780ToggleEnable( mcp23017, hd44780 );
//    usleep( 150 );   // >100uS.
//    hd44780WriteByte( mcp23017, hd44780, 0x02, MODE_COMMAND );
//    hd44780ToggleEnable( mcp23017, hd44780 );
//    usleep( 50 );   // >37uS.

    // Set actual function mode - cannot be changed after this point
    // without reinitialising.
    printf( "\nSetting LCD functions.\n" );
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

    printf( "\nFinished initialising LCD display.\n" );
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
//  example: Pac Man and pulsing heart.
//  ---------------------------------------------------------------------------
/*
    PacMan 1        PacMan 2        Ghost 1         Ghost 2
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00,   00000 = 0x00
    00000 = 0x00,   00000 = 0x00,   01110 = 0x0e,   01110 = 0x0e
    01110 = 0x0e,   01111 = 0x0f,   11001 = 0x19,   11001 = 0x13
    11011 = 0x1b,   10110 = 0x16,   11101 = 0x1d,   11011 = 0x17
    11111 = 0x1f,   11100 = 0x1c,   11111 = 0x1f,   11111 = 0x1f
    11111 = 0x1f,   11110 = 0x1e,   11111 = 0x1f,   11111 = 0x1f
    01110 = 0x0e,   01111 = 0x0f,   10101 = 0x15,   01010 = 0x1b
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00,   00000 = 0x00

    Heart 1         Heart 2         Pac Man 3
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00
    01010 = 0x0a,   00000 = 0x00,   00000 = 0x00
    11111 = 0x1f,   01010 = 0x0a,   11110 = 0x1e
    11111 = 0x1f,   01110 = 0x0e,   01101 = 0x0d
    11111 = 0x1f,   01110 = 0x0e,   00111 = 0x07
    01110 = 0x0e,   00100 = 0x04,   01111 = 0x0f
    00100 = 0x04,   00000 = 0x00,   11110 = 0x1e
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00
*/
const uint8_t pacMan[CUSTOM_MAX][CUSTOM_SIZE] =
{
 { 0x00, 0x00, 0x0e, 0x1b, 0x1f, 0x1f, 0x0e, 0x00 },
 { 0x00, 0x00, 0x0f, 0x16, 0x1c, 0x1e, 0x0f, 0x00 },
 { 0x00, 0x0e, 0x19, 0x1d, 0x1f, 0x1f, 0x15, 0x00 },
 { 0x00, 0x0e, 0x13, 0x17, 0x1f, 0x1f, 0x1b, 0x00 },
 { 0x00, 0x0a, 0x1f, 0x1f, 0x1f, 0x0e, 0x04, 0x00 },
 { 0x00, 0x00, 0x0a, 0x0e, 0x0e, 0x04, 0x00, 0x00 },
 { 0x00, 0x00, 0x1e, 0x0d, 0x07, 0x0f, 0x1e, 0x00 }};

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
//  Rotates a string passed as buffer by increments.
//  ---------------------------------------------------------------------------
static void rotateString( char *buffer, size_t length, size_t increments )
{
    // if (!text || !*text ) return;
    increments %= length;
    reverseString( buffer, 0, increments );
    reverseString( buffer, increments, length );
    reverseString( buffer, 0, length );

    return;
}

//  ---------------------------------------------------------------------------
//  Displays text on display row as a tickertape.
//  ---------------------------------------------------------------------------
void *displayTicker( void *threadTicker )
{
    // Get parameters.
    struct ticker *ticker = threadTicker;

    // Close thread if text string is too big.
    if ( ticker->length + ticker->padding > TEXT_MAX_LENGTH )
         pthread_exit( NULL );

    // Variables for nanosleep function.
    struct timespec sleepTime = { 0 };  // Structure defined in time.h.
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = ticker->delay * 1000000;

    // Add some padding so rotated text looks better.
    size_t i;
    for ( i = ticker->length; i < ticker->length + ticker->padding; i++ )
        ticker->text[i] = ' ';
    ticker->text[i] = '\0';
    ticker->length = strlen( ticker->text );

    // Set up a text window equal to the number of display columns.
    char buffer[DISPLAY_COLUMNS];

    while ( 1 )
    {
        // Copy the display text.
        strncpy( buffer, ticker->text, DISPLAY_COLUMNS );

        // Lock thread and display ticker text.
        pthread_mutex_lock( &displayBusy );
        hd44780Goto( ticker->mcp23017, ticker->hd44780, ticker->row, 0 );
        hd44780WriteString( ticker->mcp23017, ticker->hd44780, buffer );
        pthread_mutex_unlock( &displayBusy );

        // Delay for readability.
        nanosleep( &sleepTime, NULL );

        // Rotate the ticker text.
        rotateString( ticker->text, ticker->length, ticker->increment );
    }

    pthread_exit( NULL );
};

//  ---------------------------------------------------------------------------
//  Displays formatted date/time strings.
//  ---------------------------------------------------------------------------
void *displayCalendar( void *threadCalendar )
{
    // Get parameters.
    struct calendar *calendar = threadCalendar;

    printf( "Set up local calendar data structure.\n" );

    // Definitions for time.h functions.
    struct tm *timePtr;
    time_t timeVar;

    // Definitions for nanosleep function.
    struct timespec sleepTime = { 0 };
    if ( calendar->delay < 1 )
        sleepTime.tv_nsec = calendar->delay * 1000000000;

    printf( "Set up time types.\n" );

    // Display string.
    char buffer[20] = "";
    uint8_t frame = 0; // Animation frames

    while ( 1 )
    {
        if ( frame > 1 ) frame = 0;
        // Get current date & time.
        timeVar = time( NULL );
        timePtr = localtime( &timeVar );

        strftime( buffer, calendar->length,
                          calendar->format[frame], timePtr );
        frame++;

        // Display time string.
        pthread_mutex_lock( &displayBusy );
        hd44780Goto( calendar->mcp23017,
                     calendar->hd44780,
                     calendar->row,
                     calendar->col );
        hd44780WriteString( calendar->mcp23017, calendar->hd44780, buffer );
        pthread_mutex_unlock( &displayBusy );

        // Sleep for designated delay.
        if ( calendar->delay < 1 ) nanosleep( &sleepTime, NULL );
        else sleep( calendar->delay );
    }
    pthread_exit( NULL );
};
