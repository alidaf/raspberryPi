// ****************************************************************************
// ****************************************************************************
/*
    piLCD:

    LCD control app for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
        - based on Python script lcd_16x2.py 2015 by Matt Hawkins.
        - see http://www.raspberrypi-spy.co.uk

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

#define Version "Version 0.1"

//  Compilation:
//
//  Compile with gcc piLCD.c -o piLCD -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    08/11/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

//  To Do:
//      Add routine to check validity of GPIOs.
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

// ----------------------------------------------------------------------------
//  Data structure for command line arguments.
// ----------------------------------------------------------------------------
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
          will likely damage the Pi. Connect to GND!
*/

/*
    HD44780 command codes
        - see https://en.wikipedia.org/wiki/Hitachi_HD44780_LCD_controller

    Key:
    +-----+----------------------+
    | Key | Effect               |
    +-----+----------------------+
    | D/I | Cursor pos L/R       |
    | L/R | Shift display L/R.   |
    | S   | Auto shift off/on.   |
    | DL  | Nibble/byte mode.    |
    | D   | Display off/on.      |
    | N   | 1/2 lines.           |
    | C   | Cursor off/on.       |
    | F   | 5x7/5x10 dots.       |
    | B   | Cursor blink off/on. |
    | C/S | Move cursor/display. |
    | BF  | Busy flag.           |
    +-----+----------------------+

    DDRAM: Display Data RAM.
    CGRAM: Character Generator RAM.

    LCD register bits:
    +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
    | RS  | RW  | D7  | D6  | D5  | D4  | D3  | D2  | D1  | D0  |
    +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
    |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  1  |
    |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  1  |  -  |
    |  0  |  0  |  0  |  0  |  0  |  0  |  0  |  1  | D/I |  S  |
    |  0  |  0  |  0  |  0  |  0  |  0  |  1  |  D  |  C  |  B  |
    |  0  |  0  |  0  |  0  |  0  |  1  | C/S | L/R |  -  |  -  |
    |  0  |  0  |  0  |  0  |  1  | DL  |  N  |  F  |  -  |  -  |
    |  0  |  0  |  0  |  1  |     :   CGRAM address :     :     |
    |  0  |  0  |  1  |     :     : DDRAM address   :     :     |
    |  0  |  1  | BF  |     :     : Address counter :     :     |
    |  1  |  0  |     :     :    Read Data    :     :     :     |
    |  1  |  1  |     :     :    Write Data   :     :     :     |
    +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+

    Example commands:
    +------+--------------------------------+
    | Hex  | Effect                         |
    +------+--------------------------------+
    | 0x01 | Clear LCD                      |
    | 0x0C | Turn on LCD, no cursor         |
    | 0x0E | Solid cursor                   |
    | 0x0F | Blinking cursor                |
    | 0x80 | Move cursor to start of line 1 |
    | 0xC0 | Move cursor to start of line 2 |
    | 0x1C | Shift display right            |
    | 0x18 | Shift display left             |
    | 0x08 | Nibble mode                    |
    | 0x0C | Byte mode                      |
    | 0x28 | Nibble mode, 2 lines, 5x7 font |
    +------+--------------------------------+
*/

// ============================================================================
//  Data structures.
// ============================================================================

/*
    Note: char is the smallest integer size (usually 8 bit) and is used to
          keep the memory footprint as low as possible.
*/

// Constants
#define BYTE_BITS   8       // Number of bits in a byte.
#define NIBBLE_BITS 4       // Number of bits in a nibble.
#define DATA_PINS   4       // Number of LCD data pins being used.

// LCD display properties.
#define LCD_WIDTH   16      // No of LCD display character.
#define LCD_CMD     false   //
#define LCD_CHR     true    //
#define LCD_LINE1   0x80    // Base address for line 1.
#define LCD_LINE2   0xC0    // Base address for line 2.

// ----------------------------------------------------------------------------
// Data structure for logic and switch GPIOs.
// ----------------------------------------------------------------------------
struct pinStruct
{
    unsigned char gpioRS;      // GPIO pin for LCD pin RS (instruction code).
    unsigned char gpioE;       // GPIO pin for LCD pin E (enable signal).
    unsigned char gpioDB[4];   // GPIO pins for LCD pins DB4-DB7 (data 4-bit mode).
};


// ****************************************************************************
//  LCD functions.
// ****************************************************************************

