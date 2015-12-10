// ****************************************************************************
// ****************************************************************************
/*
    hd44780Pi:

    HD44780 LCD display driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        HD44780 data sheet
        - see https://www.sparkfun.com/datasheets/LCD/HD44780.pdf
        An essential article on LCD initialisation by Donald Weiman.
        - see http://web.alfredstate.edu/weimandn/
        The wiringPi project copyright 2012 Gordon Henderson
        - see http://wiringpi.com

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
*/
// ****************************************************************************
// ****************************************************************************

//  Compilation:
//
//  Compile with gcc -c -fpic libhd44780.c -lwiringPi lpthread
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

#define Version "Version 0.6"

//  Authors:        D.Faulke    10/12/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//  v0.2 Added toggle functions.
//  v0.3 Added custom character function.
//  v0.4 Rewrote init and set mode functions.
//  v0.5 Finally sorted out initialisation!
//  v0.6 Aded ticker tape text function.
//

//  To Do:
//      Add animation functions.
//      Add routine to check validity of GPIOs.
//      Add support for multiple displays.
//      Multithread displays.
//      Add read function to check ready (replace delays?).
//          - most hobbyists may ground the ready pin.
//      Improve error trapping and return codes for all functions.
//      Write GPIO and interrupt routines to replace wiringPi.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "hd44780Pi.h"

// ============================================================================
//  Information.
// ============================================================================
/*
    Pin layout for Hitachi HD44780 based 16x2 LCD display.

    +-----+-------+------+---------------------------------------+
    | Pin | Label | Pi   | Description                           |
    +-----+-------+------+---------------------------------------+
    |   1 |  Vss  | GND  | Ground (0V) for logic.                |
    |   2 |  Vdd  | 5V   | 5V supply for logic.                  |
    |   3 |  Vo   | xV   | Variable V for contrast.              |
    |   4 |  RS   | GPIO | Register Select. 0: command, 1: data. |
    |   5 |  RW   | GND  | R/W. 0: write, 1: read. *Caution*     |
    |   6 |  E    | GPIO | Enable bit.                           |
    |   7 |  DB0  | n/a  | Data bit 0. Not used in 4-bit mode.   |
    |   8 |  DB1  | n/a  | Data bit 1. Not used in 4-bit mode.   |
    |   9 |  DB2  | n/a  | Data bot 2. Not used in 4-bit mode.   |
    |  10 |  DB3  | n/a  | Data bit 3. Not used in 4-bit mode.   |
    |  11 |  DB4  | GPIO | Data bit 4.                           |
    |  12 |  DB5  | GPIO | Data bit 5.                           |
    |  13 |  DB6  | GPIO | Data bit 6.                           |
    |  14 |  DB7  | GPIO | Data bit 7.                           |
    |  15 |  A    | xV   | Voltage for backlight (max 5V).       |
    |  16 |  K    | GND  | Ground (0V) for backlight.            |
    +-----+-------+------+---------------------------------------+

    Note: Setting pin 5 (R/W) to 1 (read) while connected to a GPIO
          will likely damage the Pi unless V is reduced or grounded.

    LCD register bits:
    +---+---+---+---+---+---+---+---+---+---+   +---+---------------+
    |RS |RW |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|   |Key|Effect         |
    +---+---+---+---+---+---+---+---+---+---+   +---+---------------+
    | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |   |I/D|DDRAM inc/dec. |
    | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 | - |   |R/L|Shift R/L.     |
    | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 1 |I/D| S |   |S  |Shift on.      |
    | 0 | 0 | 0 | 0 | 0 | 0 | 1 | D | C | B |   |DL |4-bit/8-bit.   |
    | 0 | 0 | 0 | 0 | 0 | 1 |S/C|R/L| - | - |   |D  |Display on/off.|
    | 0 | 0 | 0 | 0 | 1 |DL | N | F | - | - |   |N  |1/2 lines.     |
    | 0 | 0 | 0 | 1 |   : CGRAM address :   |   |C  |Cursor on/off. |
    | 0 | 0 | 1 |   :   DDRAM address   :   |   |F  |5x8/5x10 font. |
    | 0 | 1 |BF |   :   Address counter :   |   |B  |Blink on/off.  |
    | 1 | 0 |   :   : Read Data :   :   :   |   |S/C|Display/cursor.|
    | 1 | 1 |   :   : Write Data:   :   :   |   |BF |Busy flag.     |
    +---+---+---+---+---+---+---+---+---+---+   +---+---------------+
    DDRAM: Display Data RAM.
    CGRAM: Character Generator RAM.
*/

struct hd44780Struct hd44780 =
// Default values in case no command line parameters are passed.
{
    .cols      = 16,  // Display columns.
    .rows      = 2,   // Display rows.
    .gpioRS    = 7,   // Pin 26 (RS).
    .gpioEN    = 8,   // Pin 24 (E).
    .gpioRW    = 11,  // Pin 23 (RW).
    .gpioDB[0] = 25,  // Pin 12 (DB4).
    .gpioDB[1] = 24,  // Pin 16 (DB5).
    .gpioDB[2] = 23,  // Pin 18 (DB6).
    .gpioDB[3] = 18   // Pin 22 (DB7).
};

