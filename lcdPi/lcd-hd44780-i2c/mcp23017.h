/*
//  ===========================================================================

    mcp23017:

    Driver for the MCP23017 port expander.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on the following guides and codes:
        MCP23017 data sheet.
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/21952b.pdf

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

    Authors:        D.Faulke    15/12/2015  This program.

    Contributors:

//  Information. --------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander:

                        +------------O------------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
                      { | GPB0 |  01 | 28  | GPA7 | }
                      { | GPB1 |  02 | 27  | GPA6 | }
                      { | GPB2 |  03 | 26  | GPA5 | }
          BANKA GPIOs { | GPB3 |  04 | 25  | GPA4 | } BANKB GPIOs
                      { | GPB4 |  05 | 24  | GPA3 | }
                      { | GPB5 |  06 | 23  | GPA2 | }
                      { | GPB6 |  07 | 22  | GPA1 | }
                      { | GPB7 |  08 | 21  | GPA0 | }
                +5V <---|  VDD |  09 | 20  | INTA |---> Interrupt A.
                GND <---|  VSS |  10 | 19  | INTB |---> Interrupt B.
                      x-|   NC |  11 | 18  | RST  |---> Reset when low.
            I2C CLK <---|  SCL |  12 | 17  | A2   |---}
            I2C I/O <---|  SDA |  13 | 16  | A1   |---} Address.
                      x-|   NC |  14 | 15  | A0   |---}
                        +-------------------------+

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

#ifndef MCP23017_H
#define MCP23017_H

//  MCP23017 ------------------------------------------------------------------

#define MCP23017_MAX           8 // Max number of MCP23017s.

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

#define MCP23017_REGISTERS    22
#define MCP23017_BANKS         2

// MCP23017 registers.
enum mcp23017Reg_t
    { IODIRA,   IODIRB,   IPOLA,    IPOLB,    GPINTENA, GPINTENB,
      DEFVALA,  DEFVALB,  INTCONA,  INTCONB,  IOCONA,   IOCONB,
      GPPUA,    GPPUB,    INTFA,    INTFB,    INTCAPA,  INTCAPB,
      GPIOA,    GPIOB,    OLATA,    OLATB };

// MCP23017 register addresses ( IOCON.BANK = 0).
#define BANK0_IODIRA   0x00
#define BANK0_IODIRB   0x01
#define BANK0_IPOLA    0x02
#define BANK0_IPOLB    0x03
#define BANK0_GPINTENA 0x04
#define BANK0_GPINTENB 0x05
#define BANK0_DEFVALA  0x06
#define BANK0_DEFVALB  0x07
#define BANK0_INTCONA  0x08
#define BANK0_INTCONB  0x09
#define BANK0_IOCONA   0x0a
#define BANK0_IOCONB   0x0b
#define BANK0_GPPUA    0x0c
#define BANK0_GPPUB    0x0d
#define BANK0_INTFA    0x0e
#define BANK0_INTFB    0x0f
#define BANK0_INTCAPA  0x10
#define BANK0_INTCAPB  0x11
#define BANK0_GPIOA    0x12
#define BANK0_GPIOB    0x13
#define BANK0_OLATA    0x14
#define BANK0_OLATB    0x15

// MCP23017 register addresses (IOCON.BANK = 1).

#define BANK1_IODIRA   0x00
#define BANK1_IODIRB   0x10
#define BANK1_IPOLA    0x01
#define BANK1_IPOLB    0x11
#define BANK1_GPINTENA 0x02
#define BANK1_GPINTENB 0x12
#define BANK1_DEFVALA  0x03
#define BANK1_DEFVALB  0x13
#define BANK1_INTCONA  0x04
#define BANK1_INTCONB  0x14
#define BANK1_IOCONA   0x05
#define BANK1_IOCONB   0x15
#define BANK1_GPPUA    0x06
#define BANK1_GPPUB    0x16
#define BANK1_INTFA    0x07
#define BANK1_INTFB    0x17
#define BANK1_INTCAPA  0x08
#define BANK1_INTCAPB  0x18
#define BANK1_GPIOA    0x09
#define BANK1_GPIOB    0x19
#define BANK1_OLATA    0x0a
#define BANK1_OLATB    0x1a

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

//  Data structures. ----------------------------------------------------------

unit8_t mcp23017_Register[MCP23017_REGISTERS][MCP23017_BANKS] =
/*
    Register address can be reference with enumerated type
         {          BANK0, BANK1          }
*/
        {{   BANK0_IODIRA, BANK1_IODIRA   },
         {   BANK0_IODIRB, BANK1_IODIRB   },
         {    BANK0_IPOLA, BANK1_IPOLA    },
         {    BANK0_IPOLB, BANK1_IPOLB    },
         { BANK0_GPINTENA, BANK1_GPINTENA },
         { BANK0_GPINTENB, BANK1_GPINTENB },
         {  BANK0_DEFVALA, BANK1_DEFVALA  },
         {  BANK0_DEFVALB, BANK1_DEFVALB  },
         {  BANK0_INTCONA, BANK1_INTCONA  },
         {  BANK0_INTCONB, BANK1_INTCONB  },
         {   BANK0_IOCONA, BANK1_IOCONA   },
         {   BANK0_IOCONB, BANK1_IOCONB   },
         {    BANK0_GPPUA, BANK1_GPPUA    },
         {    BANK0_GPPUB, BANK1_GPPUB    },
         {    BANK0_INTFA, BANK1_INTFA    },
         {    BANK0_INTFB, BANK1_INTFB    },
         {  BANK0_INTCAPA, BANK1_INTCAPA  },
         {  BANK0_INTCAPB, BANK1_INTCAPB  },
         {    BANK0_GPIOA, BANK1_GPIOA    },
         {    BANK0_GPIOB, BANK1_GPIOB    },
         {    BANK0_OLATA, BANK1_OLATA    },
         {    BANK0_OLATB, BANK1_OLATB    }};

typedef enum mcp23017_bits = { BYTE, WORD };    // 8-bit or 16-bit.
typedef enum mcp23017_mode = { BYTE, SEQ  };    // Read/write mode.

struct Mcp23017
{
    uint8_t         addr;   // Address of MCP23017.
    mcp23017_bits   bits;   // 8-bit or 16-bit mode.
    mcp23017_mode   mode;   // Byte or sequential R/W mode.
};

struct Mcp23017 *mcp23017[MCP23017_MAX];


//  Hardware functions. -------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
//  ---------------------------------------------------------------------------
static int8_t mcp23017WriteByte( uint8_t handle, uint8_t reg, uint8_t byte );

//  ---------------------------------------------------------------------------
//  Initialises MCP23017 chips.
//  ---------------------------------------------------------------------------
int8_t mcp23017_init( uint8_t addr, bitsMode_t bits, rdwrMode_t rdwr );

#endif