// ----------------------------------------------------------------------------
//  Toggles Enable bit to allow writing.
// ----------------------------------------------------------------------------
char LCDtoggleEnable( struct pinStruct pin )
{
    delay( 5 );
    digitalWrite( pin.gpioE, 1 );
    delay ( 5 );
    digitalWrite( pin.gpioE, 0 );
    delay( 5 );
}


// ----------------------------------------------------------------------------
//  Writes byte values to LCD in nibbles.
// ----------------------------------------------------------------------------
static char *getByteString(unsigned int nibble)
{
    static char binary[BYTE_BITS + 1];
    unsigned int i;
    for ( i = 0; i < BYTE_BITS; i++ )
        binary[i] = (( nibble >> ( BYTE_BITS - i - 1 )) & 1 ) + '0';
    binary[i] = '\0';
    return binary;
};


// ----------------------------------------------------------------------------
//  Writes byte values to LCD in nibbles.
// ----------------------------------------------------------------------------
char LCDwriteByte( struct pinStruct pin, unsigned char data, bool mode )
{
    unsigned char i;
    int nibble[NIBBLE_BITS];

    printf( "Char = %02x, binary = %s. ", data, getByteString( data ));

    digitalWrite( pin.gpioRS, mode );

    // High nibble first.
    for ( i = 0; i < NIBBLE_BITS; i++ )
        nibble[i] = (( data >> ( NIBBLE_BITS - i - 1 )) & 0x10 );

    // Clear gpio data pins.
    for ( i = 0; i < NIBBLE_BITS; i++ )
        digitalWrite( pin.gpioDB[i], 0 );

    // Now write nibble to GPIOs
    for ( i = 0; i < NIBBLE_BITS; i++ )
        digitalWrite( pin.gpioDB[i], nibble[i] );

    // Toggle enable bit to send nibble.
    LCDtoggleEnable( pin );

    printf( "Nibbles = " );
    for ( i = 0; i < NIBBLE_BITS; i++ )
    {
        if ( nibble[i] ) printf( "1" );
        else printf( "0" );
    }
    printf( "," );

    // Low nibble next.
    for ( i = NIBBLE_BITS; i < BYTE_BITS; i++ )
        nibble[i-NIBBLE_BITS] = (( data >> ( BYTE_BITS - i - 1 )) & 0x1 );

    // Clear gpio data pins.
    for ( i = 0; i < NIBBLE_BITS; i++ )
        digitalWrite( pin.gpioDB[i], 0 );

    // Now write nibble to GPIOs
    for ( i = 0; i < NIBBLE_BITS; i++ )
        digitalWrite( pin.gpioDB[i], nibble[i] );

    printf( "Low nibble = " );
    for ( i = 0; i < NIBBLE_BITS; i++ )
    {
        if ( nibble[i] ) printf( "1" );
        else printf( "0" );
    }
    printf( ".\n" );

    // Toggle enable bit to send nibble.
    LCDtoggleEnable( pin );

    return 0;
};


// ----------------------------------------------------------------------------
//  Writes a string to LCD.
// ----------------------------------------------------------------------------
char LCDwriteString( struct pinStruct pin, char *string, char line  )
{
    unsigned int i;

    if (( line < 1 ) || ( line > 2 )) return -1;
    if ( line == 1 ) LCDwriteByte( pin, LCD_LINE1, LCD_CMD );
    else
    if ( line == 1 ) LCDwriteByte( pin, LCD_LINE2, LCD_CMD );

    for ( i = 0; i < strlen( string ); i++ )
        LCDwriteByte( pin, string[i], LCD_CHR );

    return 0;
};


// ----------------------------------------------------------------------------
//  Clears LCD screen.
// ----------------------------------------------------------------------------
char LCDclearScreen( struct pinStruct pin )
{
    LCDwriteByte( pin, 0x01, LCD_CMD );
}


// ----------------------------------------------------------------------------
//  Initialises LCD.
// ----------------------------------------------------------------------------
char LCDinitialise( struct pinStruct pin )
{
    LCDwriteByte( pin, 0x33, LCD_CMD ); // Init.
    LCDwriteByte( pin, 0x32, LCD_CMD ); // Init.
    LCDwriteByte( pin, 0x28, LCD_CMD ); // Nibble mode, 2 lines, 5x7 font.
    LCDwriteByte( pin, 0x08, LCD_CMD ); // Display off, cursor off, blink off.
    LCDwriteByte( pin, 0x01, LCD_CMD ); // Clear display.
    LCDwriteByte( pin, 0x0C, LCD_CMD ); // Display on.
    LCDwriteByte( pin, 0x06, LCD_CMD ); // Entry mode.
    delay( 5 );
    return 0;
};


