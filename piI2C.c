// ****************************************************************************
// ****************************************************************************
/*
    piI2C:

    Orient Display AMG19264 LCD display driver for the Raspberry Pi via a
    MAX7325 port expander. I2C slave address is either 0x50 or 0x60 according
    to the MAX7325 data sheet.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        AMG19264C data sheet

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
//  Compile with gcc piI2C.c -o piI2C -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    24/11/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

//  To Do:
//      Get it to work with the MAX7325 I2C expander.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// ============================================================================
//  Information.
// ============================================================================
/*
    Pin layout for Orient Display AMG19264 based 192x64 LCD.

    +-----+-------+------+---------------------------------------+
    | Pin | Label | Pi   | Description                           |
    +-----+-------+------+---------------------------------------+
    |   1 |  Vss  | GND  | Ground (0V) for logic.                |
    |   2 |  Vdd  | 5V   | 5V supply for logic.                  |
    |   3 |  Vo   | +V   | Variable V for contrast.              |
    |   4 |  Vee  | -V   | Voltage output.                       |
    |   5 |  RS   | GPIO | Register Select. 0: command, 1: data. |
    |   6 |  RW   | GND  | R/W. 0: write, 1: read. *Caution*     |
    |   7 |  E    | GPIO | Enable bit.                           |
    |   8 |  DB0  | GPIO | Data bit 0.                           |
    |   9 |  DB1  | GPIO | Data bit 1.                           |
    |  10 |  DB2  | GPIO | Data bit 2.                           |
    |  11 |  DB3  | GPIO | Data bit 3.                           |
    |  12 |  DB4  | GPIO | Data bit 4.                           |
    |  13 |  DB5  | GPIO | Data bit 5.                           |
    |  14 |  DB6  | GPIO | Data bit 6.                           |
    |  15 |  DB7  | GPIO | Data bit 7.                           |
    |  16 |  CS3  | GPIO | Chip select (left).                   |
    |  17 |  CS2  | GPIO | Chip select (middle).                 |
    |  18 |  CS1  | GPIO | Chip select (right).                  |
    |  19 |  RST  |      | Reset signal.                         |
    |  16 |  BLA  | +V   | Voltage for backlight (max 5V).       |
    |  16 |  BLK  | GND  | Ground (0V) for backlight.            |
    +-----+-------+------+---------------------------------------+

    Note: Most displays are combinations of up to 3 64x64 modules, each
          controlled via the CSx (chip select) registers.
          Pages may refer to virtual screens.
    Note: Setting pin 6 (R/W) to 1 (read) while connected to a GPIO
          will likely damage the Pi unless V is reduced or grounded.

    LCD register bits:
    +---+---+---+---+---+---+---+---+---+---+   +---+---------------+
    |RS |RW |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|   |Key|Effect         |
    +---+---+---+---+---+---+---+---+---+---+   +---+---------------+
    | 0 | 0 | 0 | 0 | 1 | 1 | 1 | 1 | 1 | D |   | D |Display on/off.|
    | 0 | 0 | 0 | 1 |   : Y address (0-63)  |   | B |Busy status.   |
    | 0 | 0 | 1 | 0 | 1 | 1 | 1 |Page (0-7) |   | D |Display status.|
    | 0 | 0 | 1 | 1 |     X address (0-63)  |   | R |Reset status.  |
    | 0 | 1 | B | 1 | D | R | 0 | 0 | 0 | 0 |   +---+---------------+
    | 1 | 0 |   :   : Write Data:   :   :   |
    | 1 | 1 |   :   : Read Data :   :   :   |
    +---+---+---+---+---+---+---+---+---+---+
*/
// ============================================================================
//  Command and constant macros.
// ============================================================================

// Constants. Change these according to needs.
#define PINS_DATA      8 // Number of data pins used.

// These should be replaced by command line options.
#define DISPLAY_PMAX   8 // Max No of LCD pages.
#define DISPLAY_XMAX  64 // Max No of LCD display rows.
#define DISPLAY_YMAX  64 // Max No of LCD display columns.
#define DISPLAY_NUM    1 // Number of displays.

// Display commands.
#define DISPLAY_ON  0x3f // Turn display on.
#define DISPLAY_OFF 0x3e // Turn display off.

// LCD addresses.
#define BASE_PADDR  0xb8 // Base address for pages 0-7.
#define BASE_XADDR  0x40 // Base address for X position.
#define BASE_YADDR  0xc0 // Base address for Y position.

