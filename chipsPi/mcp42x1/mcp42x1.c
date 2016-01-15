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
//  Returns value of MCP42x1 register.
//  ---------------------------------------------------------------------------
int16_t mcp42x1ReadReg( uint8_t spi, uint8_t reg )
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
    uint8_t  cmd;
    uint16_t ret;
    char data[2];
    char bytes[2];

    printf( "Reading Register:\n" );
    printf( "Register = 0x%01x.\n", reg );
    printf( "Command  = 0x%01x.\n", MCP42X1_CMD_READ );

    cmd  = ( reg << 4 ) & 0xf0;              // Register address.
    cmd |= ( MCP42X1_CMD_READ << 2 ) & 0x0c; // Command.
    printf( "Command byte = 0x%02x.\n", cmd );

    bytes[0] = cmd;
    printf( "Byte 0 = 0x%02x.\n", bytes[0] );
    bytes[1] = 0x00;
    printf( "Byte 1 = 0x%02x.\n", bytes[1] );

    // Send command and receive data via SPI bus.
    spiXfer( spi, bytes, data, 2 );
    printf( "Reg Data = 0x%04x.\n", data );

    // Convert chars back into a 16-bit word.
    ret = ( data[0] << 8 ) | data[1];
    return ret;
}

//  ---------------------------------------------------------------------------
//  Writes bytes to register of MCP42x1.
//  ---------------------------------------------------------------------------
void mcp42x1WriteReg( uint8_t spi, uint8_t reg, uint16_t value )
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
    Data is 10 bits.
*/
{
    uint8_t cmd;
    uint8_t data;
    char bytes[2];

    cmd  = ( reg << 4 ) & 0xf0;               // Register address.
    cmd |= ( MCP42X1_CMD_WRITE << 2 ) & 0x0c; // Command.
    cmd |= (( value >> 8 ) & 0x03 );          // Data bits 8 - 9.
    data = ( value & 0xff );                  // Data bits 0 - 7.

    bytes[0] = cmd;
    bytes[1] = data;

    printf( "Writing Register:\n" );
    printf( "Register = 0x%01x.\n", reg );
    printf( "Data     = 0x%04x.\n", value );
    printf( "Command  = 0x%01x.\n", MCP42X1_CMD_WRITE );
    printf( "Cmd byte = 0x%02x.\n", bytes[0] );
    printf( "Dat byte = 0x%02x.\n", bytes[1] );

    // Send command via SPI bus.
    spiWrite( spi, bytes,  2 );

    return;
}

//  ---------------------------------------------------------------------------
//  Sets wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1SetResistance( uint8_t spi, uint8_t wiper, uint16_t value )
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
    switch ( wiper )
    {
        case 0  : mcp42x1WriteReg( spi, MCP42X1_REG_WIPER0, value ); break;
        case 1  : mcp42x1WriteReg( spi, MCP42X1_REG_WIPER1, value ); break;
        default : return;
    }
    return;
}

//  ---------------------------------------------------------------------------
//  Increments wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1IncResistance( uint8_t spi, uint8_t wiper )
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
    uint8_t cmd;
    char    bytes[1];

    switch ( wiper ) // Register address.
    {
        case 0 : cmd = ( MCP42X1_REG_WIPER0 << 4 ) & 0xf0; break;
        case 1 : cmd = ( MCP42X1_REG_WIPER1 << 4 ) & 0xf0; break;
        default : return;
    }
    cmd |= ( MCP42X1_CMD_INC << 2 ) & 0x0c; // Command;
    bytes[0] = cmd;

    printf( "Incrementing Resistance:\n" );
    printf( "Register = 0x%01x.\n", wiper );
    printf( "Command  = 0x%01x.\n", MCP42X1_CMD_INC );
    printf( "Cmd byte = 0x%02x.\n", bytes[0] );

    // Send command via SPI bus.
    spiWrite( spi, bytes, 1 );

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
void mcp42x1DecResistance( uint8_t spi, uint8_t wiper )
{
    uint8_t cmd;
    char    bytes[1];

    switch ( wiper ) // Register address.
    {
        case 0 : cmd = ( MCP42X1_REG_WIPER0 << 4 ) & 0xf0; break;
        case 1 : cmd = ( MCP42X1_REG_WIPER1 << 4 ) & 0xf0; break;
        default : return;
    }
    cmd |= ( MCP42X1_CMD_DEC << 2 ) & 0x0c; // Command;
    bytes[0] = cmd;

    printf( "Incrementing Resistance:\n" );
    printf( "Register = 0x%01x.\n", wiper );
    printf( "Command  = 0x%01x.\n", MCP42X1_CMD_DEC );
    printf( "Cmd byte = 0x%02x.\n", bytes[0] );

    // Send command via SPI bus.
    spiWrite( spi, bytes, 1 );

    return;
}

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t spi, uint8_t wiper )
//  SPI configuration flags - see pigpio.
{
    struct mcp42x1 *mcp42x1this; // MCP42x1 instance.
    static bool init = false;    // 1st call.

    uint8_t handle = -1; // MCP42x1 handle.
    uint8_t i;           // Counter.

    if ( !init ) // 1st call to this function.
    {
        for ( i = 0; i < MCP42X1_DEVICES * MCP42X1_WIPERS; i++ )
            mcp42x1[i] = NULL;
    }

    // Get next available handle.
    for ( i = 0; i < MCP42X1_DEVICES * MCP42X1_WIPERS; i++ )
        if ( mcp42x1[i] == NULL ) // If not already init.
        {
            handle = i; // Next available id.
            i = MCP42X1_DEVICES * MCP42X1_WIPERS; // Break.
        }

    if ( handle < 0 ) return MCP42X1_ERR_NOINIT; // Return if not init.

    // Return if wiper is not valid.
    if (( wiper < 0 ) | ( wiper > 1 )) return MCP42X1_ERR_NOWIPER;

    // Allocate memory for MCP42x1 data structure.
    mcp42x1this = malloc( sizeof ( struct mcp42x1 ));

    // Return if unable to allocate memory.
    if ( mcp42x1this == NULL ) return MCP42X1_ERR_NOMEM;

    // Create an instance of this device.
    mcp42x1this->spi = spi;     // SPI handle.
    mcp42x1this->wiper = wiper; // Wiper.

    // Check this instance is unique.
    for ( i = 0; i < handle; i++ )
        if (( mcp42x1this->spi   == mcp42x1[i]->spi ) &&
            ( mcp42x1this->wiper == mcp42x1[i]->wiper ))
             return MCP42X1_ERR_DUPLIC;

    mcp42x1[handle] = mcp42x1this; // Copy into instance.

    init = true;

    return handle;
};
