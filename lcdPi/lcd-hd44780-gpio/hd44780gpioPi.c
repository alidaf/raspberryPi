/*
//  ===========================================================================

    hd44780gpioPi:

    HD44780 LCD display driver for the Raspberry Pi (GPIO version).

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        HD44780 data sheet
        - see https://www.sparkfun.com/datasheets/LCD/HD44780.pdf
        An essential article on LCD initialisation by Donald Weiman.
        - see http://web.alfredstate.edu/weimandn/

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

        gcc -c -fpic -Wall hd44780gpio.c -lwiringPi -lpthread

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

    To Do:
        Add animation functions.
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
#include <wiringPi.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "hd44780gpioPi.h"


//  Data structures. ----------------------------------------------------------

struct hd44780Struct hd44780 =
{
    .cols      = 16,   // Display columns.
    .rows      = 2,    // Display rows.
    .gpioRS    = 7,    // Pin 26 (RS).
    .gpioEN    = 8,    // Pin 24 (E).
    .gpioRW    = 11,   // Pin 23 (RW).
    .gpioDB[0] = 25,   // Pin 12 (DB4).
    .gpioDB[1] = 24,   // Pin 16 (DB5).
    .gpioDB[2] = 23,   // Pin 18 (DB6).
    .gpioDB[3] = 18    // Pin 22 (DB7).
};


//  Display functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes data nibble to display.
//  ---------------------------------------------------------------------------
int8_t writeNibble( uint8_t data )
{
    uint8_t i;
    uint8_t nibble;

    // Write nibble to GPIOs
    nibble = data;
    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        digitalWrite( hd44780.gpioDB[i], ( nibble & 1 ));
        delayMicroseconds( 41 );
        nibble >>= 1;
    }

    // Toggle enable bit to send nibble.
    digitalWrite( hd44780.gpioEN, GPIO_SET );
    delayMicroseconds( 41 );
    digitalWrite( hd44780.gpioEN, GPIO_UNSET );
    delayMicroseconds( 41 ); // Commands should take 5mS!

    /*
        Need to add routine to read ready bit.
    */

    return 0;
};

//  ---------------------------------------------------------------------------
//  Writes command byte to display.
//  ---------------------------------------------------------------------------
int8_t writeCommand( uint8_t data )
{
    uint8_t nibble;

    // Set to command mode.
    digitalWrite( hd44780.gpioRS, GPIO_UNSET );
    delayMicroseconds( 41 );

    // High nibble.
    nibble = ( data >> BITS_NIBBLE ) & 0x0f;
    writeNibble( nibble );

    // Low nibble.
    nibble = data & 0x0f;
    writeNibble( nibble );

    /*
        Need to add routine to read ready bit.
    */

    return 0;
};

//  ---------------------------------------------------------------------------
//  Writes data byte to display.
//  ---------------------------------------------------------------------------
int8_t writeData( uint8_t data )
{
    uint8_t nibble;

    // Set to character mode.
    digitalWrite( hd44780.gpioRS, GPIO_SET );
    delayMicroseconds( 41 );

    // High nibble.
    nibble = ( data >> BITS_NIBBLE ) & 0xf;
    writeNibble( nibble );

    // Low nibble.
    nibble = data & 0xf;
    writeNibble( nibble );

    /*
        Need to add routine to read ready bit.
    */

    return 0;
};