// Constants for GPIO states.
#define GPIO_UNSET     0 // Set GPIO to low.
#define GPIO_SET       1 // Set GPIO to high.

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
struct gpioStruct
{
    unsigned char rs;            // GPIO pin for LCD RS register.
    unsigned char en;            // GPIO pin for LCD Enable register.
    unsigned char rw;            // GPIO pin for R/W mode. Not used.
    unsigned char cs1;           // GPIO pin for Chip Select 1 register.
    unsigned char cs2;           // GPIO pin for Chip Select 2 register.
    unsigned char cs3;           // GPIO pin for Chip Select 3 register.
    unsigned char db[PINS_DATA]; // GPIO pins for LCD data registers.
} gpio =
// Default gpio numbers in case no command line parameters are passed.
{
    .rs    = 14,
    .en    = 15,
//    .rw    = xx,
//    .cs1   = xx,  // Right hand screen.
    .cs2   = 10, // Centre screen.
    .cs3   =  9, // Left hand screen.
    .db[0] = 18,
    .db[1] = 23,
    .db[2] = 24,
    .db[3] = 25,
    .db[4] =  4,
    .db[5] = 17,
    .db[6] = 27,
    .db[7] = 22
};

// ============================================================================
//  Display functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Sends command to display registers.
// ----------------------------------------------------------------------------
static void writeCommand( unsigned char cs, unsigned char command )
{
    unsigned char i;

    if ( cs == 0 )      // Left hand screen.
    {
        digitalWrite( gpio.cs3, GPIO_SET );
        digitalWrite( gpio.cs2, GPIO_UNSET );
//        digitalWrite( gpio.cs1, GPIO_UNSET );
    }
    else if ( cs == 1 ) // Centre screen.
    {
        digitalWrite( gpio.cs3, GPIO_UNSET );
        digitalWrite( gpio.cs2, GPIO_SET );
//        digitalWrite( gpio.cs1, GPIO_UNSET );
    }
    else if ( cs == 2 ) // Rigt hand screen.
    {
        digitalWrite( gpio.cs3, GPIO_UNSET );
        digitalWrite( gpio.cs2, GPIO_UNSET );
//        digitalWrite( gpio.cs1, GPIO_SET );
    }

    // Set RS and RW registers.
    digitalWrite( gpio.rs, GPIO_UNSET );
//    digitalWrite( gpio.rw, GPIO_UNSET );

    // Load data into DB0-DB7 registers.
    for ( i = 0; i < PINS_DATA; i++ )
        digitalWrite( gpio.db[i], ( command << i ) & 1 );
    delayMicroseconds( 10 );

    // Toggle EN register to send data.
    digitalWrite( gpio.en, GPIO_SET );
    delayMicroseconds( 5 );
    digitalWrite( gpio.en, GPIO_UNSET );
    delayMicroseconds( 10 );

    digitalWrite( gpio.cs3, GPIO_SET );
    digitalWrite( gpio.cs2, GPIO_SET );
};

// ----------------------------------------------------------------------------
//  Sends data to display registers.
// ----------------------------------------------------------------------------
static void writeData( unsigned char cs, unsigned char data )
{
    unsigned char i;

    if ( cs == 0 )      // Left hand screen.
    {
        digitalWrite( gpio.cs3, GPIO_SET );
        digitalWrite( gpio.cs2, GPIO_UNSET );
//        digitalWrite( gpio.cs1, GPIO_UNSET );
    }
    else if ( cs == 1 ) // Centre screen.
    {
        digitalWrite( gpio.cs3, GPIO_UNSET );
        digitalWrite( gpio.cs2, GPIO_SET );
//        digitalWrite( gpio.cs1, GPIO_UNSET );
    }
    else if ( cs == 2 ) // Rigt hand screen.
    {
        digitalWrite( gpio.cs3, GPIO_UNSET );
        digitalWrite( gpio.cs2, GPIO_UNSET );
//        digitalWrite( gpio.cs1, GPIO_SET );
    }

    // Set RS and RW registers.
    digitalWrite( gpio.rs, GPIO_SET );
//    digitalWrite( gpio.rw, GPIO_UNSET );

    // Load data into DB0-DB7 registers.
    for ( i = 0; i < PINS_DATA; i++ )
        digitalWrite( gpio.db[i], ( data << i ) & 1 );
    delayMicroseconds( 10 );

    // Toggle EN register to send data.
    digitalWrite( gpio.en, GPIO_SET );
    delayMicroseconds( 5 );
    digitalWrite( gpio.en, GPIO_UNSET );
    delayMicroseconds( 10 );

    digitalWrite( gpio.cs3, GPIO_SET );
    digitalWrite( gpio.cs2, GPIO_SET );
};

