/*
//  ===========================================================================

    mcp42x1:

    Driver for the MCP42x1 SPI digital potentiometer.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

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

    Authors:        D.Faulke    24/12/2015

    Contributors:

//  Information. --------------------------------------------------------------

    The MCP42x1 is an SPI bus operated Dual 7/8-bit digital potentiometer with
    non-volatile memory.

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
                        | CS   |  01 | 14  |  VDD | 1.8V -> 5.5V
                        | SCK  |  02 | 13  |  SDO |
                        | SDI  |  03 | 12  | SHDN |
                        | VSS  |  04 | 11  |   WP |
              ,----- // | P1B  |  05 | 10  |  P0B | // -----,
           R [ ]<--- // | P1W  |  06 | 09  |  P0W | // --->[ ] R
              '----- // | P1A  |  07 | 08  |  P0A | // -----'
                        +-------------------------+
            R = 5, 10, 50 or 100 kOhms.

            Wiper resistance = 75 Ohms.


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

#define MCP23017_REGISTERS    22
#define MCP23017_BANKS         2

// MCP23017 registers.
typedef enum reg { IODIRA,   IODIRB,   IPOLA,    IPOLB,    GPINTENA, GPINTENB,
                   DEFVALA,  DEFVALB,  INTCONA,  INTCONB,  IOCONA,   IOCONB,
                   GPPUA,    GPPUB,    INTFA,    INTFB,    INTCAPA,  INTCAPB,
                   GPIOA,    GPIOB,    OLATA,    OLATB } mcp23017Reg;

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

//  Data structures. ----------------------------------------------------------

typedef enum mcp23017Bank { BANK_0, BANK_1 } mcp23017Bank; // BANK mode.

struct mcp23017
{
    uint8_t      id;   // I2C handle.
    uint8_t      addr; // Address of MCP23017.
    mcp23017Bank bank; // 8-bit or 16-bit mode.
};

struct mcp23017 *mcp23017[MCP23017_MAX];


//  MCP23017 functions. -------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017WriteByte( struct mcp23017 *mcp23017,
                                  uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Writes word to register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017WriteWord( struct mcp23017 *mcp23017,
                                  uint8_t reg, uint16_t data );

//  ---------------------------------------------------------------------------
//  Reads byte from register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017ReadByte( struct mcp23017 *mcp23017, uint8_t reg );

//  ---------------------------------------------------------------------------
//  Reads word from register of MCP23017.
//  ---------------------------------------------------------------------------
int16_t mcp23017ReadWord( struct mcp23017 *mcp23017, uint8_t reg );

//  ---------------------------------------------------------------------------
//  Checks byte bits of MCP23017 register.
//  ---------------------------------------------------------------------------
bool mcp23017CheckBitsByte( struct mcp23017 *mcp23017,
                            uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Checks word bits of MCP23017 register.
//  ---------------------------------------------------------------------------
bool mcp23017CheckBitsWord( struct mcp23017 *mcp23017,
                            uint8_t reg, uint16_t data );

//  ---------------------------------------------------------------------------
//  Toggles byte bits of MCP23017 register.
//  ---------------------------------------------------------------------------
int8_t mcp23017ToggleBitsByte( struct mcp23017 *mcp23017,
                               uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Toggles word bits of MCP23017 register.
//  ---------------------------------------------------------------------------
int8_t mcp23017ToggleBitsWord( struct mcp23017 *mcp23017,
                               uint8_t reg, uint16_t data );

//  ---------------------------------------------------------------------------
//  Sets byte bits of MCP23017 register.
//  ---------------------------------------------------------------------------
int8_t mcp23017SetBitsByte( struct mcp23017 *mcp23017,
                            uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Sets word bits of MCP23017 register.
//  ---------------------------------------------------------------------------
int8_t mcp23017SetBitsWord( struct mcp23017 *mcp23017,
                            uint8_t reg, uint16_t data );

//  ---------------------------------------------------------------------------
//  Clears byte bits of MCP23017 register.
//  ---------------------------------------------------------------------------
int8_t mcp23017ClearBitsByte( struct mcp23017 *mcp23017,
                              uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Clears word bits of MCP23017 register.
//  ---------------------------------------------------------------------------
int8_t mcp23017ClearBitsWord( struct mcp23017 *mcp23017,
                              uint8_t reg, uint16_t data );

//  ---------------------------------------------------------------------------
//  Initialises MCP23017 registers. Call for each MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017Init( uint8_t addr );

#endif
