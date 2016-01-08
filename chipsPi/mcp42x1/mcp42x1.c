//  ===========================================================================
/*
    mcp42x1:

    Driver for the MCP42x1 SPI digital potentiometer.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on the MCP42x1 data sheet:
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/22059b.pdf.

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
    Authors:        D.Faulke            08/01/2016

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

// pigpio library needs to be installed - see http://abyz.co.uk/rpi/pigpio/
#include <pigpio.h>

//  Companion header.
#include "mcp42x1.h"


//  MCP42x1 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes bytes to register of MCP42x1.
//  ---------------------------------------------------------------------------
void mcp42x1WriteReg( uint8_t handle, uint16_t reg, uint16_t data )
{
    union u_write // Need to split command into bytes.
    {
        uint16_t full;
        char    *bytes;
    } cmd;

    uint8_t len = 16 / sizeof( char );

    cmd.full = MCP42X1_CMD_WRITE; // MCP42x1 write command.
    cmd.full |= reg;              // Register to write to.
    cmd.full |= ( data & 0x1ff ); // Mask in lower 9 bits for data.

    printf( "Writing 0x%04x to reg 0x%04x. Command =  0x%04x.\n",
        data, reg, cmd.full );

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes, len );
}

//  ---------------------------------------------------------------------------
//  Returns value of MCP42x1 register.
//  ---------------------------------------------------------------------------
int16_t mcp42x1ReadReg( uint8_t handle, uint16_t reg )
{

    union u_read // Need to split command into bytes and join data into word.
    {
        uint16_t full;
        char    *bytes;
    } cmd, data;

    uint8_t len = 16 / sizeof( char );

    cmd.full = MCP42X1_CMD_READ; // MCP42x1 read command
    cmd.full |= reg;             // MCP42X1 register to read.

    printf( "Reading register 0x%04x. Command =  0x%04x.\n", reg, cmd.full );
    // Send command and receive data via SPI bus.
    spiXfer( mcp42x1[handle]->spi, cmd.bytes, data.bytes, len );
    printf( "Read bytes from register 0x%04x = 0x%04x.\n", reg, data.full );

    return data.full;
}

//  ---------------------------------------------------------------------------
//  Sets wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1SetResistance( uint8_t handle, uint16_t resistance )
{
    union u_write // Need to split command into bytes.
    {
        uint16_t full;
        char    *bytes;
    } cmd;

    uint8_t len = 16 / sizeof( char );

    cmd.full = MCP42X1_CMD_WRITE; // MCP42x1 write command.

    cmd.full |= mcp42x1[handle]->wip;   // Set wiper to change.
    cmd.full |= ( resistance & 0x1ff ); // Set value to write.

    printf( "Setting resistance of wiper %d to 0x%03x. Command = 0x%04x.\n",
        mcp42x1[handle]->wip, resistance, cmd.full );

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes, len );
}

//  ---------------------------------------------------------------------------
//  Increments wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1IncResistance( uint8_t handle )
{
    union u_write // Need to split command into bytes.
    {
        uint8_t full;
        char   *bytes;
    } cmd;

    uint8_t len = 8 / sizeof( char );

    cmd.full = MCP42X1_CMD_INC;       // MCP42x1 increment command.
    cmd.full |= mcp42x1[handle]->wip; // Set wiper to change.

    printf( "Incrementing resistance of wiper %d. Command = 0x%02x.\n",
        mcp42x1[handle]->wip, cmd.full );

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes, len );
}

//  ---------------------------------------------------------------------------
//  Decrements wiper resistance.
//  ---------------------------------------------------------------------------
void mcp42x1DecResistance( uint8_t handle )
{
    union u_write
    {
        uint8_t full;
        char   *bytes;
    } cmd;

    uint8_t len = 8 / sizeof( char );

    cmd.full = MCP42X1_CMD_DEC;       // MCP42x1 decrement command.
    cmd.full |= mcp42x1[handle]->wip; // Set wiper to change.

    printf( "Decrementing resistance of wiper %d. Command = 0x%02x.\n",
        mcp42x1[handle]->wip, cmd.full );

    // Send command via SPI bus.
    spiWrite( mcp42x1[handle]->spi, cmd.bytes, len );
}

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t cs, uint8_t wip, uint32_t flags )
//  SPI configuration flags - see pigpio.
{
    struct mcp42x1 *mcp42x1this; // MCP42x1 instance.
    static bool init = false;    // 1st call.
    static uint8_t index = 0;    // Index for mcp42x1 struct.

    int8_t spi = -1; // SPI handle.
    int8_t id  = -1; // MCP41x1 handle.
    uint8_t i;       // Counter.

    if ( !init ) // 1st call to this function.
    {
        for ( i = 0; i < MCP42X1_MAX * MCP42X1_WIP; i++ )
            mcp42x1[i] = NULL;
    }

    // Check network is within limits.
    if (( wip < 0 ) | ( wip > ( MCP42X1_WIP - 1 ))) return MCP42X1_ERR_NOWIPER;

    // Get next available ID.
    for ( i = 0; i < MCP42X1_MAX * MCP42X1_WIP; i++ )
        if ( mcp42x1[i] == NULL ) // If not already init.
        {
            id = i; // Next available id.
            i = MCP42X1_MAX * MCP42X1_WIP; // Break.
        }

    if ( id < 0 ) return MCP42X1_ERR_NOINIT; // Return if not init.

    // Allocate memory for MCP42x1 data structure.
    mcp42x1this = malloc( sizeof ( struct mcp42x1 ));

    // Return if unable to allocate memory.
    if ( mcp42x1this == NULL ) return MCP42X1_ERR_NOMEM;

    // Now init the SPI.
//    if (!init)
        spi = spiOpen( cs, MCP42X1_SPI_BAUD, flags );
    if ( spi < 0 ) return MCP42X1_ERR_NOSPI;

    // Create an instance of this device.
    mcp42x1this->id  = id;  // Handle.
    mcp42x1this->spi = spi; // SPI handle.
    mcp42x1this->cs  = cs;  // Chip Select of MCP42x1.
    mcp42x1this->wip = wip; // Resistor network.

    // Check this instance is unique.
    for ( i = 0; i < index; i++ )
        if ((( mcp42x1this->id  == mcp42x1[i]->id  ) ||
             ( mcp42x1this->spi == mcp42x1[i]->spi ) ||
             ( mcp42x1this->cs  == mcp42x1[i]->cs  )) &&
             ( mcp42x1this->wip == mcp42x1[i]->wip ))
            return MCP42X1_ERR_DUPLIC;

    mcp42x1[index] = mcp42x1this; // Copy into instance.

    index++;
    init = true;

    return mcp42x1this->id;
};