// ============================================================================
//  Display output functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Writes byte value of a char to display in nibbles.
// ----------------------------------------------------------------------------
char writeNibble( unsigned char data )
{
    unsigned char i;
    unsigned char nibble;

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

// ----------------------------------------------------------------------------
//  Writes byte value of a command to display in nibbles.
// ----------------------------------------------------------------------------
char writeCommand( unsigned char data )
{
    unsigned char nibble;

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

// ----------------------------------------------------------------------------
//  Writes byte value of data to display in nibbles.
// ----------------------------------------------------------------------------
char writeData( unsigned char data )
{
    unsigned char nibble;

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

// ----------------------------------------------------------------------------
//  Writes a string of data to display.
// ----------------------------------------------------------------------------
char writeDataString( char *string )
{
    unsigned int i;

    for ( i = 0; i < strlen( string ); i++ )
        writeData( string[i] );
    return 0;
};

// ----------------------------------------------------------------------------
//  Moves cursor to row, position.
// ----------------------------------------------------------------------------
/*
    All displays, regardless of size, have the same start address for each
    row due to common architecture. Moving from the end of a line to the start
    of the next is not contiguous memory.
*/
char gotoRowPos( unsigned char row, unsigned char pos )
{
    if (( pos < 0 ) | ( pos > DISPLAY_COLUMNS - 1 )) return -1;
    if (( row < 0 ) | ( row > DISPLAY_ROWS - 1 )) return -1;
    // This doesn't properly check whether the number of display
    // lines has been set to 1.

    // Array of row start addresses
    unsigned char rows[DISPLAY_ROWS_MAX] = { ADDRESS_ROW_0, ADDRESS_ROW_1,
                                             ADDRESS_ROW_2, ADDRESS_ROW_3 };

    writeCommand(( ADDRESS_DDRAM | rows[row] ) + pos );
    return 0;
};

// ============================================================================
//  Display init and mode functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Clears display.
// ----------------------------------------------------------------------------
char displayClear( void )
{
    writeCommand( DISPLAY_CLEAR );
    delayMicroseconds( 1600 ); // Data sheet doesn't give execution time!
    return 0;
};

// ----------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
// ----------------------------------------------------------------------------
char displayHome( void )
{
    writeCommand( DISPLAY_HOME );
    delayMicroseconds( 1600 ); // Needs 1.52ms to execute.
    return 0;
};

// ----------------------------------------------------------------------------
//  Initialises display. Must be called before any other display functions.
// ----------------------------------------------------------------------------
/*
    Currently only supports 4-bit mode initialisation.
    Software initialisation is achieved by setting 8-bit mode and writing a
    sequence of EN toggles with fixed delays between each command.
        Initial delay after Vcc rises to 2.7V
        15mS@5V, 40mS@3V (minimum).
        Set 8-bit mode (command 0x3).
        4.1mS (minimum).
        Set 8-bit mode (command 0x3).
        100uS (minimum).
        Set 8-bit mode (command 0x3).
        100uS (minimum).
        Set function mode. Cannot be changed after this unless re-initialised.
        37uS (minimum).
        Display off.
        Display clear.
        Set entry mode.
*/
char hd44780Init( bool data, bool lines, bool font, bool display,
                  bool cursor, bool blink, bool counter, bool shift,
                  bool mode, bool direction )
/*
    data      = 0: 4-bit mode.
    data      = 1: 8-bit mode.
    lines     = 0: 1 display line.
    lines     = 1: 2 display lines.
    font      = 0: 5x10 font (uses 2 lines).
    font      = 1: 5x8 font.
    counter   = 0: Decrement DDRAM counter after data write (cursor moves L)
    counter   = 1: Increment DDRAM counter after data write (cursor moves R)
    shift     = 0: Do not shift display after data write.
    shift     = 1: Shift display after data write.
    display   = 0: Display off.
    display   = 1: Display on.
    cursor    = 0: Cursor off.
    cursor    = 1: Cursor on.
    blink     = 0: Blink (block cursor) on.
    blink     = 1: Blink (block cursor) off.
    mode      = 0: Shift cursor.
    mode      = 1: Shift display.
    direction = 0: Left.
    direction = 1: Right.
*/
{
    unsigned char i;

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

// ============================================================================
//  Mode settings.
// ============================================================================

// ----------------------------------------------------------------------------
//  Sets entry mode.
// ----------------------------------------------------------------------------
/*
    counter = 0: Decrement DDRAM counter after data write (cursor moves L)
    counter = 1: Increment DDRAM counter after data write (cursor moves R)
    shift =   0: Do not shift display after data write.
    shift =   1: Shift display after data write.
*/
char setEntryMode( bool counter, bool shift )
{
    writeCommand( ENTRY_BASE | ( counter * ENTRY_COUNTER )
                             | ( shift * ENTRY_SHIFT ));
    // Clear display.
    writeCommand( DISPLAY_CLEAR );

    return 0;
};

// ----------------------------------------------------------------------------
//  Sets display mode.
// ----------------------------------------------------------------------------
/*
    display = 0: Display off.
    display = 1: Display on.
    cursor  = 0: Cursor off.
    cursor  = 1: Cursor on.
    blink   = 0: Blink (block cursor) on.
    blink   = 0: Blink (block cursor) off.
*/
char setDisplayMode( bool display, bool cursor, bool blink )
{
    writeCommand( DISPLAY_BASE | ( display * DISPLAY_ON )
                               | ( cursor * DISPLAY_CURSOR )
                               | ( blink * DISPLAY_BLINK ));
    // Clear display.
    writeCommand( DISPLAY_CLEAR );

    return 0;
};

// ----------------------------------------------------------------------------
//  Shifts cursor or display.
// ----------------------------------------------------------------------------
/*
    mode      = 0: Shift cursor.
    mode      = 1: Shift display.
    direction = 0: Left.
    direction = 1: Right.
*/
char setMoveMode( bool mode, bool direction )
{
    writeCommand( MOVE_BASE | ( mode * MOVE_DISPLAY )
                            | ( direction * MOVE_DIRECTION ));
    // Clear display.
    writeCommand( DISPLAY_CLEAR );

    return 0;
};

// ============================================================================
//  Custom characters and animation.
// ============================================================================
/*
    Default characters actually have an extra row at the bottom, reserved
    for the cursor. It is therefore possible to define 8 5x8 characters.
*/

// ----------------------------------------------------------------------------
//  example: Pac Man and pulsing heart.
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
//  Loads custom characters into CGRAM.
// ----------------------------------------------------------------------------
/*
    Set command to point to start of CGRAM and load data line by line. CGRAM
    pointer is auto-incremented. Set command to point to start of DDRAM to
    finish.
*/
char loadCustomChar( const unsigned char newChar[CUSTOM_MAX][CUSTOM_SIZE] )
{
    writeCommand( ADDRESS_CGRAM );
    unsigned char i, j;
    for ( i = 0; i < customChars.num; i++ )
        for ( j = 0; j < CUSTOM_SIZE; j++ )
            writeData( newChar[i][j] );
    writeCommand( ADDRESS_DDRAM );
    return 0;
};

// ============================================================================
//  Some display functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Returns a reversed string.
// ----------------------------------------------------------------------------
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
};

// ----------------------------------------------------------------------------
//  Returns a rotated string.
// ----------------------------------------------------------------------------
static void rotateString( char *text, size_t length, size_t increments )
{
//    if (!text || !*text ) return;
    increments %= length;
    reverseString( text, 0, increments );
    reverseString( text, increments, length );
    reverseString( text, 0, length );
}

// ----------------------------------------------------------------------------
//  Displays text on display row as a tickertape.
// ----------------------------------------------------------------------------
void *displayTicker( void *threadTicker )
/*
    text:      Tickertape text.
    length:    Length of tickertape text.
    padding:   Number of padding spaces at the end of the ticker text.
    row:       Row to display tickertape text.
    increment: Direction and number of characters to rotate.
               +ve value rotates left,
               -ve value rotates right.
    delay:     Controls the speed of the ticker tape. Delay in mS.
*/
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

// ----------------------------------------------------------------------------
//  Displays time at row with justification. Threaded version.
// ----------------------------------------------------------------------------
/*
    Animates a blinking colon between numbers. Best called as a thread.
    Justification = -1: Left justified.
    Justification =  0: Centre justified.
    Justification =  1: Right justified.
*/
void *displayTime( void *threadTime )
{
    struct timeStruct *text = threadTime;

    // Need to set the correct format.
    char timeString[8];

    struct tm *timePtr; // Structure defined in time.h.
    time_t timeVar;     // Type defined in time.h.

    struct timespec sleepTime = { 0 }; // Structure defined in time.h.
    sleepTime.tv_nsec = 500000000;     // 0.5 seconds.

    unsigned char pos;
    if ( text->align == CENTRE ) pos = DISPLAY_COLUMNS / 2 - 4;
    else
    if ( text->align == RIGHT ) pos = DISPLAY_COLUMNS - 8;
    else pos = 0;

    while ( 1 )
    {
        timeVar = time( NULL );
        timePtr = localtime( &timeVar );

        sprintf( timeString, "%02d:%02d:%02d", timePtr->tm_hour,
                                               timePtr->tm_min,
                                               timePtr->tm_sec );
        pthread_mutex_lock( &displayBusy );
        gotoRowPos( text->row, pos );
        writeDataString( timeString );
        pthread_mutex_unlock( &displayBusy );
        nanosleep( &sleepTime, NULL );

        timeVar = time( NULL );
        timePtr = localtime( &timeVar );

        sprintf( timeString, "%02d %02d %02d", timePtr->tm_hour,
                                               timePtr->tm_min,
                                               timePtr->tm_sec );
        pthread_mutex_lock( &displayBusy );
        gotoRowPos( text->row, pos );
        writeDataString( timeString );
        pthread_mutex_unlock( &displayBusy );
        nanosleep( &sleepTime, NULL );
    }
    pthread_exit( NULL );
};
