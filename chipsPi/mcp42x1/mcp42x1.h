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

#define MCP42X1_VERSION 01.01

//  ===========================================================================
/*
    Authors:        D.Faulke            14/01/2016

    Contributors:

    Changelog:

        v01.00      Original version.
        v01.01      Rewrote init routine.
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

            The maximum SCK (serial clock) frequency is 10MHz.

            The only SPI modes supported are 0,0 and 1,1.

        Commands:

            The MCP42x1 has 4 commands:

                +-----------------------------------------------------+
                | Command    | Size   | addr  |cmd|       data        |
                |------------+--------+-------+---+-------------------|
                | Read data  | 16-bit |x|x|x|x|1|1|x|x|x|x|x|x|x|x|x|x|
                | Write data | 16-bit |x|x|x|x|0|0|x|x|x|x|x|x|x|x|x|x|
                | Increment  |  8-bit |x|x|x|x|0|1|x|x|-|-|-|-|-|-|-|-|
                | Decrement  |  8-bit |x|x|x|x|1|0|x|x|-|-|-|-|-|-|-|-|
                +---------------------------------+-+-+-+-+-+-+-+-+-+-+
                | Min resistance (x-bit) = 0x000  |0|0|0|0|0|0|0|0|0|0|
                | Max resistance (7-bit) = 0x080  |0|0|1|0|0|0|0|0|0|0|
                | Max resistance (8-bit) = 0x100  |0|1|0|0|0|0|0|0|0|0|
                +-----------------------------------------------------+
*/

//  MCP42x1 data. -------------------------------------------------------------

//  Max number of MCP42X1 chips, determined by No of chip selects.
#define MCP42X1_DEVICES   2 // SPI0 has 2 chip selects but AUX has 3.
#define MCP42X1_WIPERS    2 // Number of wipers on MCP42x1.
#define MCP42X1_RMIN 0x0000 // Minimum wiper value.
#define MCP42X1_RMAX 0x0100 // Maximum wiper value.

//  Maximum SCK frequency = 10MHz.
#define MCP42X1_SPI_BAUD 10000000

//  Commands.
#define MCP42X1_CMD_READ  0x0c00
#define MCP42X1_CMD_WRITE 0x0000
#define MCP42X1_CMD_INC   0x04
#define MCP42X1_CMD_DEC   0x08

//  MCP42x1 register addresses.
enum mcp42x1_registers
{
     MCP42X1_REG_WIPER0 = 0x00, // Wiper for resistor network 0.
     MCP42X1_REG_WIPER1 = 0x10, // Wiper for resistor network 1.
     MCP42X1_REG_TCON   = 0x40, // Terminal control.
     MCP42X1_REG_STATUS = 0x50  // Status.
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

//  Error codes.
enum mcp42x1_error
{
    MCP42X1_ERR_NOWIPER = -2, // Wiper is invalid.
    MCP42X1_ERR_NOINIT  = -3, // Couldn't initialise MCP42x1.
    MCP42X1_ERR_NOMEM   = -4, // Not enough memory.
    MCP42X1_ERR_DUPLIC  = -5, // Duplicate properties.
};

struct mcp42x1
{
    uint8_t spi;   // SPI handle.
    uint8_t wiper; // Wiper.
};

struct mcp42x1 *mcp42x1[MCP42X1_DEVICES * MCP42X1_WIPERS];


//  MCP42x1 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes bytes to register of MCP42x1.
//  ---------------------------------------------------------------------------
void mcp42x1WriteReg( uint8_t handle, uint16_t reg, uint16_t data );

//  ---------------------------------------------------------------------------
//  Returns value of MCP42x1 register.
//  ---------------------------------------------------------------------------
int16_t mcp42x1ReadReg( uint8_t handle, uint16_t reg );

//  ---------------------------------------------------------------------------
//  Sets wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1SetResistance( uint8_t handle, uint16_t resistance );

//  ---------------------------------------------------------------------------
//  Increments wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1IncResistance( uint8_t handle );

//  ---------------------------------------------------------------------------
//  Decrements wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1DecResistance( uint8_t handle );

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t spi, uint8_t wiper );

#endif // #ifndef MCP42X1_H