// ----------------------------------------------------------------------------
//  Draws a dot.
// ----------------------------------------------------------------------------
static void drawDot( unsigned char x, unsigned char y )
{
    writeCommand( x / 64, BASE_XADDR | ( x % 64 ));
    writeCommand( x / 64, BASE_PADDR | ( y / 8 ));
    writeData( x / 64, 1 << ( y % 8 ));
};

// ----------------------------------------------------------------------------
//  Initialises display.
// ----------------------------------------------------------------------------
static void initDisplay( void )
{
    writeCommand( 0, BASE_YADDR );
    writeCommand( 1, BASE_YADDR );
    writeCommand( 2, BASE_YADDR );

    writeCommand( 0, DISPLAY_ON );
    writeCommand( 1, DISPLAY_ON );
    writeCommand( 2, DISPLAY_ON );
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
    digitalWrite( gpio.rs, GPIO_UNSET );
    digitalWrite( gpio.en, GPIO_UNSET );
    // Data pins.
    for ( i = 0; i < PINS_DATA; i++ )
        digitalWrite( gpio.db[i], GPIO_UNSET );

    // Set LCD pin modes.
    pinMode( gpio.rs, OUTPUT ); // Enumerated type defined in wiringPi.
    pinMode( gpio.en, OUTPUT ); // Enumerated type defined in wiringPi.
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
    { "rs", 'r', "<int>", 0, "GPIO for RS (instruction code) register" },
    { "en", 'n', "<int>", 0, "GPIO for EN (chip enable) register" },
    { "rw", 'w', "<int>", 0, "GPIO for R/W (read/write) register" },
    { 0, 0, 0, 0, "Data pins:" },
    { "db0", 'a', "<int>", 0, "GPIO for data register bit 0." },
    { "db1", 'b', "<int>", 0, "GPIO for data register bit 1." },
    { "db2", 'c', "<int>", 0, "GPIO for data register bit 2." },
    { "db3", 'd', "<int>", 0, "GPIO for data register bit 3." },
    { "db4", 'e', "<int>", 0, "GPIO for data register bit 4." },
    { "db5", 'f', "<int>", 0, "GPIO for data register bit 5." },
    { "db6", 'g', "<int>", 0, "GPIO for data register bit 6." },
    { "db7", 'h', "<int>", 0, "GPIO for data register bit 7." },
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
        case 'n' :
            gpio->en = atoi( arg );
            break;
        case 'w' :
            gpio->rw = atoi( arg );
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
        case 'e' :
            gpio->db[4] = atoi( arg );
            break;
        case 'f' :
            gpio->db[5] = atoi( arg );
            break;
        case 'g' :
            gpio->db[6] = atoi( arg );
            break;
        case 'h' :
            gpio->db[7] = atoi( arg );
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
    initialiseGPIOs();  // Must be called before initialiseDisplay.

    // Create threads and mutex for animated display functions.
//    pthread_mutex_init( &displayBusy, NULL );
//    pthread_t threads[2];
//    pthread_create( &threads[0], NULL, displayTime, (void *) &textTime );
//    pthread_create( &threads[1], NULL, displayPacMan, (void *) pacManRow );
//    pthread_create( &threads[1], NULL, displayTicker, (void *) &ticker );

    initDisplay();

    writeCommand( 0, DISPLAY_OFF );
    writeCommand( 1, DISPLAY_OFF );

    writeCommand( 0, DISPLAY_ON );
    writeCommand( 1, DISPLAY_ON );

    // Display on.

    unsigned char i, j, k;
    for ( i = 0; i < DISPLAY_PMAX; i++ ) // Pages
    {
        writeCommand( 0, BASE_PADDR + i );
        for ( j = 0; j < DISPLAY_YMAX; j++ ) // Y coordinate
        {
            writeCommand( 0, BASE_YADDR + j );
            for ( k = 0; k < DISPLAY_XMAX; k++ ) // X coordinate
            {
                writeCommand( 0, BASE_YADDR + k );
                printf( "Writing at %02i,%02i,%02i.\n", i, j, k );
                writeData( 0, 0x01 );
                writeData( 1, 0x01 );
            };
        };
    };
    // Clean up threads.
//    pthread_mutex_destroy( &displayBusy );
//    pthread_exit( NULL );

}