//  ---------------------------------------------------------------------------
//  Writes a string of data to display.
//  ---------------------------------------------------------------------------
int8_t writeDataString( char *string )
{
    uint8_t i;

    for ( i = 0; i < strlen( string ); i++ )
        writeData( string[i] );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Moves cursor to row, position.
//  ---------------------------------------------------------------------------
int8_t gotoRowPos( uint8_t row, uint8_t pos )
{
    if (( pos < 0 ) | ( pos > DISPLAY_COLUMNS - 1 )) return -1;
    if (( row < 0 ) | ( row > DISPLAY_ROWS - 1 )) return -1;
    // This doesn't properly check whether the number of display
    // lines has been set to 1.

    // Array of row start addresses
    uint8_t rows[DISPLAY_ROWS_MAX] = { ADDRESS_ROW_0, ADDRESS_ROW_1,
                                       ADDRESS_ROW_2, ADDRESS_ROW_3 };

    writeCommand(( ADDRESS_DDRAM | rows[row] ) + pos );

    return 0;
};


//  Display init and mode functions. ------------------------------------------

//  ---------------------------------------------------------------------------
//  Clears display.
//  ---------------------------------------------------------------------------
int8_t displayClear( void )
{
    writeCommand( DISPLAY_CLEAR );
    delayMicroseconds( 1600 ); // Data sheet doesn't give execution time!

    return 0;
};

//  ---------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
//  ---------------------------------------------------------------------------
int8_t displayHome( void )
{
    writeCommand( DISPLAY_HOME );
    delayMicroseconds( 1600 ); // Needs 1.52ms to execute.

    return 0;
};

//  ---------------------------------------------------------------------------
//  Initialises display. Must be called before any other display functions.
//  ---------------------------------------------------------------------------
int8_t hd44780Init( bool data,   bool lines, bool font,    bool display,
                    bool cursor, bool blink, bool counter, bool shift,
                    bool mode,   bool direction )
{
    uint8_t i;

    // Set up GPIOs with wiringPi.
    wiringPiSetupGpio();

    // Set all GPIO pins to 0.
    digitalWrite( hd44780.gpioRS, GPIO_UNSET );
    digitalWrite( hd44780.gpioEN, GPIO_UNSET );

    // Data pins.
    for ( i = 0; i < PINS_DATA; i++ )
        digitalWrite( hd44780.gpioDB[i], GPIO_UNSET );

    // Set LCD pin modes.
    pinMode( hd44780.gpioRS, OUTPUT );
    pinMode( hd44780.gpioEN, OUTPUT );
    // Data pins.
    for ( i = 0; i < PINS_DATA; i++ )
        pinMode( hd44780.gpioDB[i], OUTPUT );

    // Allow a start-up delay for display initialisation.
    delay( 50 ); // >40mS@3V.

    // Need to write low nibbles only as display starts off in 8-bit mode.
    // Sending high nibble first (0x0) causes init to fail and the display
    // subsequently shows garbage.
    writeNibble( 0x3 );
    delay( 5 );                 // >4.1mS.
    writeNibble( 0x3 );
    delayMicroseconds( 150 );   // >100uS.
    writeNibble( 0x3 );
    delayMicroseconds( 150 );   // >100uS.
    writeNibble( 0x2);
    delayMicroseconds( 150 );

    // Set actual function mode - cannot be changed after this point
    // without reinitialising.
    writeCommand( FUNCTION_BASE | ( data * FUNCTION_DATA )
                                | ( lines * FUNCTION_LINES )
                                | ( font * FUNCTION_FONT ));
    // Display off.
    writeCommand( DISPLAY_BASE );

    // Set entry mode.
    writeCommand( ENTRY_BASE | ( counter * ENTRY_COUNTER )
                             | ( shift * ENTRY_SHIFT ));

    // Display should be initialised at this point. Function can no longer
    // be changed without re-initialising.

    // Set display properties.
    writeCommand( DISPLAY_BASE | ( display * DISPLAY_ON )
                               | ( cursor * DISPLAY_CURSOR )
                               | ( blink * DISPLAY_BLINK ));

    // Set initial display/cursor movement mode.
    writeCommand( MOVE_BASE | ( mode * MOVE_DISPLAY )
                            | ( direction * MOVE_DIRECTION ));

    // Goto start of DDRAM.
    writeCommand( ADDRESS_DDRAM );

    // Wipe any previous display.
    displayClear();

    return 0;
};


//  Mode settings. ------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Sets entry mode.
//  ---------------------------------------------------------------------------
int8_t setEntryMode( bool counter, bool shift )
{
    writeCommand( ENTRY_BASE | ( counter * ENTRY_COUNTER )
                             | ( shift * ENTRY_SHIFT ));
    // Clear display.
    writeCommand( DISPLAY_CLEAR );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Sets display mode.
//  ---------------------------------------------------------------------------
int8_t setDisplayMode( bool display, bool cursor, bool blink )
{
    writeCommand( DISPLAY_BASE | ( display * DISPLAY_ON )
                               | ( cursor * DISPLAY_CURSOR )
                               | ( blink * DISPLAY_BLINK ));
    // Clear display.
    writeCommand( DISPLAY_CLEAR );

    return 0;
};

//  ---------------------------------------------------------------------------
//  Shifts cursor or display.
//  ---------------------------------------------------------------------------
int8_t setMoveMode( bool mode, bool direction )
{
    writeCommand( MOVE_BASE | ( mode * MOVE_DISPLAY )
                            | ( direction * MOVE_DIRECTION ));
    // Clear display.
    writeCommand( DISPLAY_CLEAR );

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

struct customCharsStruct customChars =
{
    .num = 7,
    .data = {{ 0x00, 0x00, 0x0e, 0x1b, 0x1f, 0x1f, 0x0e, 0x00 },
             { 0x00, 0x00, 0x0f, 0x16, 0x1c, 0x1e, 0x0f, 0x00 },
             { 0x00, 0x0e, 0x19, 0x1d, 0x1f, 0x1f, 0x15, 0x00 },
             { 0x00, 0x0e, 0x13, 0x17, 0x1f, 0x1f, 0x1b, 0x00 },
             { 0x00, 0x0a, 0x1f, 0x1f, 0x1f, 0x0e, 0x04, 0x00 },
             { 0x00, 0x00, 0x0a, 0x0e, 0x0e, 0x04, 0x00, 0x00 },
             { 0x00, 0x00, 0x1e, 0x0d, 0x07, 0x0f, 0x1e, 0x00 },
             { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
};

//  ---------------------------------------------------------------------------
//  Loads custom characters into CGRAM.
//  ---------------------------------------------------------------------------
int8_t loadCustomChar( const uint8_t newChar[CUSTOM_MAX][CUSTOM_SIZE] )
{

    writeCommand( ADDRESS_CGRAM ); // Goto start of CGRAM.

    uint8_t i, j;
    for ( i = 0; i < customChars.num; i++ )
        for ( j = 0; j < CUSTOM_SIZE; j++ )
            writeData( newChar[i][j] );

    writeCommand( ADDRESS_DDRAM ); // Goto start of DDRAM.

    return 0;
};


//  Some display functions. ---------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns a reversed string. Used for rotate string function.
//  ---------------------------------------------------------------------------
static void reverseString( char *text, size_t start, size_t end )
{
    char temp;
    while ( start < end )
    {
        end--;
        temp = text[start];
        text[start] = text[end];
        text[end] = temp;
        start++;
    }

    return;
};

//  ---------------------------------------------------------------------------
//  Returns a rotated string.
//  ---------------------------------------------------------------------------
static void rotateString( char *text, size_t length, size_t increments )
{
    // if (!text || !*text ) return;
    increments %= length;
    reverseString( text, 0, increments );
    reverseString( text, increments, length );
    reverseString( text, 0, length );

    return;
}

//  ---------------------------------------------------------------------------
//  Displays text on display row as a tickertape.
//  ---------------------------------------------------------------------------
void *displayTicker( void *threadTicker )
{
    struct tickerStruct *ticker = threadTicker;

    // Close thread if text string is too big.
    if ( ticker->length + ticker->padding > TEXT_MAX_LENGTH ) pthread_exit( NULL );

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
    char displayText[DISPLAY_COLUMNS];

    while ( 1 )
    {
        // Copy the display text.
        strncpy( displayText, ticker->text, DISPLAY_COLUMNS );

        // Lock thread and display ticker text.
        pthread_mutex_lock( &displayBusy );
        gotoRowPos( 1, 0 );
        writeDataString( displayText );
        pthread_mutex_unlock( &displayBusy );

        // Delay for readability.
        nanosleep( &sleepTime, NULL );

        // Rotate the ticker text.
        rotateString( ticker->text, ticker->length, ticker->increment );
    }

    pthread_exit( NULL );
};

//  ---------------------------------------------------------------------------
//  Displays date/time at row with formatting.
//  ---------------------------------------------------------------------------
void *displayDateTime( void *threadDate )
{
    // Get date struct.
    struct DateAndTime *dateAndTime = threadDate;

    // Definitions for time.h functions.
    struct tm *timePtr;
    time_t timeVar;

    // Definitions for nanosleep function.
    struct timespec sleepTime = { 0 };
    if ( dateAndTime->delay < 1 )
        sleepTime.tv_nsec = dateAndTime->delay * 1000000000;

    // Display string.
    char buffer[20] = "";
    uint8_t frame = 0; // Animation frames

    while ( 1 )
    {
        if ( frame > 1 ) frame = 0;
        // Get current date & time.
        timeVar = time( NULL );
        timePtr = localtime( &timeVar );

        strftime( buffer, dateAndTime->length,
                          dateAndTime->format[frame], timePtr );
        frame++;

        // Display time string.
        pthread_mutex_lock( &displayBusy );
        gotoRowPos( dateAndTime->row, dateAndTime->col );
        writeDataString( buffer );
        pthread_mutex_unlock( &displayBusy );

        // Sleep for designated delay.
        if ( dateAndTime->delay < 1 ) nanosleep( &sleepTime, NULL );
        else sleep( dateAndTime->delay );
    }
    pthread_exit( NULL );
};
