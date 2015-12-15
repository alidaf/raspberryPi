/*
//  ===========================================================================

    hd44780i2cPi:

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

        gcc -c -fpic -Wall hd44780i2c.c -lwiringPi -lpthread

    Also use the following flags for Raspberry Pi optimisation:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    15/12/2015  This program.

    Contributors:

//  Information. --------------------------------------------------------------

    Pin layout for Hitachi HD44780 based LCD display.

        +------------------------------------------------------------+
        | Pin | Label | Pi   | Description                           |
        |-----+-------+------+---------------------------------------|
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
        +------------------------------------------------------------+

    LCD register bits:

        +---------------------------------------+   +-------------------+
        |RS |RW |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|   |Key|Effect         |
        |---+---+---+---+---+---+---+---+---+---|   |---+---------------|
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
        +---------------------------------------+   +-------------------+

    DDRAM: Display Data RAM.
    CGRAM: Character Generator RAM.

//  ---------------------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander:

                           +------------O------------+
                           |  Fn  | pin | pin |  Fn  |
                           |------+-----+-----+------|
                LCD RS  <--| GPB0 |  01 | 28  | GPA7 |
                LCD R/W <--| GPB1 |  02 | 27  | GPA6 |
                LCD EN  <--| GPB2 |  03 | 26  | GPA5 |
                           | GPB3 |  04 | 25  | GPA4 |
                LCD DB4 <--| GPB4 |  05 | 24  | GPA3 |
                LCD DB5 <--| GPB5 |  06 | 23  | GPA2 |
                LCD DB6 <--| GPB6 |  07 | 22  | GPA1 |
                LCD DB7 <--| GPB7 |  08 | 21  | GPA0 |
        +5V <---+----------|  VDD |  09 | 20  | INTA |
        GND <---+--||------|  VSS |  10 | 19  | INTB |
                  0.1uF    |   NC |  11 | 18  | RST  |--> +5V      10k
                Pi SCL1 <--|  SCL |  12 | 17  | A2   |--> GND | --/\/\/-> +5V
                Pi SDA1 <--|  SDA |  13 | 16  | A1   |--> GND | --/\/\/-> +5V
                           |   NC |  14 | 15  | A0   |--> GND | --/\/\/-> +5V
                           +-------------------------+

    Notes:  There is a 0.1uF ceramic capaictor across pins 09 and 10
            for stability.

            SCL1 and SDA1 are connected to the Pi directly. The Pi has
            resistors to 'pull' the voltage to +3.3V on these pins so
            using a logic level shifter is probably unnecessary.

            The RST (hardware reset) pin is kept high for normal operation.

            The I2C address device is addressed as follows:

                +-----+-----+-----+-----+-----+-----+-----+-----+
                |  0  |  1  |  0  |  0  |  A2 |  A1 |  A0 | R/W |
                +-----+-----+-----+-----+-----+-----+-----+-----+
                : <----------- slave address -----------> :     :
                : <--------------- control byte --------------> :

                R/W = 0: write.
                R/W = 1: read.

            Possible addresses are therefore 0x20 to 0x27 and set by wiring
            pins A0 to A2 low (GND) or high (+5V). If wiring high, the pins
            should be connected to +5V via a 10k resistor.

            Further HD44780 displays may be added by wiring the LCD EN pin to
            another MCP23017 pin. The RS, R/W and DB pins can be wired in
            parallel to the exisiting connections.

//  ---------------------------------------------------------------------------

    Reading/writing to the MCP23017:

        The MCP23017 has two 8-bit ports (PORTA & PORTB) that can operate in
        8-bit or 16-bit modes. Each port has associated registers but share
        a configuration register IOCON.

    MCP23017 register addresses:

            +----------------------------------------------------------+
            | BANK1 | BANK0 | Register | Description                   |
            |-------+-------+----------+-------------------------------|
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
            +----------------------------------------------------------+

        The IOCON register bits set various configurations including the BANK
        bit (bit 7):

            +-------------------------------------------------------+
            | BIT7 | BIT6 | BIT5 | BIT4 | BIT3 | BIT2 | BIT1 | BIT0 |
            |------+------+------+------+------+------+------+------|
            | BANK |MIRROR|SEQOP |DISSLW| HAEN | ODR  |INTPOL| ---- |
            +-------------------------------------------------------+

            BANK   = 1: PORTS are segregated, i.e. 8-bit mode.
            BANK   = 0: PORTS are paired into 16-bit mode.
            MIRROR = 1: INT pins are connected.
            MIRROR = 0: INT pins operate independently.
            SEQOP  = 1: Sequential operation disabled.
            SEQOP  = 0: Sequential operation enabled.
            DISSLW = 1: Slew rate disabled.
            DISSLW = 0: Slew rate enabled.
            HAEN   = 1: N/A for MCP23017.
            HAEN   = 0: N/A for MCP23017.
            ODR    = 1: INT pin configured as open-drain output.
            ODR    = 0: INT pin configured as active driver output.
            INTPOL = 1: Polarity of INT pin, active = low.
            INTPOL = 0: Polarity of INT pin, active = high.

            Default is 0 for all bits.

            The internal pull-up resistors are 100kOhm.
*/

