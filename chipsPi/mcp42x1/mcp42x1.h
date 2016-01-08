//  ===========================================================================
/*
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
*/
//  ===========================================================================

#define MCP42X1_VERSION 0100 // v01.00

//  ===========================================================================
/*
    Authors:        D.Faulke    07/01/2016

    Contributors:

    Changelog:

        v1.00   Original version.
*/
//  ===========================================================================

#ifndef MCP42X1_H
#define MCP42X1_H

//  MCP42x1 information. ------------------------------------------------------
/*
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
            R = 5, 10, 50 or 100 kOhms. Wiper resistance = 75 Ohms.

            Key:
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

        Device memory map:

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
*/


//  MCP42x1 data. -------------------------------------------------------------

//  Max number of MCP42X1 chips.
#define MCP42X1_MAX 2 // SPI0 has 2 chip selects but AUX has 3.
/*
    Mote:   It is possible to handle the chip select independently of the SPI
            controller and therefore have many more but that is outside the
            scope of this driver.
*/

//  MCP42x1 register addresses.
enum mcp42x1_registers
{
     MCP42X1_REG_WIPER0 = 0x00, // Wiper for resistor network 0.
     MCP42X1_REG_WIPER1 = 0x01, // Wiper for resistor network 1.
     MCP42X1_REG_TCON   = 0x04, // Terminal control.
     MCP42X1_REG_STATUS = 0x05  // Status.
};

//  TCON register masks.
enum mcp42x1_tcon
{
    MCP42X1_TCON_R0B  = 0x01, // Resistor newtork 0, pin B.
    MCP42X1_TCON_R0W  = 0x02, // Resistor network 0, wiper.
    MCP42X1_TCON_R0A  = 0x04, // Resistor network 0, pin A.
    MCP42X1_TCON_R0HW = 0x08, // Resistor network 0, hardware configuration.
    MCP42X1_TCON_R1B  = 0x10, // Resistor network 1, pin B.
    MCP42X1_TCON_R1W  = 0x20, // Resistor network 1, wiper.
    MCP42X1_TCON_R1A  = 0x40, // Resistor network 1, pin A.
    MCP42X1_TCON_R1HW = 0x80  // Resistor network 1, hardware configuration.
};

struct mcp42x1
{
    uint8_t id; // SPI handle.
    uint8_t cs; // Chip Select.
};

struct mcp42x1 *mcp42x1[MCP42X1_MAX];


//  MCP42x1 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1WriteByte( struct mcp42x1 *mcp42x1, uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Reads byte from register of MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1ReadByte( struct mcp42x1 *mcp42x1, uint8_t reg );

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t device, uint8_t mode, uint8_t bits, uint16_t divider );
/*
    device  : 0 = CS0
              1 = CS1
    mode    : SPI_MODE_0 : CPOL = 0, CPHA = 0.
              SPI_MODE_1 : CPOL = 0, CPHA = 1.
              SPI_MODE_2 : CPOL = 1, CPHA = 0.
              SPI_MODE_3 : CPOL = 1, CPHA = 1.
    bits    : bits per word, usually 8.
    divider : SPI bus clock divider, multiple of 2 up to 65536.
              SCLK = Core clock / divider.
              Core clock = 250MHz.
              Values of 0, 1 and 65536 are functionally equivalent.
              The fastest speed is with a value of 2, i.e. 125MHz.
*/

#endif // #ifndef MCP42X1_H
