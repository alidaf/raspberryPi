// ****************************************************************************
// ****************************************************************************
/*
    piLCDexp:

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
        MCP23017 direct access from simple-on-off.c
        - see https://github.com/elegantandrogyne/mcp23017-demo/blob/master/
          simple-on-off.c
       Unified LCD driver by Dougie Lawson.
       - see https://github.com/DougieLawson/RaspberryPi/blob/master/
         Unified_LCD/i2cLcd.c

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
//  Compile with gcc piLCDexp.c -o piLCDexp -lwiringPi lpthread
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    08/12/2015  This program.
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//  v0.2 Finished converting to MCP23017 commands.
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
#include <time.h>
#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include <fcntl.h>
#include <unistd.h>

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
    |   9 |  DB2  | n/a  | Data bit 2. Not used in 4-bit mode.   |
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

    The MCP23017 is an I2C bus operated 16-bit I/O port expander.
    The pin layout and connections used are shown below:

                    +------+-----+-----+------+
                    |  Fn  | pin | pin |  Fn  |
                    +------+-----+-----+------+
       LCD    RS <--| GPB0 |  01 | 28  | GPA7 |
       LCD   R/W <--| GPB1 |  02 | 27  | GPA6 |
       LCD    EN <--| GPB2 |  03 | 26  | GPA5 |
                    | GPB3 |  04 | 25  | GPA4 |
       LCD   DB4 <--| GPB4 |  05 | 24  | GPA3 |
       LCD   DB5 <--| GPB5 |  06 | 23  | GPA2 |
       LCD   DB6 <--| GPB6 |  07 | 22  | GPA1 |
       LCD   DB7 <--| GPB7 |  08 | 21  | GPA0 |
  +5V <-:-----------|  VDD |  09 | 20  | INTA |
   0V <-:-0.1uF-||--|  VSS |  10 | 19  | INTB |
                    |   NC |  11 | 18  | RST  |-----> +5V
        Pi  SCL1 <--|  SCL |  12 | 17  | A2   |----->  0V or +5V
        Pi  SDA1 <--|  SDA |  13 | 16  | A1   |----->  0V or +5V
                    |   NC |  14 | 15  | A0   |----->  0V or +5V
                    +------+-----+-----+------+

    Notes:  There is a 0.1uF ceramic capaictor across pins 09 and 10
            for stability.

            SCL1 and SDA1 are connected to the pi via a logic level shifter
            to protect the Pi from +5V reads at the pins since I2C is
            bidirectional, however, the Pi has resistors to 'pull' the voltage
            to +3.3V on these pins so the shifter can probably be dropped.

            The RST (hardware reset) pin is kept high for normal operation.

            The I2C address device is addressed as follows:

                +-----+-----+-----+-----+-----+-----+-----+-----+
                |  0  |  1  |  0  |  0  |  A2 |  A1 |  A0 | R/W |
                +-----+-----+-----+-----+-----+-----+-----+-----+
                : <----------- slave address -----------> :     :
                : <--------------- control byte --------------> :

                R/W = 0: write.
                R/W = 1: read.

            The address reported by i2cdetect should be 0x20 to 0x27
            depending on whether pins A0 to A2 are wired high or low.

    Reading/writing to the MCP23017:

        The MCP23017 has two 8-bit ports (PORTA & PORTB) that can operate in
        8-bit or 16-bit modes.

        The MCP23017 has 2 sets (PORTS) of 8 GPIOs; GPA0-GPA7 and GPB0-GPB7.
        They can be written to or read as byte values to address all GPIOs
        in a PORT simultaneously or sequentially.

        The device address is sent, followed by the register address.

        MCP23017 register addresses:

            +-------+-------+----------+-------------------------------+
            | BANK1 | BANK0 | Register | Description                   |
            +-------+-------+----------+-------------------------------+
            |  0x00 |  0x00 | IODIRA   | IO direction (port A).        |
            |  0x10 |  0x01 | IODIRB   | IO direction (port B).        |
            |  0x01 |  0x02 | IPOLA    | Polarity (port A).            |
            |  0x11 |  0x03 | IPOLB    | Polarity (port B).            |
            |  0x02 |  0x04 | GPINTENA | Interrupt on change (port A). |
            |  0x12 |  0x05 | GPINTENB | Interrupt on change (port B). |
            |  0x03 |  0x06 | DEFVALA  | Default compare (port A).     |
            |  0x13 |  0x07 | DEFVALB  | Default compare (port B).     |
            |  0x04 |  0x08 | INTCONA  | Interrupt control (port A).   |
            |  0x14 |  0x09 | INTCONB  | Interrupt control (port B).   |
            |  0x05 |  0x0a | IOCON    | Configuration.                |
            |  0x15 |  0x0b | IOCON    | Configuration.                |
            |  0x06 |  0x0c | GPPUA    | Pull-up resistors (port A).   |
            |  0x16 |  0x0d | GPPUB    | Pull-up resistors (port B).   |
            |  0x07 |  0x0e | INTFA    | Interrupt flag (port A).      |
            |  0x17 |  0x0f | INTFB    | Interrupt flag (port B).      |
            |  0x08 |  0x10 | INTCAPA  | Interrupt capture (port A).   |
            |  0x18 |  0x11 | INTCAPB  | Interrupt capture (port B).   |
            |  0x09 |  0x12 | GPIOA    | GPIO ports (port A).          |
            |  0x19 |  0x13 | GPIOB    | GPIO ports (port B).          |
            |  0x0a |  0x14 | OLATA    | Output latches (port A).      |
            |  0x1a |  0x15 | OLATB    | Output latches (port B).      |
            +-------+-------+----------+-------------------------------+

        The IOCON register bits set various configurations including the BANK
        bit (bit 7).
        If BANK = 1, the ports are segregated, i.e. 8-bit mode with PORTA
        mapped from 0x00 to 0x0a and PORTB mapped from 0x10 to 0x1a.
        If BANK = 0, the ports are paired into 16-bit mode and the registers
        are mapped from 0x00 - 0x15.

        The pull-up resistors are 100kOhm.

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

// ----------------------------------------------------------------------------

// MCP23017 specific.
#define MCP23017_CHIPS         1 // Number of MCP20317 expander chips (max 8).

// I2C base addresses, set according to A0-A2. Maximum of 8.
#define MCP23017_ADDRESS_0  0x20
#define MCP23017_ADDRESS_1  0x21 // Dummy address for additional chip.
#define MCP23017_ADDRESS_2  0x22 // Dummy address for additional chip.
#define MCP23017_ADDRESS_3  0x23 // Dummy address for additional chip.
#define MCP23017_ADDRESS_4  0x24 // Dummy address for additional chip.
#define MCP23017_ADDRESS_5  0x25 // Dummy address for additional chip.
#define MCP23017_ADDRESS_6  0x26 // Dummy address for additional chip.
#define MCP23017_ADDRESS_7  0x27 // Dummy address for additional chip.

// MCP23017 register addresses (BANK0).
#define MCP23017_IODIRA     0x00
#define MCP23017_IODIRB     0x01
#define MCP23017_IPOLA      0x02
#define MCP23017_IPOLB      0x03
#define MCP23017_GPINTENA   0x04
#define MCP23017_GPINTENB   0x05
#define MCP23017_DEFVALA    0x06
#define MCP23017_DEFVALB    0x07
#define MCP23017_INTCONA    0x08
#define MCP23017_INTCONB    0x09
#define MCP23017_IOCONA     0x0a
#define MCP23017_IOCONB     0x0b
#define MCP23017_GPPUA      0x0c
#define MCP23017_GPPUB      0x0d
#define MCP23017_INTFA      0x0e
#define MCP23017_INTFB      0x0f
#define MCP23017_INTCAPA    0x10
#define MCP23017_INTCAPB    0x11
#define MCP23017_GPIOA      0x12
#define MCP23017_GPIOB      0x13
#define MCP23017_OLATA      0x14
#define MCP23017_OLATB      0x15

static const char *i2cDevice = "/dev/i2c-1";  // Path to I2C file system.
unsigned char ioBuffer[2];                    // MCP23017 write buffer.
unsigned char mcp23017Handle[MCP23017_CHIPS]; // Handles for each chip.
unsigned char mcp23017Address[MCP23017_CHIPS] = { MCP23017_ADDRESS_0 };

// ----------------------------------------------------------------------------

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
//  Data structure for MCP23017 pins.
// ----------------------------------------------------------------------------
struct lcdStruct
{
    unsigned char rs;    // LCD RS pin.
    unsigned char en;    // LCD Enable pin.
    unsigned char rw;    // R/W mode pin.
    unsigned char db[4]; // LCD data pins.
} lcdPin =
// Default values in case no command line parameters are passed.
{
    .rs    = 0x80, // MCP23017 GPB0 -> LCD RS.
    .en    = 0x40, // MCP23017 GPB1 -> LCD E.
    .rw    = 0x20, // MCP23017 GPB2 -> LCD RW.
                   // MCP23017 GPB3 -> not used.
    .db[0] = 0x08, // MCP23017 GPB4 -> LCD DB4.
    .db[1] = 0x04, // MCP23017 GPB5 -> LCD DB5.
    .db[2] = 0x02, // MCP23017 GPB6 -> LCD DB6.
    .db[3] = 0x01  // MCP23017 GPB7 -> LCD DB7.
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
//  Some helpful functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Returns binary string for a byte. Used for debugging only.
// ----------------------------------------------------------------------------
static char *getByteString( unsigned char byte )
{
    unsigned char data = byte;
    static char binary[BITS_BYTE + 1];
    unsigned int i;
    for ( i = 0; i < BITS_BYTE; i++ )
        binary[i] = (( data >> ( BITS_BYTE - i - 1 )) & 1 ) + '0';
    binary[i] = '\0';
    return binary;
};

// ----------------------------------------------------------------------------
//  Returns binary string for a nibble. Used for debugging only.
// ----------------------------------------------------------------------------
static char *getNibbleString( unsigned char nibble )
{
    unsigned char data = nibble;
    static char binary[BITS_NIBBLE + 1];
    unsigned int i;
    for ( i = 0; i < BITS_NIBBLE; i++ )
        binary[i] = (( data >> ( BITS_NIBBLE - i - 1 )) & 1 ) + '0';
    binary[i] = '\0';
    return binary;
};

// ============================================================================
//  MCP23017 functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
// ----------------------------------------------------------------------------
static char mcp23017WriteByte( unsigned char mcp23017Handle,
                               unsigned char mcp23017Register,
                               unsigned char byte )
{
    unsigned char i;
    unsigned char data = byte;

    printf( "Handle = %d, Register = 0x%02x.\n", mcp23017Handle, mcp23017Register );
    printf( "RS EN RW NC DB DB DB DB\n" );
    for ( i = 0; i < BITS_BYTE; i++ )
        printf( "%2d ", ( data >> BITS_BYTE - i - 1 ) & 1 );
    printf( "\n" );
    i2c_smbus_write_byte_data( mcp23017Handle, mcp23017Register, byte );
}

// ----------------------------------------------------------------------------
//  Initialises MCP23017 chips.
// ----------------------------------------------------------------------------
static char mcp23017init( void )
{

    unsigned char i;

    for ( i = 0; i < MCP23017_CHIPS; i++ )
	{
        // I2C communication is via device file (/dev/i2c-1).
        if (( mcp23017Handle[i] = open( i2cDevice, O_RDWR )) < 0 );
        {
            printf( "Couldn't open I2C device %s.\n", i2cDevice );
            return -1;
        }
        // Set slave address for each MCP23017 device.
        if ( ioctl( mcp23017Handle[i], I2C_SLAVE, mcp23017Address[i] ) < 0 );
        {
            printf( "Couldn't set slave address 0x%02x.\n",
                     mcp23017Address[i] );
            return -2;
        }
        // Set directions to out (PORTA).
        mcp23017WriteByte( mcp23017Handle[i], MCP23017_IODIRA, 0x00 );
        // Set directions to out (PORTB).
        mcp23017WriteByte( mcp23017Handle[i], MCP23017_IODIRB, 0x00 );
        // Set all outputs to low (PORTA).
        mcp23017WriteByte( mcp23017Handle[i], MCP23017_OLATA, 0x00 );
        // Set all outputs to low (PORTB).
        mcp23017WriteByte( mcp23017Handle[i], MCP23017_OLATB, 0x00 );
	}

    return 0;
};

// ============================================================================
//  HD44780 LCD functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Writes command byte to HD44780 via MCP23017.
// ----------------------------------------------------------------------------
static char hd44780WriteCommand( unsigned char handle,
                                 unsigned char reg,
                                 unsigned char data )
{
    unsigned char i;
    unsigned char commandByte;
    unsigned char highNibble, lowNibble;
    char *highString, *lowString;

    // High nibble.
    highNibble = ( data >> BITS_NIBBLE ) & 0xf;
    highString = getNibbleString( highNibble );

    // Low nibble.
    lowNibble = data & 0xf;
    lowString = getNibbleString( lowNibble );

    printf( "Command byte = 0x%02x.\n", data );

    // High nibble first.
    // Create a byte containing the address of the MCP23017 pins to be changed.
    commandByte = 0; // Make sure rs pin is unset for command.
/*
    for ( i = 0; i < BITS_NIBBLE; i++ )
        binary[i] = (( data >> ( BITS_NIBBLE - i - 1 )) & 1 ) + '0';
*/
    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        commandByte |= ( highNibble >> (( BITS_NIBBLE - i - 1 ) & 1 ) * lcdPin.db[i]);
