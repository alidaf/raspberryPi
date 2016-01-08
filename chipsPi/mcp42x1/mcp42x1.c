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
    Authors:        D.Faulke    08/01/2016

    Contributors:

*/
//  ===========================================================================

//  Libraries.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pigpio.h>

//  Companion header.
#include "mcp42x1.h"


//  MCP42x1 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns code version.
//  ---------------------------------------------------------------------------
uint8_t mcp42x1_version( void )
{
    return MCP42X1_VERSION;
}

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1WriteByte( struct mcp42x1 *mcp42x1,
                         uint8_t reg, uint8_t data )
{
    // Should work with IOCON.BANK = 0 or IOCON.BANK = 1 for PORT A and B.
    uint8_t handle = mcp42x1->id;

    // Get register address for BANK mode.
    uint8_t addr = mcp42x1Register[reg][bank];

    // Write byte into register.
    return i2c_smbus_write_byte_data( handle, addr, data );
}

//  ---------------------------------------------------------------------------
//  Reads byte from register of MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1ReadByte( struct mcp42x1 *mcp42x1, uint8_t reg )
{
    // Should work with IOCON.BANK = 0 or IOCON.BANK = 1 for PORT A and B.
    uint8_t handle = mcp42x1->id;

    // Get register address for BANK mode.
    uint8_t addr = mcp42x1Register[reg][bank];

    // Return register value.
    return i2c_smbus_read_byte_data( handle, addr );
}

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t device, uint8_t mode, uint8_t bits, uint16_t divider );
{
    struct mcp42x1 *mcp42x1this; // MCP42x1 instance.
    static bool init = false;    // 1st call.
    static uint8_t index = 0;

    int8_t  id = -1;
    uint8_t i;

    // Set all intances of mcp23017 to NULL on first call.
    if ( !init )
        for ( i = 0; i < MCP42X1_MAX; i++ )
            mcp42x1[i] = NULL;

    // Chip Select must be 0 or 1.

    static const char *spiDevice; // Path to SPI file system.
    switch ( device )
    {
        case 1:
            spiDevice = "/dev/spidev0.1";
            break;
        default:
            spiDevice = "/dev/spidev0.0";
            break;
    }


    // Get next available ID.
    for ( i = 0; i < MCP42X1_MAX; i++ )
	{
        if ( mcp42x1[i] == NULL )  // If not already init.
        {
            id = i;                // Next available id.
            i = MCP42X1_MAX;       // Break.
        }
    }

    if ( id < 0 ) return -1;        // Return if not init.

    // Allocate memory for MCP23017 data structure.
    mcp42x1this = malloc( sizeof ( struct mcp42x1 ));

    // Return if unable to allocate memory.
    if ( mcp42x1this == NULL ) return -1;

    if (!init)
    {
        // I2C communication is via device file (/dev/i2c-1).
        if (( id = open( spiDevice, O_RDWR )) < 0 )
        {
            printf( "Couldn't open SPI device %s.\n", spiDevice );
            printf( "Error code = %d.\n", errno );
            return -1;
        }

        init = true;
    }

    // Create an instance of this device.
    mcp42x1this->id = id; // SPI handle.
    mcp42x1this->device = device; // Chip Select of MCP42x1.
    mcp42x1[index] = mcp42x1this; // Copy into instance.
    index++;                        // Increment index for next MCP23017.

    uint8_t mode

    // Set slave address for this device.
    if ( ioctl( mcp42x1this->id, I2C_SLAVE, mcp42x1this->addr ) < 0 )
    {
        printf( "Couldn't set slave address 0x%02x.\n", mcp42x1this->addr );
        printf( "Error code = %d.\n", errno );
        return -1;
    }

    return id;
};

