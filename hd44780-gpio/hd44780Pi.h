// ****************************************************************************
// ****************************************************************************
/*
    libhd44780Pi:

    HD44780 LCD display driver library for the Raspberry Pi.

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

//  Authors:        D.Faulke    17/11/2015  This program.
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

// Constants. Change these according to needs.
#define BITS_BYTE          8 // Number of bits in a byte.
#define BITS_NIBBLE        4 // Number of bits in a nibble.
#define PINS_DATA          4 // Number of data pins used.
#define TEXT_MAX_LENGTH  512 // Arbitrary length limit for text string.

// These should be replaced by command line options.
#define DISPLAY_COLUMNS   16 // No of LCD display characters.
#define DISPLAY_ROWS       2 // No of LCD display lines.
#define DISPLAY_NUM        1 // Number of displays.
#define DISPLAY_ROWS_MAX   4 // Max known number of rows for this type of LCD.

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
#define ADDRESS_ROW_0   0x00 // Row 1 start address.
#define ADDRESS_ROW_1   0x40 // Row 2 start address.
#define ADDRESS_ROW_2   0x14 // Row 3 start address.
#define ADDRESS_ROW_3   0x54 // Row 4 start address.

// Constants for GPIO states.
#define GPIO_UNSET         0 // Set GPIO to low.
#define GPIO_SET           1 // Set GPIO to high.

// Constants for display alignment and ticker directions.
enum textAlignment_t { LEFT, CENTRE, RIGHT };

// Enumerated types for date and time displays.
enum timeFormat_t { HMS, HM };
enum dateFormat_t { DAY_DMY, DAY_DM, DAY_D, DMY };

// Define a mutex to allow concurrent display routines.
/*
    Mutex needs to lock before any cursor positioning or write functions.
*/
pthread_mutex_t displayBusy;

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
struct hd44780Struct
{
    unsigned char id;                // Handle for multiple displays.
    unsigned char cols;              // Number of display columns (x).
    unsigned char rows;              // Number of display rows (y).
    unsigned char gpioRS;            // GPIO pin for LCD RS pin.
    unsigned char gpioEN;            // GPIO pin for LCD Enable pin.
    unsigned char gpioRW;            // GPIO pin for R/W mode. Not used.
    unsigned char gpioDB[PINS_DATA]; // GPIO pins for LCD data pins.
} hd44780 =
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

// ----------------------------------------------------------------------------
//  Data structures for displaying text.
// ----------------------------------------------------------------------------
/*
    .align  = TEXT_ALIGN_NULL   : No set alignment (just print at cursor ).
            = TEXT_ALIGN_LEFT   : Text aligns or rotates left.
            = TEXT_ALIGN_CENTRE : Text aligns or oscillates about centre.
            = TEXT_ALIGN_RIGHT  : Text aligns or rotates right.
*/
struct textStruct
{
    char string[DISPLAY_COLUMNS];
    unsigned char row;
    enum textAlignment_t align;
};

struct timeStruct
{
    unsigned char row;
    unsigned int delay;
    enum textAlignment_t align;
    enum timeFormat_t format;
};

struct dateStruct
{
    unsigned char row;
    unsigned int delay;
    enum textAlignment_t align;
    enum dateFormat_t format;
};

/*
    .increment = Number and direction of characters to rotate.
                 +ve rotate left.
                 -ve rotate right.
    .length + .padding must be < TEXT_MAX_LENGTH.
*/
struct tickerStruct
{
    char text[TEXT_MAX_LENGTH];
    unsigned int length;
    unsigned int padding;
    unsigned char row;
    unsigned int  increment;
    unsigned int  delay;
};

// ============================================================================
//  Display output functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Writes byte value of a char to display in nibbles.
// ----------------------------------------------------------------------------
char writeNibble( unsigned char data );

// ----------------------------------------------------------------------------
//  Writes byte value of a command to display in nibbles.
// ----------------------------------------------------------------------------
char writeCommand( unsigned char data );

// ----------------------------------------------------------------------------
//  Writes byte value of data to display in nibbles.
// ----------------------------------------------------------------------------
char writeData( unsigned char data );

// ----------------------------------------------------------------------------
//  Writes a string of data to display.
// ----------------------------------------------------------------------------
char writeDataString( char *string );

// ----------------------------------------------------------------------------
//  Moves cursor to row, position.
// ----------------------------------------------------------------------------
/*
    All displays, regardless of size, have the same start address for each
    row due to common architecture. Moving from the end of a line to the start
    of the next is not contiguous memory.
*/
char gotoRowPos( unsigned char row, unsigned char pos );

// ============================================================================
//  Display init and mode functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Clears display.
// ----------------------------------------------------------------------------
char displayClear( void );

// ----------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
// ----------------------------------------------------------------------------
char displayHome( void );

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
                  bool mode, bool direction );
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
char setEntryMode( bool counter, bool shift );

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
char setDisplayMode( bool display, bool cursor, bool blink );

// ----------------------------------------------------------------------------
//  Shifts cursor or display.
// ----------------------------------------------------------------------------
/*
    mode      = 0: Shift cursor.
    mode      = 1: Shift display.
    direction = 0: Left.
    direction = 1: Right.
*/
char setMoveMode( bool mode, bool direction );

// ============================================================================
//  Custom characters and animation.
// ============================================================================
/*
    Default characters actually have an extra row at the bottom, reserved
    for the cursor. It is therefore possible to define 8 5x8 characters.
*/

#define CUSTOM_SIZE  8 // Size of char (rows) for custom chars (5x8).
#define CUSTOM_MAX   8 // Max number of custom chars allowed.
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
struct customCharsStruct
{
    unsigned char num; // Number of custom chars (max 8).
    unsigned char data[CUSTOM_MAX][CUSTOM_SIZE];

} customChars =
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
#define CUSTOM_SIZE  8 // Size of char (rows) for custom chars (5x8).
#define CUSTOM_MAX   8 // Max number of custom chars allowed.
char loadCustom( const unsigned char newChar[CUSTOM_MAX][CUSTOM_SIZE] );

// ============================================================================
//  Some display functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Returns a reversed string.
// ----------------------------------------------------------------------------
static void reverseString( char *text, size_t start, size_t end );

// ----------------------------------------------------------------------------
//  Returns a rotated string.
// ----------------------------------------------------------------------------
static void rotateString( char *text, size_t length, size_t increments );

// ----------------------------------------------------------------------------
//  Displays text on display row as a tickertape.
// ----------------------------------------------------------------------------
void *displayTicker( void *threadTicker );
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

// ----------------------------------------------------------------------------
//  Displays time at row with justification. Threaded version.
// ----------------------------------------------------------------------------
/*
    Animates a blinking colon between numbers. Best called as a thread.
    Justification = -1: Left justified.
    Justification =  0: Centre justified.
    Justification =  1: Right justified.
*/
void *displayTime( void *threadTime );
