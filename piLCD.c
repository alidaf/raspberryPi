// ****************************************************************************
// ****************************************************************************
/*
    piLCD:

    HD44780 LCD display driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        HD44780 data sheet
        - unknown source.
        elm-chan.org
        - http://elm-chan.org/docs/lcd/hd44780_e.html
        the Bus Pirate Project.
        - see https://code.google.com/p/the-bus-pirate

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

#define Version "Version 0.4"

//  Compilation:
//
//  Compile with gcc piLCD.c -o piLCD -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    12/11/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//  v0.2 Added toggle functions.
//  v0.3 Added custom character function.
//  v0.4 Rewrote init and set mode functions.
//

//  To Do:
//      Add animation functions.
//      Add routine to check validity of GPIOs.
//      Multithread display lines?
//      Add support for multiple displays.
//      Multithread displays.
//      Convert to a library.
//      Add read function to check ready (replace delays?).
//          - most hobbyists may ground the ready pin.
//      Improve error trapping and return codes for all functions.
//      Write GPIO and interrupt routines to replace wiringPi.
//      Remove all global variables.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

// ============================================================================
//  Information.
// ============================================================================
/*
    Pin layout for Hitachi HD44780 based 16x2 LCD.

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
// ============================================================================
//  Command and constant macros.
// ============================================================================

#define DEBUG 1

// Constants. Change these according to needs.
#define BITS_BYTE          8 // Number of bits in a byte.
#define BITS_NIBBLE        4 // Number of bits in a nibble.
#define PINS_DATA          4 // Number of data pins used.

// These should be replaced by command line options.
#define DISPLAY_COLUMNS   16 // No of LCD display characters.
#define DISPLAY_ROWS       2 // No of LCD display lines.
#define DISPLAY_NUM        1 // Number of displays.

// Modes
#define MODE_COMMAND       0 // Enable command mode for RS pin.
#define MODE_DATA          1 // Enable character mode for RS pin.

// Clear and reset.
#define DISPLAY_CLEAR   0x01 // Clears DDRAM and sets address counter to start.
#define DISPLAY_HOME    0x02 // Sets DDRAM address counter to start.

// Character entry modes.
// ENTRY_BASE = command, decrement DDRAM counter, display shift off.
#define ENTRY_BASE      0x04 // OR this with the options below:
#define ENTRY_COUNTER   0x02 // Increment DDRAM counter (cursor position).
#define ENTRY_SHIFT     0x01 // Display shift on.

// Screen and cursor commands.
// DISPLAY_BASE = command, display off, underline cursor off, block cursor off.
#define DISPLAY_BASE    0x08 // OR this with the options below:
#define DISPLAY_ON      0x04 // Display on.
#define DISPLAY_CURSOR  0x02 // Cursor on.
#define DISPLAY_BLINK   0x01 // Block cursor on.

// Screen and cursor movement.
// MOVE_BASE = command, move cursor left.
#define MOVE_BASE       0x10 // OR this with the options below:
#define MOVE_DISPLAY    0x08 // Move screen.
#define MOVE_DIRECTION  0x04 // Move screen/cursor right.

// LCD function modes.
// FUNCTION_BASE = command, 4-bit mode, 1 line, 5x8 font.
#define FUNCTION_BASE   0x20 // OR this with the options below:
#define FUNCTION_DATA   0x10 // 8-bit (byte) mode.
#define FUNCTION_LINES  0x08 // Use 2 display lines.
#define FUNCTION_FONT   0x04 // 5x10 font.

// LCD character generator and display memory addresses.
#define ADDRESS_CGRAM   0x40 // Character generator start address.
#define ADDRESS_DDRAM   0x80 // Display data start address.
#define ADDRESS_ROWS    0x40 // Row address increment.

// ============================================================================
//  Data structures.
// ============================================================================
/*
    Note: char is the smallest integer size (usually 8 bit) and is used to
          keep the memory footprint as low as possible.
*/

