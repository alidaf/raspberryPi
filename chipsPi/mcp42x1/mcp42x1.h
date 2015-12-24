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
                        | VSS  |  04 | 11  |   NC |
              ,----- // | P1B  |  05 | 10  |  P0B | // -----,
           R [ ]<--- // | P1W  |  06 | 09  |  P0W | // --->[ ] R
              '----- // | P1A  |  07 | 08  |  P0A | // -----'
                        +-------------------------+
            R = 5, 10, 50 or 100 kOhms.

            Wiper resistance = 75 Ohms.


        Functions:

                    +---------------------------------------+
                    | Fn   | Description                    |
                    |------+--------------------------------|
                    | CS   | SPI chip select.               |
                    | SCK  | SPI clock input.               |
                    | SDI  | SPI serial data in.            |
                    | VSS  | GND.                           |
                    | PxA  | Potentiometer pin A.           |
                    | PxW  | Potentiometer wiper.           |
                    | PxB  | Potentiometer pin B.           |
                    | NC   | Not internally connected.      |
                    | SHDN | Hardware shutdown (reset).     |
                    | SDO  | Serial data out.               |
                    | VDD  | Supply voltage (1.8V to 5.5V). |
                    +---------------------------------------+

        Notes:

            NC is not internally connected but should be externally connected
            to either VSS or VDD to reduce noise coupling.

        Memory map:

                        +--------------------------------+
                        | Addr | Function          | Mem |
                        |------+-------------------+-----|
                        | 00h  | Volatile wiper 0. | RAM |
                        | 01h  | Volatile wiper 1. | RAM |
                        | 02h  | Reserved.         | --- |
                        | 03h  | Reserved.         | --- |
                        | 04h  | Volatile TCON.    | RAM |
                        | 05h  | Status.           | RAM |
                        | 06h+ | Reserved.         | --- |
                        +--------------------------------+
        Notes:

            All 16 locations are 9 bits wide.
            The status register at 05h has 5 status bits, 4 of which are
            reserved. Bit 1 is the shutdown status; 0 = normal, 1 = Shutdown.

            The TCON register (Terminal CONtrol) has 8 control bits, 4 for
            each wiper, as shown:

            +--------------------------------------------------------------+
            | bit8 | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
            |------+------+------+------+------+------+------+------+------|
            |  D8  | R1HW | R1A  | R1W  | R1B  | R0HW | R0A  | R0W  | R0B  |
            +--------------------------------------------------------------+

                RxHW : Forces potentiometer x into shutdown configuration of
                       the SHDN pin; 0 = normal, 1 = forced.
                RxA  : Connects/disconnects potentiometer x pin A to/from the
                       resistor network; 0 = connected, 1 = disconnected.
                RxW  : Connects/disconnects potentiometer x wiper to/from the
                       resistor network; 0 = connected, 1 = disconnected.
                RxB  : Connects/disconnects potentiometer x pin B to/from the
                       resistor network; 0 = connected, 1 = disconnected.

                The SHDN pin, when active, overrides the state of these bits.

            Thw resistance networks are controlled.......

//  ---------------------------------------------------------------------------
*/

#ifndef MCP42X1_H
#define MCP42X1_H

//  MCP42x1 -------------------------------------------------------------------

#define MCP42X1_MAX           2 // Max number of CS pins on Pi.

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
