// ****************************************************************************
// ****************************************************************************
/*
    piLCD:

    LCD control app for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        the Bus Pirate Project.
        - see https://code.google.com/p/the-bus-pirate
        Pratyush's blog
        - see http://electronicswork.blogspot.in
        wiringPi (lcd.c) Copyright 2012 Gordon Henderson
        - see https://github.com/WiringPi

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

#define Version "Version 0.2"

//  Compilation:
//
//  Compile with gcc piLCD.c -o piLCD -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    11/11/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//  v0.2 Added toggle functions.
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
*/
// ============================================================================
//  Useful LCD commands and constants.
// ============================================================================

#define DEBUG 1

// Constants
#define BITS_BYTE    8 // Number of bits in a byte.
#define BITS_NIBBLE  4 // Number of bits in a nibble.
#define PINS_DATA    4 // Number of LCD data pins being used.
#define LCD_COLS    16 // No of LCD display characters.
#define LCD_ROWS     2 // No of LCD display lines.

// Modes
#define MODE_CMD     0 // Enable command mode for RS pin.
#define MODE_CHAR    1 // Enable character mode for RS pin.

// Clear and reset.
#define MODE_CLR  0x01 // Clear LCD screen.
#define MODE_HOME 0x02 // Screen and cursor home.

// Character entry modes.
#define MODE_ENTR 0x04 // OR this with the options below:
#define ENTR_INCR 0x02 // Cursor increment. Default is decrement,
#define ENTR_SHFT 0x01 // Auto shift. Default is off.

// Screen and cursor commands.
#define MODE_DISP 0x08 // OR this with the options below:
#define DISP_ON   0x04 // Display on. Default is off.
#define CURS_ON   0x02 // Cursor on. Default is off.
#define BLNK_ON   0x01 // Blink on. Default is off.

// Screen and cursor movement.
#define MODE_MOVE 0x10 // OR this with the options below:
#define MOVE_DISP 0x08 // Move screen. Default is cursor.
#define MOVE_RGHT 0x04 // Move screen/cursor right. Default is left.

// LCD function modes.
#define MODE_LCD  0x20 // OR this with the options below:
#define LCD_DATA  0x10 // 8 bit (byte) mode. Default is 4 bit (nibble) mode.
#define LCD_LINE  0x08 // Use 2 display lines. Default is 1 line.
#define LCD_FONT  0x04 // 5x10 font. Default is 5x7 font.

// LCD character generator and display addresses.
#define CHAR_ADDR 0x40 // Character generator start address.
#define DISP_ADDR 0x80 // Display data start address.

#define ROW1_ADDR 0x00 // Start address of LCD row 1.
#define ROW2_ADDR 0x40 // Start address of LCD row 2.

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

// ----------------------------------------------------------------------------
//  Data structure of toggle switches for various LCD functions.
// ----------------------------------------------------------------------------
struct modeStruct
{
    // MODE_DISP
    unsigned char display   :1; // 0 = display off, 1 = display on.
    unsigned char cursor    :1; // 0 = cursor off, 1 = cursor on.
    unsigned char blink     :1; // 0 = blink off, 1 = blink on.
    // MODE_LCD
    unsigned char data      :1; // 0 = nibble mode, 1 = byte mode.
    unsigned char lines     :1; // 0 = 1 line, 1 = 2 lines.
    unsigned char font      :1; // 0 = 5x7, 1 = 5x10.
    // MODE_MOVE
    unsigned char movedisp  :1; // 0 = move cursor, 1 = move screen.
    unsigned char direction :1; // 0 = left, 1 = right.
    //MODE_ENTR
    unsigned char increment :1; // 0 = decrement, 1 = increment.
    unsigned char shift     :1; // 0 = auto shift off, 1 = auto shift on.
    unsigned char           :6; // Padding to make structure 16 bits.
} mode =
{
    .display   = 1,
    .cursor    = 1,
    .blink     = 0,
    .data      = 0,
    .lines     = 1,
    .font      = 1,
    .movedisp  = 0,
    .direction = 1,
    .increment = 1,
    .shift     = 0
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
static char toggleEnable( void )
{
    digitalWrite( gpio.en, 1 );
    delayMicroseconds( 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 41 );
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
        nibble >>= 1;
    }
    // Toggle enable bit to send nibble.
    toggleEnable();

    return 0;
};