// ----------------------------------------------------------------------------
//  Data structure for GPIOs.
// ----------------------------------------------------------------------------
struct gpioStruct
{
    unsigned char rs;            // GPIO pin for LCD RS pin.
    unsigned char en;            // GPIO pin for LCD Enable pin.
    unsigned char rw;            // GPIO pin for R/W mode. Not used.
    unsigned char db[PINS_DATA]; // GPIO pins for LCD data pins.
} gpio =
{
    .rs    = 7,   // Pin 26 (RS).
    .en    = 8,   // Pin 24 (E).
    .rw    = 11,  // Pin 23 (RW).
    .db[0] = 25,  // Pin 12 (DB4).
    .db[1] = 24,  // Pin 16 (DB5).
    .db[2] = 23,  // Pin 18 (DB6).
    .db[3] = 18   // Pin 22 (DB7).
};

// ============================================================================
//  Some helpful functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Returns binary string for a nibble. Used for debugging only.
// ----------------------------------------------------------------------------
static const char *getBinaryString(unsigned char nibble)
{
    static const char *nibbles[] =
    {
        "0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
        "1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111",
    };
    return nibbles[nibble & 0xff];
};

// ============================================================================
//  LCD output functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Toggles Enable bit to allow writing.
// ----------------------------------------------------------------------------
static char writeEnable( void )
{
    digitalWrite( gpio.en, 1 );
    delayMicroseconds( 41 );    // Setting enable flag takes 41uS.
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 41 );    // Setting enable flag takes 41uS.
};

// ----------------------------------------------------------------------------
//  Writes byte value of a char to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeNibble( unsigned char data )
{
    unsigned char i;
    unsigned char nibble;

    // Write nibble to GPIOs
    nibble = data;
    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        digitalWrite( gpio.db[i], ( nibble & 1 ));
        delayMicroseconds( 41 ); // Writing data to DDRAM takes 41uS.
        nibble >>= 1;
    }
    // Toggle enable bit to send nibble.
    writeEnable();

    return 0;
};

// ----------------------------------------------------------------------------
//  Writes byte value of a command to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeCommand( unsigned char data )
{
    unsigned char nibble;
    unsigned char i;

    // Set to command mode.
    digitalWrite( gpio.rs, 0 );
    delayMicroseconds( 37 ); // setting command mode takes 37uS.

    // High nibble.
    if (DEBUG) printf( "Writing cmd 0x%02x = ", data );
    nibble = ( data >> BITS_NIBBLE ) & 0x0f;
    if (DEBUG) printf( "%s,", getBinaryString( nibble ));
    writeNibble( nibble );

    // Low nibble.
    nibble = data & 0x0f;
    if (DEBUG) printf( "%s.\n", getBinaryString( nibble ));
    writeNibble( nibble );

    return 0;
};

// ----------------------------------------------------------------------------
//  Writes byte value of a command to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeData( unsigned char data )
{
    unsigned char i;
    unsigned char nibble;

    // Set to character mode.
    digitalWrite( gpio.rs, 1 );
    delayMicroseconds( 37 ); // setting command mode takes 37uS.

    // High nibble.
    if (DEBUG) printf( "Writing char %c (0x%0x) = ", data, data );
    nibble = ( data >> BITS_NIBBLE ) & 0xf;
    if (DEBUG) printf( "%s,", getBinaryString( nibble ));
    writeNibble( nibble );

    // Low nibble.
    nibble = data & 0xf;
    if (DEBUG) printf( "%s.\n", getBinaryString( nibble ));
    writeNibble( nibble );

    return 0;
};

// ----------------------------------------------------------------------------
//  Moves cursor to position, row.
// ----------------------------------------------------------------------------
static char gotoRowPos( unsigned char row, unsigned char pos )
{
    if (( pos < 0 ) | ( pos > DISPLAY_COLUMNS - 1 )) return -1;
    if (( row < 0 ) | ( row > DISPLAY_ROWS - 1 )) return -1;

    // Array of row start addresses
    unsigned char rows[DISPLAY_ROWS] = { 0x00, ADDRESS_ROWS };
    unsigned char address = ( ADDRESS_DDRAM | ( rows[row] + pos ));

    if (DEBUG) printf( "Moving cursor to 0x%02x.\n", address );
    writeCommand( address );
    return 0;
};

// ----------------------------------------------------------------------------
//  Writes a string to LCD.
// ----------------------------------------------------------------------------
static char writeDataString( char *string )
{
    unsigned int i;

    for ( i = 0; i < strlen( string ); i++ )
        writeData( string[i] );
    return 0;
};

