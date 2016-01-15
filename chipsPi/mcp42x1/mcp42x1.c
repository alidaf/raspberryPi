//  ===========================================================================
/*
    mcp42x1:

    Driver for the MCP42x1 SPI digital potentiometer.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on the MCP42x1 data sheet:
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/22059b.pdf.

    Uses the pigpio library for SPI access.
        - see http://abyz.co.uk/rpi/pigpio/
          and https://github.com/joan2937/pigpio.

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
/*
    For a shared library, compile with:

        gcc -c -Wall -fpic mcp42x1.c -lpigpio
        gcc -shared -o libmcp42x1.so mcp42x1.o

    For Raspberry Pi v1 optimisation use the following flags:

        -march=armv6zk -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

    For Raspberry Pi v2 optimisation use the following flags:

        -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4
        -ffast-math -pipe -O3

*/
//  ===========================================================================
/*
    Authors:        D.Faulke            15/01/2016

    Contributors:

*/
//  ===========================================================================

//  Standard libraries.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>

// pigpio library needs to be installed manually.
#include <pigpio.h>

//  Companion header.
#include "mcp42x1.h"


//  MCP42x1 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes bytes to register of MCP42x1.
//  ---------------------------------------------------------------------------
void mcp42x1WriteReg( uint8_t handle, uint16_t reg, uint16_t data )
/*
    This is a 16-bit command in 2 8-bit parts:

    +----------------------------------------------------------------+
    |  reg address  |  cmd  | data  ||             data              |
    |---------------+-------+-------||-------------------------------|
    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 || 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    |---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---|
    | 0 | 0 | 0 | 0 | 0 | 0 | x | x || x | x | x | x | x | x | x | x |  Wiper 0
    | 0 | 0 | 0 | 1 | 0 | 0 | x | x || x | x | x | x | x | x | x | x |  Wiper 1
    | 0 | 1 | 0 | 0 | 0 | 0 | x | x || x | x | x | x | x | x | x | x |  TCON
    | 0 | 1 | 0 | 1 | 0 | 0 | x | x || x | x | x | x | x | x | x | x |  Status
    +----------------------------------------------------------------+

    All other addresses are reserved.
*/
{
    union command
    {
        uint16_t word;
        char    *bytes;
    } cmd;

    cmd.word =  MCP42X1_CMD_WRITE;  // Command.
    cmd.word |= reg << 8;           // Register address.
    cmd.word |= ( data & 0x1ff );   // Data.

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes,  2 );

    return;
}

//  ---------------------------------------------------------------------------
//  Returns value of MCP42x1 register.
//  ---------------------------------------------------------------------------
int16_t mcp42x1ReadReg( uint8_t handle, uint16_t reg )
/*
    This is a 16-bit command in 2 8-bit parts:

    +----------------------------------------------------------------+
    |  reg address  |  cmd  | data  ||             data              |
    |---------------+-------+-------||-------------------------------|
    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 || 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    |---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---|
    | 0 | 0 | 0 | 0 | 1 | 1 | - | - || - | - | - | - | - | - | - | - |  Wiper 0
    | 0 | 0 | 0 | 1 | 1 | 1 | - | - || - | - | - | - | - | - | - | - |  Wiper 1
    | 0 | 1 | 0 | 0 | 1 | 1 | - | - || - | - | - | - | - | - | - | - |  TCON
    | 0 | 1 | 0 | 1 | 1 | 1 | - | - || - | - | - | - | - | - | - | - |  Status
    +----------------------------------------------------------------+

    All other addresses are reserved.
*/
{
    union command
    {
        uint16_t word;
        char    *bytes;
    } cmd, data;

    cmd.word =  MCP42X1_CMD_READ; // Command.
    cmd.word |= reg << 8;         // Register address.

    // Send command and receive data via SPI bus.
    spiXfer( mcp42x1[handle]->spi, cmd.bytes, data.bytes, 2 );
    printf( "Register data = 0x%04x.\n", data.word );

    return data.word;
}