// ----------------------------------------------------------------------------
//  Writes byte value of a command to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeCmd( unsigned char data )
{
    unsigned char nibble;
    unsigned char i;

    // Set to command mode.
    digitalWrite( gpio.rs, 0 );
    digitalWrite( gpio.en, 0 );

    // High nibble.
    if (DEBUG) printf( "Writing cmd 0x%02x = ", data );
    nibble = ( data >> BITS_NIBBLE ) & 0x0F;
    if (DEBUG) printf( "%s,", getBinaryString( nibble ));
    writeNibble( nibble );

    // Low nibble.
    nibble = data & 0x0F;
    if (DEBUG) printf( "%s.\n", getBinaryString( nibble ));
    writeNibble( nibble );

    return 0;
};

// ----------------------------------------------------------------------------
//  Writes byte value of a command to LCD in nibbles.
// ----------------------------------------------------------------------------
static char writeChar( unsigned char data )
{
    unsigned char i;
    unsigned char nibble;

    // Set to character mode.
    digitalWrite( gpio.rs, 1 );
    digitalWrite( gpio.en, 0 );

    // High nibble.
    if (DEBUG) printf( "Writing char %c (0x%0x) = ", data, data );
    nibble = ( data >> BITS_NIBBLE ) & 0xF;
    if (DEBUG) printf( "%s,", getBinaryString( nibble ));
    writeNibble( nibble );

    // Low nibble.
    nibble = data & 0xF;
    if (DEBUG) printf( "%s.\n", getBinaryString( nibble ));
    writeNibble( nibble );

    return 0;
};

// ----------------------------------------------------------------------------
//  Moves cursor to position, row.
// ----------------------------------------------------------------------------
static char gotoRowPos( unsigned char row, unsigned char pos )
{
    if (( pos < 0 ) | ( pos > LCD_COLS - 1 )) return -1;
    if (( row < 0 ) | ( row > LCD_ROWS - 1 )) return -1;

    static unsigned char address[LCD_ROWS] = { ROW1_ADDR, ROW2_ADDR };
    writeCmd(( DISP_ADDR | address[row] ) + pos );
    return 0;
};

// ----------------------------------------------------------------------------
//  Writes a string to LCD.
// ----------------------------------------------------------------------------
static char writeString( char *string )
{
    unsigned int i;

    for ( i = 0; i < strlen( string ); i++ )
        writeChar( string[i] );

    return 0;
};

// ============================================================================
//  LCD init and mode functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Clears LCD screen.
// ----------------------------------------------------------------------------
static char clearDisplay( void )
{
    if (DEBUG) printf( "Clearing display.\n" );
    writeCmd( MODE_CLR );
    delayMicroseconds( 1600 ); // Needs 1.6ms to clear.
    return 0;
};

// ----------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
// ----------------------------------------------------------------------------
static char resetDisplay( void )
{
    if (DEBUG) printf( "Resetting display.\n" );
    writeCmd( MODE_HOME );
    delayMicroseconds( 1600 ); // Needs 1.6ms to clear
    return 0;
};

// ----------------------------------------------------------------------------
//  sets LCD modes.
// ----------------------------------------------------------------------------
static char setMode( void )
{

    if (DEBUG)
    {
        printf( "Setting display mode:\n" );
        if ( mode.data )
             printf( "\t8-bit mode.\n" );
        else printf( "\t4-bit mode.\n" );
        if ( mode.lines )
             printf( "\t2 display lines with a " );
        else printf( "\t1 display line with a " );
        if ( mode.font )
             printf( "5x10 font.\n" );
        else printf( "5x7 font.\n" );
    }
    writeCmd( MODE_LCD | ( mode.data * LCD_DATA )
                       | ( mode.lines * LCD_LINE )
                       | ( mode.font * LCD_FONT ));

    if (DEBUG)
    {
        printf( "Setting display and cursor properties:\n" );
        if ( mode.display )
             printf( "\tDisplay is on.\n" );
        else printf( "\tDisplay is off.\n" );
        if ( mode.cursor )
             printf( "\tCursor is on and " );
        else printf( "\tCursor is off and " );
        if ( mode.blink )
             printf( "blinking.\n" );
        else printf( "not blinking.\n" );
    }
    writeCmd( MODE_DISP | ( mode.display * DISP_ON )
                        | ( mode.cursor * CURS_ON )
                        | ( mode.blink * BLNK_ON ));

    if (DEBUG) printf( "Clearing display.\n" );
    clearDisplay();

    if (DEBUG)
    {
        printf( "Setting entry mode:\n" );
        if ( mode.increment )
             printf( "\tCursor movement is +ve with " );
        else printf( "\tCursor movement is -ve with " );
        if ( mode.shift )
             printf( "auto shift.\n" );
        else printf( "no auto shift.\n" );
    }
    writeCmd( MODE_ENTR | ( mode.increment * ENTR_INCR )
                        | ( mode.shift * ENTR_SHFT ));

    if (DEBUG)
        {
            printf( "Setting screen/cursor move mode:\n" );
            if ( mode.movedisp )
                 printf( "\tShift display " );
            else printf( "\tShift cursor " );
            if ( mode.direction )
                 printf( "right.\n" );
            else printf( "left.\n" );
        }
    writeCmd( MODE_MOVE | ( mode.movedisp * MOVE_DISP )
                        | ( mode.direction * MOVE_RGHT ));

    if (DEBUG) printf( "Setting Address.\n" );
    writeCmd( DISP_ADDR | ROW1_ADDR );

    return 0;
};