// ============================================================================
//  LCD init and mode functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Clears LCD screen.
// ----------------------------------------------------------------------------
static char displayClear( void )
{
    if (DEBUG) printf( "Clearing display.\n" );
    writeCommand( DISPLAY_CLEAR );
    delayMicroseconds( 1600 ); // Data sheet doesn't give execution time!
    return 0;
};

// ----------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
// ----------------------------------------------------------------------------
static char displayHome( void )
{
    if (DEBUG) printf( "Resetting display.\n" );
    writeCommand( DISPLAY_HOME );
    delayMicroseconds( 1520 ); // Needs 1.52ms to execute.
    return 0;
};

// ----------------------------------------------------------------------------
//  Initialises LCD. Must be called before any other LCD functions.
// ----------------------------------------------------------------------------
/*
    data  = 0: 4-bit mode.
    data  = 1: 8-bit mode.
    lines = 0: 1 display line.
    lines = 1: 2 display lines.
    font  = 0: 5x10 font (uses 2 lines).
    font  = 1: 5x8 font.

    Software initialisation is achieved by setting 8-bit mode and writing a
    sequence of EN toggles with fixed delays between each command.
        Initial delay after Vcc rises to 2.7V
        15mS@5V, 40mS@3V (minimum).
        Set 8-bit mode (command 0x30).
        Set enable
        4.1mS (minimum).
        Set enable (8-bit mode stays set until changed).
        100uS (minimum).
        Set enable (8-bit mode stays set until changed).
        100uS (minimum).
        Set function mode. Cannot be changed after this unless re-initialised.
        37uS (minimum).
        Display off.
        Display clear.
        Set entry mode.
*/
static char initialiseDisplay( bool data, bool lines, bool font,
                               bool counter, bool shift )
{
    delay( 5 ); // >40mS@3V.

    writeCommand( FUNCTION_BASE | FUNCTION_DATA );
    digitalWrite( gpio.en, 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 4200 );  // >4.1mS.

//    writeCommand( MODE_INIT );
    digitalWrite( gpio.en, 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 150 );   // >=100uS.

//    writeCommand( MODE_INIT );
    digitalWrite( gpio.en, 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 150 );   // >=100uS.

    // Set display mode - cannot be changed after this point
    // without reinitialising.
    if (DEBUG)
    {
        printf( "Setting display mode:\n" );
        if ( data )
             printf( "\t8-bit mode.\n" );
        else printf( "\t4-bit mode.\n" );
        if ( lines )
             printf( "\t2 display lines.\n" );
        else printf( "\t1 display line.\n" );
        if ( font )
             printf( "\t5x10 font.\n" );
        else printf( "\t5x8 font.\n" );
    }
    // delay for write command is handled in function.
    writeCommand( FUNCTION_BASE | ( data * FUNCTION_DATA )
                                | ( lines * FUNCTION_LINES )
                                | ( font * FUNCTION_FONT ));

    if (DEBUG) printf( "Turning display off.\n" );
    writeCommand( DISPLAY_BASE );   // Display off;
    delay( 2000 );
    if (DEBUG) printf( "Clearing display.\n" );
    writeCommand( DISPLAY_CLEAR );  // Clear display;
    delay( 2000 );

    // Set entry mode.
    if (DEBUG)
    {
        printf( "Setting entry mode:\n" );
        if ( counter )
             printf( "\tCursor position increments after data write.\n" );
        else printf( "\tCursor position decrements after data write.\n" );
        if ( shift )
             printf( "\tDisplay shifts after data write.\n" );
        else printf( "\tNo display shift after data write.\n" );
    }
    writeCommand( ENTRY_BASE | ( counter * ENTRY_COUNTER )
                             | ( shift * ENTRY_SHIFT ));
    delay( 2000 );

    // Goto start of DDRAM.
    if (DEBUG) printf( "Setting cursor to start of DDRAM.\n" );
    writeCommand( ADDRESS_DDRAM );
    delay( 2000 );
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
static char setEntryMode( bool counter, bool shift )
{
    if (DEBUG)
    {
        printf( "Setting entry mode:\n" );
        if ( counter )
             printf( "\tCursor position increments after data write.\n" );
        else printf( "\tCursor position decrements after data write.\n" );
        if ( shift )
             printf( "\tDisplay shifts after data write.\n" );
        else printf( "\tNo display shift after data write.\n" );
    }
    writeCommand( ENTRY_BASE | ( counter * ENTRY_COUNTER )
                             | ( shift * ENTRY_SHIFT ));

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
static char setDisplayMode( bool display, bool cursor, bool blink )
{
    if (DEBUG)
    {
        printf( "Setting display and cursor properties:\n" );
        if ( display )
             printf( "\tDisplay on.\n" );
        else printf( "\tDisplay off.\n" );
        if ( cursor )
             printf( "\tCursor on.\n" );
        else printf( "\tCursor off.\n" );
        if ( blink )
             printf( "\tBlink on.\n" );
        else printf( "\tBlink off.\n" );
    }
    writeCommand( DISPLAY_BASE | ( display * DISPLAY_ON )
                               | ( cursor * DISPLAY_CURSOR )
                               | ( blink * DISPLAY_BLINK ));
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
static char setMoveMode( bool mode, bool direction )
{
    if (DEBUG)
        {
            printf( "Setting screen/cursor move mode:\n" );
            if ( mode )
                 printf( "\tShift display " );
            else printf( "\tShift cursor " );
            if ( direction )
                 printf( "right.\n" );
            else printf( "left.\n" );
        }
    writeCommand( MOVE_BASE | ( mode * MOVE_DISPLAY )
                            | ( direction * MOVE_DIRECTION ));
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
//  Pac Man 5x8.
// ----------------------------------------------------------------------------
#define CUSTOM_SIZE  8 // Size of char (rows) for custom chars (5x8).
#define CUSTOM_MAX   8 // Max number of custom chars allowed.
#define CUSTOM_CHARS 7 // Number of custom chars used.
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

    Small dot       Large dot       Pac Man 3
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00
    00000 = 0x00,   00000 = 0x00,   11110 = 0x1e
    00000 = 0x00,   01100 = 0x0c,   01101 = 0x0d
    00100 = 0x04,   01100 = 0x0c,   00111 = 0x07
    00000 = 0x00,   00000 = 0x00,   01111 = 0x0f
    00000 = 0x00,   00000 = 0x00,   11110 = 0x1e
    00000 = 0x00,   00000 = 0x00,   00000 = 0x00
*/
const unsigned char pacMan[CUSTOM_CHARS][CUSTOM_SIZE] =
{
 { 0x00, 0x00, 0x0e, 0x1b, 0x1f, 0x1f, 0x0e, 0x00 },
 { 0x00, 0x00, 0x0f, 0x16, 0x1c, 0x1e, 0x0f, 0x00 },
 { 0x00, 0x0e, 0x19, 0x1d, 0x1f, 0x1f, 0x15, 0x00 },
 { 0x00, 0x0e, 0x13, 0x17, 0x1f, 0x1f, 0x1b, 0x00 },
 { 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x00, 0x00, 0x00 },
 { 0x00, 0x00, 0x1e, 0x0d, 0x07, 0x0f, 0x1e, 0x00 }};

// ----------------------------------------------------------------------------
//  Loads custom character into CGRAM.
// ----------------------------------------------------------------------------
/*
    Set command to point to start of CGRAM and load data line by line. CGRAM
    pointer is auto-incremented. Set command to point to start of DDRAM to
    finish.
*/
static char loadCustom( const unsigned char newChar[CUSTOM_CHARS][CUSTOM_SIZE] )
{
    writeCommand( ADDRESS_CGRAM );
    delayMicroseconds( 37 ); // Write command takes 37uS to execute.
    unsigned char i, j;
    for ( i = 0; i < CUSTOM_CHARS; i++ )
        for ( j = 0; j < CUSTOM_SIZE; j++ )
            writeData( newChar[i][j] );
    writeCommand( ADDRESS_DDRAM );
    return 0;
};

// ----------------------------------------------------------------------------
//  Animates a single char at a fixed position.
// ----------------------------------------------------------------------------
/*
    Move cursor to position and cycle through frames.
*/
static char animateChar( unsigned int row,
                         unsigned int pos,
                         unsigned char frames[],
                         unsigned int numFrames,
                         unsigned int frameDelay,
                         unsigned int numCycles )
{
    static char i, j;
    for ( i = 0; i < numCycles; i++ )
        for ( j = 0; j < numFrames; j++ )
        {
            gotoRowPos( row, pos );
            writeData( frames[j] );
            delay( frameDelay );
        }
    return 0;
};

// ============================================================================
//  GPIO functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Initialises GPIOs.
// ----------------------------------------------------------------------------
static char initialiseGPIOs( void )
{
    unsigned char i;
    wiringPiSetupGpio();

    // Set all GPIO pins to 0.
    digitalWrite( gpio.rs, 0 );
    digitalWrite( gpio.en, 0 );
    // Data pins.
    for ( i = 0; i < PINS_DATA; i++ )
        digitalWrite( gpio.db[i], 0 );

    // Set LCD pin modes.
    pinMode( gpio.rs, OUTPUT );
    pinMode( gpio.en, OUTPUT );
    // Data pins.
    for ( i = 0; i < PINS_DATA; i++ )
        pinMode( gpio.db[i], OUTPUT );

    delay( 35 );
    return 0;
};

// ============================================================================
//  Command line option functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  argp documentation.
// ----------------------------------------------------------------------------
const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi LCD driver.";
static const char args_doc[] = "piLCD <options>";

// ----------------------------------------------------------------------------
//  Command line argument definitions.
// ----------------------------------------------------------------------------
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Switches:" },
    { "rs", 'r', "<int>", 0, "GPIO for RS (instruction code)" },
    { "en", 'e', "<int>", 0, "GPIO for EN (chip enable)" },
    { 0, 0, 0, 0, "Data pins:" },
    { "db4", 'a', "<int>", 0, "GPIO for data bit 4." },
    { "db5", 'b', "<int>", 0, "GPIO for data bit 5." },
    { "db6", 'c', "<int>", 0, "GPIO for data bit 6." },
    { "db7", 'd', "<int>", 0, "GPIO for data bit 7." },
    { 0 }
};

// ----------------------------------------------------------------------------
//  Command line argument parser.
// ----------------------------------------------------------------------------
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str, *token;
    const char delimiter[] = ",";
    struct gpioStruct *gpio = state->input;

    switch ( param )
    {
        case 'r' :
            gpio->rs = atoi( arg );
            break;
        case 'e' :
            gpio->en = atoi( arg );
            break;
        case 'a' :
            gpio->db[0] = atoi( arg );
            break;
        case 'b' :
            gpio->db[1] = atoi( arg );
            break;
        case 'c' :
            gpio->db[2] = atoi( arg );
            break;
        case 'd' :
            gpio->db[3] = atoi( arg );
            break;
    }
    return 0;
};

// ----------------------------------------------------------------------------
//  argp parser parameter structure.
// ----------------------------------------------------------------------------
static struct argp argp = { options, parse_opt, args_doc, doc };

// ============================================================================
//  Main section.
// ============================================================================
char main( int argc, char *argv[] )
{
    // ------------------------------------------------------------------------
    //  Get command line arguments and check within bounds.
    // ------------------------------------------------------------------------
    argp_parse( &argp, argc, argv, 0, 0, &gpio );
    // Need to check validity of pins.

    // ------------------------------------------------------------------------
    //  Initialise wiringPi and LCD.
    // ------------------------------------------------------------------------
    initialiseGPIOs();
    initialiseDisplay( false, true, false, true, false );
//    setEntryMode( true, false );
    setDisplayMode( true, false, false );
//    setMoveMode( false, false );

    loadCustom( pacMan );

//    gotoRowPos( 0, 0 );
//    writeDataString( "Darren Faulke" );
    unsigned int i;
    unsigned int frameDelay = 500;

    unsigned int numAnimations = 3;
    unsigned int numFrames = 2;
    unsigned char animations[3][2] =
    {{ 0, 1 },  // Pac Man right.
     { 2, 3 },  // Ghosts.
     { 0, 6 }}; // Pac Man left.

    animateChar( 0, 0, animations[0], numFrames, 300, 5 );
    displayClear();
    animateChar( 1, 0, animations[2], numFrames, 300, 5 );
    displayClear();

//    for ( i = 0; i < 46; i++ )
//    {
//        gotoRowPos( 1, 0 );
//        writeDataString( animation[i] );
//        delay( frameDelay );
//    }
//    displayClear();
    return 0;
}