// ----------------------------------------------------------------------------
//  Initialises GPIOs.
// ----------------------------------------------------------------------------
char wiringPiInit( struct pinStruct pin )
{
    unsigned char i;
    wiringPiSetupGpio();

    // Set LCD pin modes.
    pinMode( pin.gpioRS,  OUTPUT );
    pinMode( pin.gpioE,   OUTPUT );
    // Data pins.
    for ( i = 0; i < DATA_PINS; i++ )
        pinMode( pin.gpioDB[i], OUTPUT );

    // Set pull-up
    pullUpDnControl( pin.gpioRS, PUD_UP );
    pullUpDnControl( pin.gpioE,  PUD_UP );
    // Data pins.
    for ( i = 0; i < DATA_PINS; i++ )
        pullUpDnControl( pin.gpioDB[i], PUD_UP );

    return 0;
}


// ****************************************************************************
//  Command line option functions.
// ****************************************************************************

// ============================================================================
//  argp documentation.
// ============================================================================
const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi LCD control.";
static const char args_doc[] = "piLCD <options>";


// ============================================================================
//  Command line argument definitions.
// ============================================================================
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Switches:" },
    { "rs",     'r', "<int>", 0, "GPIO for RS (instruction code)" },
    { "enable", 'e', "<int>", 0, "GPIO for chip enable" },
    { 0, 0, 0, 0, "Data pins:" },
    { "db4",    'a', "<int>", 0, "GPIO for data bit 4." },
    { "db5",    'b', "<int>", 0, "GPIO for data bit 5." },
    { "db6",    'c', "<int>", 0, "GPIO for data bit 6." },
    { "db7",    'd', "<int>", 0, "GPIO for data bit 7." },
    { 0 }
};


// ============================================================================
//  Command line argument parser.
// ============================================================================
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str, *token;
    const char delimiter[] = ",";
    struct pinStruct *pin = state->input;

    switch ( param )
    {
        case 'r' :
            pin->gpioRS = atoi( arg );
            break;
        case 'e' :
            pin->gpioE = atoi( arg );
            break;
        case 'a' :
            pin->gpioDB[0] = atoi( arg );
            break;
        case 'b' :
            pin->gpioDB[1] = atoi( arg );
            break;
        case 'c' :
            pin->gpioDB[2] = atoi( arg );
            break;
        case 'd' :
            pin->gpioDB[3] = atoi( arg );
            break;
    }
    return 0;
};


// ============================================================================
//  argp parser parameter structure.
// ============================================================================
static struct argp argp = { options, parse_opt, args_doc, doc };


// ============================================================================
//  Main section.
// ============================================================================
char main( int argc, char *argv[] )
{

    unsigned char i;

    // ------------------------------------------------------------------------
    //  Set default values.
    // ------------------------------------------------------------------------
    static struct pinStruct pin =
    {
        .gpioRS    = 7,   // Pin 26.
        .gpioE     = 8,   // Pin 24.
        .gpioDB[0] = 25,  // Pin 22.
        .gpioDB[1] = 24,  // Pin 18.
        .gpioDB[2] = 23,  // Pin 16.
        .gpioDB[3] = 18   // Pin 12.
    };

    // ------------------------------------------------------------------------
    //  Get command line arguments and check within bounds.
    // ------------------------------------------------------------------------
    argp_parse( &argp, argc, argv, 0, 0, &pin );
    // Need to check validity of pins.


    // ------------------------------------------------------------------------
    //  Initialise wiringPi and LCD.
    // ------------------------------------------------------------------------
    wiringPiInit( pin );
    LCDinitialise( pin );

//    LCDwriteString( pin, "Hello Master", 1 );
//    LCDwriteString( pin, "How are you today?", 2 );

    LCDwriteString( pin, "abcdefghijklmnopqrstuvwxyz", 1 );
    LCDwriteString( pin, "0123456789", 2 );

    LCDclearScreen( pin );

    return 0;
}