// ----------------------------------------------------------------------------
//  Initialises LCD. Must be called before any other LCD functions.
// ----------------------------------------------------------------------------
static char initLCD( void )
{
/*
    LCD has to be initialised by setting 8-bit mode and writing a sequence
    of EN toggles with fixed delays between each command.
        ?ms for start-up?
        send command for 8-bit mode (0x30).
        toggle EN.
        4.1ms.
        toggle EN.
        100 us.
        toggle EN.
        100 us.
*/
    delay( 5 );
    if (DEBUG) printf( "Initialising LCD.\n" );

    writeCmd( MODE_LCD | LCD_DATA );
    digitalWrite( gpio.en, 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 4200 );  // 4.1ms + 100us margin.

//    writeCmd( MODE_INIT );
    digitalWrite( gpio.en, 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 150 );   // 100us + 50us margin.

//    writeCmd( MODE_INIT );
    digitalWrite( gpio.en, 1 );
    digitalWrite( gpio.en, 0 );
    delayMicroseconds( 150 );   // 100us + 50us margin.

    setMode();
    return 0;
};

// ============================================================================
//  Toggle functions. setMode needs to be called to stick the toggles.
// ============================================================================

// ----------------------------------------------------------------------------
//  Toggles display on/off.
// ----------------------------------------------------------------------------
static char toggleDisplay( void )
{
    mode.display = !mode.display;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles cursor on/off.
// ----------------------------------------------------------------------------
static char toggleCursor( void )
{
    mode.cursor = !mode.cursor;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles cursor blink on/off.
// ----------------------------------------------------------------------------
static char toggleBlink( void )
{
    mode.blink = !mode.blink;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles font 5x7/5x10.
// ----------------------------------------------------------------------------
static char toggleFont( void )
{
    mode.font = !mode.font;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles display/cursor movement.
// ----------------------------------------------------------------------------
static char toggleMove( void )
{
    mode.movedisp = !mode.movedisp;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles direction of screen/display left/right.
// ----------------------------------------------------------------------------
static char toggleDirection( void )
{
    mode.direction = !mode.direction;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles increment/decrement.
// ----------------------------------------------------------------------------
static char toggleIncrement( void )
{
    mode.increment = !mode.increment;
    return 0;
};

// ----------------------------------------------------------------------------
//  Toggles display auto shift on/off.
// ----------------------------------------------------------------------------
static char toggleShift( void )
{
    mode.shift = !mode.shift;
    return 0;
};

// ============================================================================
//  GPIO functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Initialises GPIOs.
// ----------------------------------------------------------------------------
static char initGPIO( void )
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
static const char doc[] = "Raspberry Pi LCD control.";
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
    initGPIO();
    initLCD();
    setMode();

    printf( "Initialisation finished.\n" );
    printf( "Press a key.\n" );
    getchar();

    while (1)
    {
        gotoRowPos( 0, 0 );
        writeString( "abcdefghijklmnop" );
        gotoRowPos( 1, 0 );
        writeString( "ABCDEFGHIJKLMNOP" );

        printf( "Press a key.\n" );
        getchar();
        clearDisplay();
        resetDisplay();

        gotoRowPos( 0, 0 );
        writeString( "***0123456789***" );
        gotoRowPos( 1, 0 );
        writeString( "-Darren Faulke-" );

        printf( "Press a key.\n" );
        getchar();
        clearDisplay();
        resetDisplay();
    }

    return 0;
}