//  ---------------------------------------------------------------------------
//  Sets wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1SetResistance( uint8_t handle, uint16_t resistance )
/*
    This is a 16-bit command in 2 8-bit parts:

    +----------------------------------------------------------------+
    |  reg address  |  cmd  | data  ||             data              |
    |---------------+-------+-------||-------------------------------|
    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 || 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    |---+---+---+---+---+---+---+---++---+---+---+---+---+---+---+---|
    | 0 | 0 | 0 | 0 | 0 | 0 | x | x || x | x | x | x | x | x | x | x |  Wiper 0
    | 0 | 0 | 0 | 1 | 0 | 0 | x | x || x | x | x | x | x | x | x | x |  Wiper 1
    +----------------------------------------------------------------+

    This is effectively the same as the mcp42x1WriteReg function but limited to
    the wiper registers.
    The highest possible data value is 0x3ff but full scale settings are:
        0x000 to 0x081 for 7-bit mode, or
        0x000 to 0x101 for 8-bit mode.
*/
{
    union command
    {
        uint16_t word;
        char    *bytes;
    } cmd;

    cmd.word =  MCP42X1_CMD_WRITE;           // Command.
    cmd.word |= mcp42x1[handle]->wiper << 8; // Register address.
    cmd.word |= ( resistance & 0x1ff );      // Resistance.

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes,  2 );

    return;
}

//  ---------------------------------------------------------------------------
//  Increments wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1IncResistance( uint8_t handle )
/*
    This is an 8-bit command:

        +-------------------------------+
        |  reg address  |  cmd  | data  |
        |---------------+-------+-------|
        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
        |---+---+---+---+---+---+---+---+
        | 0 | 0 | 0 | 0 | 0 | 1 | - | - | Wiper 0
        | 0 | 0 | 0 | 1 | 0 | 1 | - | - | Wiper 1
        +-------------------------------+
*/
{
    union command
    {
        uint8_t full;
        char    *bytes;
    } cmd;

    cmd.full =  MCP42X1_CMD_INC;                   // Command.
    cmd.full |= ( mcp42x1[handle]->wiper & 0xf0 ); // Wiper address.

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes, 1 );

    return;
}

//  ---------------------------------------------------------------------------
//  Decrements wiper resistance.
//  ---------------------------------------------------------------------------
/*
    This is an 8-bit command:

        +-------------------------------+
        |  reg address  |  cmd  | data  |
        |---------------+-------+-------|
        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
        |---+---+---+---+---+---+---+---+
        | 0 | 0 | 0 | 0 | 1 | 0 | - | - | Wiper 0
        | 0 | 0 | 0 | 1 | 1 | 0 | - | - | Wiper 1
        +-------------------------------+
*/
void mcp42x1DecResistance( uint8_t handle )
{
    union command
    {
        uint8_t full;
        char    *bytes;
    } cmd;

    cmd.full =  MCP42X1_CMD_DEC;                   // Command.
    cmd.full |= ( mcp42x1[handle]->wiper & 0xf0 ); // Wiper address.

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes, 1 );

    return;
}

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t spi, uint8_t wiper )
//  SPI configuration flags - see pigpio.
{
    struct mcp42x1 *mcp42x1this; // MCP42x1 instance.
    static bool    init = false; // 1st call.
    static uint8_t index = 0;    // Index for mcp42x1 struct.

    int8_t  handle  = -1; // MCP41x1 handle.
    uint8_t i;            // Counter.

    if ( !init ) // 1st call to this function.
    {
        for ( i = 0; i < MCP42X1_DEVICES * MCP42X1_WIPERS; i++ )
            mcp42x1[i] = NULL;
    }

    // Get next available ID.
    for ( i = 0; i < MCP42X1_DEVICES * MCP42X1_WIPERS; i++ )
        if ( mcp42x1[i] == NULL ) // If not already init.
        {
            handle = i; // Next available id.
            i = MCP42X1_DEVICES * MCP42X1_WIPERS; // Break.
        }

    if ( handle < 0 ) return MCP42X1_ERR_NOINIT; // Return if not init.

    // Allocate memory for MCP42x1 data structure.
    mcp42x1this = malloc( sizeof ( struct mcp42x1 ));

    // Return if unable to allocate memory.
    if ( mcp42x1this == NULL ) return MCP42X1_ERR_NOMEM;

    // Set wiper address. Return if wiper is not valid.
    switch ( wiper )
    {
        case 0  : mcp42x1this->wiper = MCP42X1_REG_WIPER0; break;
        case 1  : mcp42x1this->wiper = MCP42X1_REG_WIPER1; break;
        default : return MCP42X1_ERR_NOWIPER; break;
    }

    // Create an instance of this device.
    mcp42x1this->spi = spi; // SPI handle.

    // Check this instance is unique.
    for ( i = 0; i < index; i++ )
        if (( mcp42x1this->spi   == mcp42x1[i]->spi ) &&
            ( mcp42x1this->wiper == mcp42x1[i]->wiper ))
             return MCP42X1_ERR_DUPLIC;

    mcp42x1[index] = mcp42x1this; // Copy into instance.

    index++;
    init = true;

    return handle;
};