//  Macros. -------------------------------------------------------------------

// Constants. Change these according to needs.
#define BITS_BYTE          8 // Number of bits in a byte.
#define BITS_NIBBLE        4 // Number of bits in a nibble.
#define PINS_DATA          4 // Number of data pins used.
#define MAX_DISPLAYS       1 // Number of displays.
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

//  MCP23017 ------------------------------------------------------------------

#define MCP23017_CHIPS         1 // Number of MCP20317 expander chips (max 8).

// I2C base addresses, set according to A0-A2. Maximum of 8.
#define MCP23017_ADDRESS_0  0x20
/*
#define MCP23017_ADDRESS_1  0x21 // Dummy address for additional chip.
#define MCP23017_ADDRESS_2  0x22 // Dummy address for additional chip.
#define MCP23017_ADDRESS_3  0x23 // Dummy address for additional chip.
#define MCP23017_ADDRESS_4  0x24 // Dummy address for additional chip.
#define MCP23017_ADDRESS_5  0x25 // Dummy address for additional chip.
#define MCP23017_ADDRESS_6  0x26 // Dummy address for additional chip.
#define MCP23017_ADDRESS_7  0x27 // Dummy address for additional chip.
*/
// MCP23017 register addresses ( IOCON.BANK = 0).
#define MCP23017_0_IODIRA   0x00
#define MCP23017_0_IODIRB   0x01
#define MCP23017_0_IPOLA    0x02
#define MCP23017_0_IPOLB    0x03
#define MCP23017_0_GPINTENA 0x04
#define MCP23017_0_GPINTENB 0x05
#define MCP23017_0_DEFVALA  0x06
#define MCP23017_0_DEFVALB  0x07
#define MCP23017_0_INTCONA  0x08
#define MCP23017_0_INTCONB  0x09
#define MCP23017_0_IOCONA   0x0a
#define MCP23017_0_IOCONB   0x0b
#define MCP23017_0_GPPUA    0x0c
#define MCP23017_0_GPPUB    0x0d
#define MCP23017_0_INTFA    0x0e
#define MCP23017_0_INTFB    0x0f
#define MCP23017_0_INTCAPA  0x10
#define MCP23017_0_INTCAPB  0x11
#define MCP23017_0_GPIOA    0x12
#define MCP23017_0_GPIOB    0x13
#define MCP23017_0_OLATA    0x14
#define MCP23017_0_OLATB    0x15