//        highNibble >>= BITS_NIBBLE - i - 1;
    }

    // Write byte to MCP23017 via output latch.
    mcp23017WriteByte( handle, reg, commandByte );

    // Toggle enable bit to send nibble via output latch.
    mcp23017WriteByte( handle, reg, commandByte || lcdPin.en );
    usleep( 41 );
    mcp23017WriteByte( handle, reg, commandByte );
    usleep( 41 );

    // Low nibble next.
    // Create a byte containing the address of the MCP23017 pins to be changed.
    commandByte = 0; // Make sure rs pin is unset for command.

    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        commandByte |= ( lowNibble >> (( BITS_NIBBLE - i - 1 ) & 1 ) * lcdPin.db[i]);
//        commandByte |= ( lowNibble & 1 ) * lcdPin.db[i];
//        lowNibble >>= BITS_NIBBLE - i - 1;
    }

    // Write byte to MCP23017 via output latch.
    mcp23017WriteByte( handle, reg, commandByte );

    // Toggle enable bit to send nibble via output latch.
    printf( "Toggling EN bit.\n" );
    mcp23017WriteByte( handle, reg, commandByte || lcdPin.en );
    usleep( 41 );
    mcp23017WriteByte( handle, reg, commandByte );
    usleep( 41 );

    return 0;
};

