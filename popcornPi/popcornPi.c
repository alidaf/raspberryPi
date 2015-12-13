// ****************************************************************************
// ****************************************************************************
/*
    piCorn:

    A driver for the Popcorn C200 display, which is an Orient Display AMG19264
    LCD connected via a MAX7325 port expander and incorporating a TI LM27966
    backlight control. I2C slave address is either 0x50 or 0x60 according to
    the MAX7325 data sheet but i2cdetect gives 0x5d and 0x6d.
    Slave address of the LM27966 is 0x36 but this is not detected by i2cdetect.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
    Based on the following guides and codes:
        AMG19264C data sheet,
        LM27966 data sheet.

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
//  Compile with gcc piCorn.c -o piCorn -lwiringPi
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    02/12/2015  This program.
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
#include <wiringPiI2C.h>
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
    |  20 |  BLA  | +V   | Voltage for backlight (max 5V).       |
    |  21 |  BLK  | GND  | Ground (0V) for backlight.            |
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

    ---------------------------------------------------------------------------

    LM27966 information:

    Chip address = 0x36.

    +------+------+------+------+------+------+------+------+
    | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
    +------+------+------+------+------+------+------+------+
    | ADR6 | ADR5 | ADR4 | ADR3 | ADR2 | ADR1 | ADR0 | R/W  |
    +------+------+------+------+------+------+------+------+

    Internal registers:

    General purpose register address = 0x10.

    +--------+--------+--------+--------+--------+--------+--------+--------+
    |  bit7  |  bit6  |  bit5  |  bit4  |  bit3  |  bit2  |  bit1  |  bit0  |
    +--------+--------+--------+--------+--------+--------+--------+--------+
    |    0   |    0   |    1   |   T1   | EN-D5  | EN-AUX |   T0   |EN-MAIN |
    +--------+--------+--------+--------+--------+--------+--------+--------+

    EN-MAIN : Enables Dx LED drivers (main display).
    T0      : Must be set to 0.
    EN-AUX  : Enables Daux LED driver (indicator lighting).
    EN-D5   : Enables D5 LED voltage sense.
    T1      : Must be set to 0.

    Main display brightness control register address = 0xa0.

    +------+------+------+------+------+------+------+------+
    | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
    +------+------+------+------+------+------+------+------+
    |   1  |   1  |   1  | Dx4  | Dx3  | Dx2  | Dx1  | Dx0  |
    +------+------+------+------+------+------+------+------+

    Aux LED brightness control register address = 0xc0.

    +------+------+------+------+------+------+------+------+
    | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
    +------+------+------+------+------+------+------+------+
    |   1  |   1  |   1  |   1  |   1  |   1  |DAUX1 |DAUX0 |
    +------+------+------+------+------+------+------+------+

    Dx4 - Dx0   : Sets brightness for Dx pins (main display).
    DAUX1-DAUX0 : Sets brightness for Daux pin.

    Brightness code = 0x00 - 0x1f (1.25% - 100% brightness) for main display.
    Brightness code = 0x0 - 0x3 (20%, 40%, 70%, 100% brightness).

    ---------------------------------------------------------------------------



*/
// ============================================================================
//  Command and constant macros.
// ============================================================================

// I2C addresses.
#define MAX7325_ADDR1  0x5d // I2C address for port expander.
#define MAX7325_ADDR2  0x6d // I2C address for port expander.
#define LM27966_ADDR   0x36 // I2C address for backlight control.

// These should be replaced by command line options.
#define DISPLAY_PMAX      8 // Max No of LCD pages.
#define DISPLAY_XMAX     64 // Max No of LCD display rows.
#define DISPLAY_YMAX     64 // Max No of LCD display columns.
#define DISPLAY_NUM       1 // Number of displays.

// Display commands.
#define DISPLAY_ON     0x3f // Turn display on.
#define DISPLAY_OFF    0x3e // Turn display off.

// LCD addresses.
#define BASE_PADDR     0xb8 // Base address for pages 0-7.
#define BASE_XADDR     0x40 // Base address for X position.
#define BASE_YADDR     0xc0 // Base address for Y position.

// Constants for GPIO states.
#define GPIO_UNSET        0 // Set GPIO to low.
#define GPIO_SET          1 // Set GPIO to high.

// Define a mutex to allow concurrent display routines.
/*
    Mutex needs to lock before any cursor positioning or write functions.
*/
pthread_mutex_t displayBusy;

// ============================================================================
//  Data structures.
// ============================================================================


// ============================================================================
//  Main section.
// ============================================================================
void main( void )
{

    // ------------------------------------------------------------------------
    //  Initialise I2C.
    // ------------------------------------------------------------------------
    int displayHandle1 = wiringPiI2CSetup( MAX7325_ADDR1 );
    int displayHandle2 = wiringPiI2CSetup( MAX7325_ADDR2 );
    int backlightHandle = wiringPiI2CSetup( LM27966_ADDR );

    printf( "Display handles are %d & %d.\n", displayHandle1, displayHandle2 );
    printf( "Backlight handle is %d.\n", backlightHandle );

}