// MCP23017 register addresses (IOCON.BANK = 1).
/*
#define MCP23017_1_IODIRA   0x00
#define MCP23017_1_IODIRB   0x10
#define MCP23017_1_IPOLA    0x01
#define MCP23017_1_IPOLB    0x11
#define MCP23017_1_GPINTENA 0x02
#define MCP23017_1_GPINTENB 0x12
#define MCP23017_1_DEFVALA  0x03
#define MCP23017_1_DEFVALB  0x13
#define MCP23017_1_INTCONA  0x04
#define MCP23017_1_INTCONB  0x14
#define MCP23017_1_IOCONA   0x05
#define MCP23017_1_IOCONB   0x15
#define MCP23017_1_GPPUA    0x06
#define MCP23017_1_GPPUB    0x16
#define MCP23017_1_INTFA    0x07
#define MCP23017_1_INTFB    0x17
#define MCP23017_1_INTCAPA  0x08
#define MCP23017_1_INTCAPB  0x18
#define MCP23017_1_GPIOA    0x09
#define MCP23017_1_GPIOB    0x19
#define MCP23017_1_OLATA    0x0a
#define MCP23017_1_OLATB    0x1a
*/

static const char *i2cDevice = "/dev/i2c-1"; // Path to I2C file system.
static int mcp23017ID[MCP23017_CHIPS];       // IDs (handles) for each chip.

// Array of addresses for each MCP23017.
static uint8_t mcp23017Addr[MCP23017_CHIPS] = { MCP23017_ADDRESS_0 };
/*
    = { MCP23017_ADDRESS_0,
        MCP23017_ADDRESS_1,
        MCP23017_ADDRESS_2,
        MCP23017_ADDRESS_3,
        MCP23017_ADDRESS_4,
        MCP23017_ADDRESS_5,
        MCP23017_ADDRESS_6,
        MCP23017_ADDRESS_7 }
*/

//  Mutex. --------------------------------------------------------------------

pthread_mutex_t displayBusy; // Locks further writes to display until finished.

//  Data structures. ----------------------------------------------------------

struct HD44780i2c
{
    uint8_t data;  // I2C data GPIO.
    uint8_t clock; // I2C clock GPIO.
};

struct textStruct
{
    uint8_t row;        // Display row.
    uint8_t col;        // Display column.
    char    *buffer;    // Display text.
};

struct Calendar
{
    uint8_t row;        // Display row (y).
    uint8_t col;        // Display col (x).
    uint8_t length;     // Length of formatting string.
    uint8_t frames;     // Actual number of animation frames.
    char    *format[2]; // format strings. Use for animating.
    float   delay;      // Delay between updates (Seconds).
};
/*
        format[n] is a string containing <time.h> formatting codes.
        Some common codes are:
            %a  Abbreviated weekday name.
            %A  Full weekday name.
            %d  Day of the month.
            %b  Abbreviated month name.
            %B  Full month name.
            %m  Month number.
            %y  Abbreviated year.
            %Y  Full year.
            %H  Hour in 24h format.
            %I  Hour in 12h format.
            %M  Minute.
            %S  Second.
            %p  AM/PM.
*/

struct tickerStruct
{
    char     text[TEXT_MAX_LENGTH]; // Display text.
    uint16_t length;                // Text length.
    uint16_t padding;               // Text padding between end to start.
    uint8_t  row;                   // Display row.
    int16_t  increment;             // Size and direction of tick movement.
    uint16_t delay;                 // Delay between ticks (mS).
};
/*
    .increment = Number and direction of characters to rotate.
                 +ve: rotate left.
                 -ve: rotate right.
    .length + .padding must be < TEXT_MAX_LENGTH.
*/

//  Hardware functions. -------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
//  ---------------------------------------------------------------------------
static int8_t mcp23017WriteByte( uint8_t handle, uint8_t reg, uint8_t byte );

//  ---------------------------------------------------------------------------
//  Initialises MCP23017 chips.
//  ---------------------------------------------------------------------------
static int8_t mcp23017init( void );

//  ---------------------------------------------------------------------------
//  Toggles E (enable) bit in byte mode without changing other bits.
//  ---------------------------------------------------------------------------
static void hd44780ToggleEnable( uint8_t handle, uint8_t reg, uint8_t byte );

//  ---------------------------------------------------------------------------
//  Writes a command or data byte (according to mode) to HD44780 via MCP23017.
//  ---------------------------------------------------------------------------
static int8_t hd44780WriteByte( uint8_t handle, uint8_t reg,
                                uint8_t data,   uint8_t mode );

//  ---------------------------------------------------------------------------
//  Writes a data string to LCD.
//  ---------------------------------------------------------------------------
static int8_t hd44780WriteString( uint8_t handle, uint8_t reg, char *string );