// ----------------------------------------------------------------------------
//  Writes data byte to HD44780 via MCP23017.
// ----------------------------------------------------------------------------
static char hd44780WriteData( unsigned char handle,
                              unsigned char reg,
                              unsigned char data )
{
    unsigned char i;
    unsigned char dataByte;
    unsigned char highNibble, lowNibble;
    char *highString, *lowString;

    // High nibble.
    highNibble = ( data >> BITS_NIBBLE ) & 0xf;
    highString = getNibbleString( highNibble );

    // Low nibble.
    lowNibble = data & 0xf;
    lowString = getNibbleString( lowNibble );

    printf( "Data byte = 0x%02x.\n", data );

    // High nibble first.
    // Create a byte containing the address of the MCP23017 pins to be changed.
    dataByte = lcdPin.rs; // Make sure rs pin is set for data.

    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        dataByte |= ( highNibble & 1 ) * lcdPin.db[i];
        highNibble >>= BITS_NIBBLE - i - 1;
    }

    // Write byte to MCP23017.
    mcp23017WriteByte( handle, reg, dataByte );

    // Toggle enable bit.
    mcp23017WriteByte( handle, reg, dataByte || lcdPin.en );
    usleep( 41000 );
    mcp23017WriteByte( handle, reg, dataByte );
    usleep( 41000 );

    // Low nibble next.
    // Create a byte containing the address of the MCP23017 pins to be changed.
    dataByte = lcdPin.rs; // Make sure rs pin is set for data.

    for ( i = 0; i < BITS_NIBBLE; i++ )
    {
        dataByte |= ( lowNibble & 1 ) * lcdPin.db[i];
        lowNibble >>= BITS_NIBBLE - i - 1;
    }

    // Write byte to MCP23017.
    mcp23017WriteByte( handle, reg, dataByte );

    // Toggle enable bit.
    printf( "Toggling EN bit.\n" );
    mcp23017WriteByte( handle, reg, dataByte || lcdPin.en );
    usleep( 41 );
    mcp23017WriteByte( handle, reg, dataByte );
    usleep( 41 );

    return 0;
};