//  ---------------------------------------------------------------------------
//  Moves cursor to row, position.
//  ---------------------------------------------------------------------------
/*
    All displays, regardless of size, have the same start address for each
    row due to common architecture. Moving from the end of a line to the start
    of the next is not contiguous memory.
*/
static int8_t hd44780Goto( uint8_t handle, uint8_t reg,
                           uint8_t row,    uint8_t pos );

//  Display init and mode functions. ------------------------------------------

//  ---------------------------------------------------------------------------
//  Clears display.
//  ---------------------------------------------------------------------------
static int8_t displayClear( uint8_t handle, uint8_t reg );

//  ---------------------------------------------------------------------------
//  Clears memory and returns cursor/screen to original position.
//  ---------------------------------------------------------------------------
static int8_t displayHome( uint8_t handle, uint8_t reg );

//  ---------------------------------------------------------------------------
//  Initialises display. Must be called before any other LCD functions.
//  ---------------------------------------------------------------------------
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
static int8_t initialiseDisplay( uint8_t handle, uint8_t reg,
                                 bool data,    bool lines,  bool font,
                                 bool display, bool cursor, bool blink,
                                 bool counter, bool shift,
                                 bool mode,    bool direction );
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

//  Mode settings. ------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Sets entry mode.
//  ---------------------------------------------------------------------------
/*
    counter = 0: Decrement DDRAM counter after data write (cursor moves L)
    counter = 1: Increment DDRAM counter after data write (cursor moves R)
    shift =   0: Do not shift display after data write.
    shift =   1: Shift display after data write.
*/
static int8_t setEntryMode( uint8_t handle, uint8_t reg,
                            bool counter, bool shift );

//  ---------------------------------------------------------------------------
//  Sets display mode.
//  ---------------------------------------------------------------------------
/*
    display = 0: Display off.
    display = 1: Display on.
    cursor  = 0: Cursor off.
    cursor  = 1: Cursor on.
    blink   = 0: Blink (block cursor) on.
    blink   = 0: Blink (block cursor) off.
*/
static int8_t setDisplayMode( uint8_t handle, uint8_t reg,
                              bool display, bool cursor, bool blink );

//  ---------------------------------------------------------------------------
//  Shifts cursor or display.
//  ---------------------------------------------------------------------------
/*
    mode      = 0: Shift cursor.
    mode      = 1: Shift display.
    direction = 0: Left.
    direction = 1: Right.
*/
static int8_t setMoveMode( uint8_t handle, uint8_t reg,
                           bool mode, bool direction );

//  Custom characters and animation. ------------------------------------------
/*
    Default characters actually have an extra row at the bottom, reserved
    for the cursor. It is therefore possible to define 8 5x8 characters.
*/

#define CUSTOM_SIZE  8 // Size of char (rows) for custom chars (5x8).
#define CUSTOM_MAX   8 // Max number of custom chars allowed.

struct customCharsStruct
{
    uint8_t num; // Number of custom chars (max 8).
    uint8_t data[CUSTOM_MAX][CUSTOM_SIZE];
};

#define CUSTOM_SIZE  8 // Size of char (rows) for custom chars (5x8).
#define CUSTOM_MAX   8 // Max number of custom chars allowed.

//  ---------------------------------------------------------------------------
//  Loads custom characters into CGRAM.
//  ---------------------------------------------------------------------------
/*
    Set command to point to start of CGRAM and load data line by line. CGRAM
    pointer is auto-incremented. Set command to point to start of DDRAM to
    finish.
*/
static int8_t loadCustom( uint8_t handle, uint8_t reg,
                    const uint8_t newChar[CUSTOM_CHARS][CUSTOM_SIZE] );

//  Display functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Displays text on display row as a tickertape.
//  ---------------------------------------------------------------------------
static void *displayTicker( void *threadTicker );
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

//  ---------------------------------------------------------------------------
//  Displays formatted date/time strings.
//  ---------------------------------------------------------------------------
void *displayCalendar( void *threadCalendar );