// ----------------------------------------------------------------------------
//  Writes a data string to LCD.
// ----------------------------------------------------------------------------
static char hd44780WriteDataString( unsigned char handle,
                                    unsigned char reg,
                                    char *string )
{
    unsigned int i;

    // Sends string to LCD byte by byte.
    for ( i = 0; i < strlen( string ); i++ )
        hd44780WriteData( handle, reg, string[i] );
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
static char hd44780Goto( unsigned char handle,
                         unsigned char reg,
                         unsigned char row,
                         unsigned char pos )
{
    if (( pos < 0 ) | ( pos > DISPLAY_COLUMNS - 1 )) return -1;
    if (( row < 0 ) | ( row > DISPLAY_ROWS - 1 )) return -1;
    // This doesn't properly check whether the number of display
    // lines has been set to 1.

    // Array of row start addresses
    unsigned char rows[DISPLAY_ROWS_MAX] = { ADDRESS_ROW_0, ADDRESS_ROW_1,
                                             ADDRESS_ROW_2, ADDRESS_ROW_3 };

    hd44780WriteCommand( handle, reg, ( ADDRESS_DDRAM | rows[row] ) + pos );
    return 0;
};

// ============================================================================
//  LCD init and mode functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Clears LCD screen.
// ----------------------------------------------------------------------------
static char displayClear( unsigned char handle, unsigned char reg )
{
    hd44780WriteCommand( handle, reg, DISPLAY_CLEAR );
    usleep( 1600 ); // Data sheet doesn't give execution time!
    return 0;
};

// ----------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
// ----------------------------------------------------------------------------
static char displayHome( unsigned char handle, unsigned char reg )
{
    hd44780WriteCommand( handle, reg, DISPLAY_HOME );
    usleep( 1600 ); // Needs 1.52ms to execute.
    return 0;
};

// ----------------------------------------------------------------------------
//  Initialises LCD. Must be called before any other LCD functions.
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
static char initialiseDisplay( unsigned char handle,
                               unsigned char reg,
                               bool data, bool lines, bool font,
                               bool display, bool cursor, bool blink,
                               bool counter, bool shift,
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
    // Allow a start-up delay.
    usleep( 50000 ); // >40mS@3V.

    // Need to write low nibbles only as display starts off in 8-bit mode.
    // Sending high nibble first (0x0) causes init to fail and the display
    // subsequently shows garbage.
    printf( "Initialising.\n" );
    mcp23017WriteByte( handle, reg, 0x03 );
    usleep( 4200 );  // >4.1mS.
    mcp23017WriteByte( handle, reg, 0x03 );
    usleep( 150 );   // >100uS.
    mcp23017WriteByte( handle, reg, 0x03 );
    usleep( 150 );   // >100uS.
    mcp23017WriteByte( handle, reg, 0x02);
    usleep( 150 );

    // Set actual function mode - cannot be changed after this point
    // without reinitialising.
    printf( "Setting LCD functions.\n" );
    hd44780WriteCommand( handle, reg,
                         FUNCTION_BASE | ( data * FUNCTION_DATA )
                                       | ( lines * FUNCTION_LINES )
                                       | ( font * FUNCTION_FONT ));
    // Display off.
    hd44780WriteCommand( handle, reg, DISPLAY_BASE );

    // Set entry mode.
    hd44780WriteCommand( handle, reg,
                         ENTRY_BASE | ( counter * ENTRY_COUNTER )
                                    | ( shift * ENTRY_SHIFT ));

    // Display should be initialised at this point. Function can no longer
    // be changed without re-initialising.

    // Set display properties.
    hd44780WriteCommand( handle, reg,
                         DISPLAY_BASE | ( display * DISPLAY_ON )
                                      | ( cursor * DISPLAY_CURSOR )
                                      | ( blink * DISPLAY_BLINK ));

    // Set initial display/cursor movement mode.
    hd44780WriteCommand( handle, reg,
                         MOVE_BASE | ( mode * MOVE_DISPLAY )
                                   | ( direction * MOVE_DIRECTION ));

    // Goto start of DDRAM.
    hd44780WriteCommand( handle, reg, ADDRESS_DDRAM );

    // Wipe any previous display.
    displayClear( handle, reg );

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
static char setEntryMode( unsigned char handle, unsigned char reg,
                          bool counter, bool shift )
{
    hd44780WriteCommand( handle, reg,
                  ENTRY_BASE | ( counter * ENTRY_COUNTER )
                             | ( shift * ENTRY_SHIFT ));
    // Clear display.
    hd44780WriteCommand( handle, reg, DISPLAY_CLEAR );

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
static char setDisplayMode( unsigned char handle, unsigned char reg,
                            bool display, bool cursor, bool blink )
{
    hd44780WriteCommand( handle, reg,
                         DISPLAY_BASE | ( display * DISPLAY_ON )
                                      | ( cursor * DISPLAY_CURSOR )
                                      | ( blink * DISPLAY_BLINK ));
    // Clear display.
    hd44780WriteCommand( handle, reg, DISPLAY_CLEAR );

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
static char setMoveMode( unsigned char handle, unsigned char reg,
                         bool mode, bool direction )
{
    hd44780WriteCommand( handle, reg,
                         MOVE_BASE | ( mode * MOVE_DISPLAY )
                                   | ( direction * MOVE_DIRECTION ));
    // Clear display.
    hd44780WriteCommand( handle, reg, DISPLAY_CLEAR );

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
const unsigned char pacMan[CUSTOM_CHARS][CUSTOM_SIZE] =
{
 { 0x00, 0x00, 0x0e, 0x1b, 0x1f, 0x1f, 0x0e, 0x00 },
 { 0x00, 0x00, 0x0f, 0x16, 0x1c, 0x1e, 0x0f, 0x00 },
 { 0x00, 0x0e, 0x19, 0x1d, 0x1f, 0x1f, 0x15, 0x00 },
 { 0x00, 0x0e, 0x13, 0x17, 0x1f, 0x1f, 0x1b, 0x00 },
 { 0x00, 0x0a, 0x1f, 0x1f, 0x1f, 0x0e, 0x04, 0x00 },
 { 0x00, 0x00, 0x0a, 0x0e, 0x0e, 0x04, 0x00, 0x00 },
 { 0x00, 0x00, 0x1e, 0x0d, 0x07, 0x0f, 0x1e, 0x00 }};

// ----------------------------------------------------------------------------
//  Loads custom characters into CGRAM.
// ----------------------------------------------------------------------------
/*
    Set command to point to start of CGRAM and load data line by line. CGRAM
    pointer is auto-incremented. Set command to point to start of DDRAM to
    finish.
*/
static char loadCustom( unsigned char handle, unsigned char reg,
                        const unsigned char newChar[CUSTOM_CHARS][CUSTOM_SIZE] )
{
    hd44780WriteCommand( handle, reg, ADDRESS_CGRAM );
    unsigned char i, j;
    for ( i = 0; i < CUSTOM_CHARS; i++ )
        for ( j = 0; j < CUSTOM_SIZE; j++ )
            hd44780WriteData( handle, reg, newChar[i][j] );
    hd44780WriteCommand( handle, reg, ADDRESS_DDRAM );
    return 0;
};

// ----------------------------------------------------------------------------
//  Simple animation demonstration using threads.
// ----------------------------------------------------------------------------
/*
    Parameters passed as textStruct, cast to void.
*/
static void *displayPacMan( void *rowPtr )
{
    unsigned int numFrames = 2;
    unsigned char pacManLeft[2] = { 0, 6 };     // Animation frames.
    unsigned char pacManRight[2] = { 1, 0 };    // Animation frames.
    unsigned char ghost[2] = { 2, 3 };          // Animation frames.
    unsigned char heart[2] = { 4, 5 };          // Animation frames.

    // Needs to be an int as size of pointers are 4 bytes.
    unsigned int row = (unsigned int)rowPtr;

    // Variables for nanosleep function.
    struct timespec sleepTime = { 0 };  // Structure defined in time.h.
    sleepTime.tv_sec = 0;
    sleepTime.tv_nsec = 300000000;

    // Load custom characters into CGRAM.
    pthread_mutex_lock( &displayBusy );
    loadCustom( mcp23017Handle[0], MCP23017_OLATB, pacMan );
    pthread_mutex_unlock( &displayBusy );

    unsigned char i, j;     // i = position counter, j = frame counter.

    while ( 1 )
    {
        for ( i = 0; i < 16; i++ )
        {
            for ( j = 0; j < numFrames; j++ )
            {
                pthread_mutex_lock( &displayBusy );
                hd44780Goto( mcp23017Handle[0],
                             MCP23017_OLATB, row, i );
                hd44780WriteData( mcp23017Handle[0],
                                  MCP23017_OLATB,
                                  pacManRight[j] );
                pthread_mutex_unlock( &displayBusy );
            if (( i - 2 ) > 0 )
                {
                    pthread_mutex_lock( &displayBusy );
                    hd44780Goto( mcp23017Handle[0],
                                 MCP23017_OLATB, row, i - 2 );
                    hd44780WriteData( mcp23017Handle[0],
                                      MCP23017_OLATB,
                                      ghost[j] );
                    pthread_mutex_unlock( &displayBusy );
                }
                if (( i - 3 ) > 0 )
                {
                    pthread_mutex_lock( &displayBusy );
                    hd44780Goto( mcp23017Handle[0],
                                 MCP23017_OLATB, row, i - 3 );
                    hd44780WriteData( mcp23017Handle[0],
                                      MCP23017_OLATB,
                                      ghost[j] );
                    pthread_mutex_unlock( &displayBusy );
                }
                if (( i - 4 ) > 0 )
                {
                    pthread_mutex_lock( &displayBusy );
                    hd44780Goto( mcp23017Handle[0],
                                 MCP23017_OLATB, row, i - 4 );
                    hd44780WriteData( mcp23017Handle[0],
                                      MCP23017_OLATB,
                                      ghost[j] );
                    pthread_mutex_unlock( &displayBusy );
                }
                if (( i - 5 ) > 0 )
                {
                    pthread_mutex_lock( &displayBusy );
                    hd44780Goto( mcp23017Handle[0],
                                 MCP23017_OLATB, row, i - 5 );
                    hd44780WriteData( mcp23017Handle[0],
                                      MCP23017_OLATB,
                                      ghost[j] );
                    pthread_mutex_unlock( &displayBusy );
                }
                nanosleep( &sleepTime, NULL );
            }
            pthread_mutex_lock( &displayBusy );
            hd44780Goto( mcp23017Handle[0], MCP23017_OLATB, row, 0 );
            hd44780WriteDataString( mcp23017Handle[0],
                                    MCP23017_OLATB,
                                    "                " );
            pthread_mutex_unlock( &displayBusy );
        }
    }

    pthread_exit( NULL );

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
static void *displayTicker( void *threadTicker )
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
        hd44780Goto( mcp23017Handle[0], MCP23017_OLATB, 1, 0 );
        hd44780WriteDataString( mcp23017Handle[0], MCP23017_OLATB, displayText );
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
        hd44780Goto( mcp23017Handle[0], MCP23017_OLATB, text->row, pos );
        hd44780WriteDataString( mcp23017Handle[0],
                                MCP23017_OLATB,
                                timeString );
        pthread_mutex_unlock( &displayBusy );
        nanosleep( &sleepTime, NULL );

        timeVar = time( NULL );
        timePtr = localtime( &timeVar );

        sprintf( timeString, "%02d %02d %02d", timePtr->tm_hour,
                                               timePtr->tm_min,
                                               timePtr->tm_sec );
        pthread_mutex_lock( &displayBusy );
        hd44780Goto( mcp23017Handle[0], MCP23017_OLATB, text->row, pos );
        hd44780WriteDataString( mcp23017Handle[0], MCP23017_OLATB, timeString );
        pthread_mutex_unlock( &displayBusy );
        nanosleep( &sleepTime, NULL );
    }
    pthread_exit( NULL );
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
    struct lcdStruct *lcdPin = state->input;

    switch ( param )
    {
        case 'r' :
            lcdPin->rs = atoi( arg );
            break;
        case 'e' :
            lcdPin->en = atoi( arg );
            break;
        case 'a' :
            lcdPin->db[0] = atoi( arg );
            break;
        case 'b' :
            lcdPin->db[1] = atoi( arg );
            break;
        case 'c' :
            lcdPin->db[2] = atoi( arg );
            break;
        case 'd' :
            lcdPin->db[3] = atoi( arg );
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
    argp_parse( &argp, argc, argv, 0, 0, &lcdPin );
    // Need to check validity of pins.

    // ------------------------------------------------------------------------
    //  Initialise LCD.
    // ------------------------------------------------------------------------
    unsigned char data      = 0; // 4-bit mode.
    unsigned char lines     = 1; // 2 display lines.
    unsigned char font      = 1; // 5x8 font.
    unsigned char display   = 1; // Display on.
    unsigned char cursor    = 0; // Cursor off.
    unsigned char blink     = 0; // Blink (block cursor) off.
    unsigned char counter   = 1; // Increment DDRAM counter after data write
    unsigned char shift     = 0; // Do not shift display after data write.
    unsigned char mode      = 0; // Shift cursor.
    unsigned char direction = 0; // Right.

    mcp23017init();
    initialiseDisplay( mcp23017Handle[0], MCP23017_OLATB,
                       data, lines, font,
                       display, cursor, blink,
                       counter, shift,
                       mode, direction );

    struct timeStruct textTime =
    {
        .row = 0,
        .delay = 300,
        .align = CENTRE,
        .format = HMS
    };

    struct dateStruct textDate =
    {
        .row = 0,
        .delay = 300,
        .align = CENTRE,
        .format = DAY_DMY
    };

    struct tickerStruct ticker =
    {
        .text = "This text is really long and used to demonstrate the ticker!",
        .length = strlen( ticker.text ),
        .padding = 6,
        .row = 1,
        .increment = 1,
        .delay = 300
    };

    // Must be an int due to typecasting of a pointer.
    unsigned int pacManRow = 1;

    // Create threads and mutex for animated display functions.
    pthread_mutex_init( &displayBusy, NULL );
    pthread_t threads[2];
    pthread_create( &threads[0], NULL, displayTime, (void *) &textTime );
//    pthread_create( &threads[1], NULL, displayPacMan, (void *) pacManRow );
    pthread_create( &threads[1], NULL, displayTicker, (void *) &ticker );

    while (1)
    {
    };

    displayClear( mcp23017Handle[0], MCP23017_OLATB );
    displayHome( mcp23017Handle[0], MCP23017_OLATB );

    // Clean up threads.
    pthread_mutex_destroy( &displayBusy );
    pthread_exit( NULL );

}
