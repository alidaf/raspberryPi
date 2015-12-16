/*
//  ===========================================================================

    mcp23017:

    Driver for the MCP23017 port expander.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on the following guides and codes:
        MCP23017 data sheet.
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/21952b.pdf.
        Experimenting with Raspberry Pi by Warren Gay
        - ISBN13: 978-1-484201-82-4.

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

    For a shared library, compile with:

        gcc -c -Wall -Werror -fpic mcp23017.c
        gcc -shared -o libmcp23017.so mcp23017.o

    For Raspberry Pi optimisation use the following flags:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    16/12/2015.

    Contributors:

    Changelog:

        v0.1    Original version.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "mcp23017.h"

//  MCP23017 functions. -------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
//  ---------------------------------------------------------------------------
static int8_t mcp23017WriteByte( uint8_t handle, uint8_t reg, uint8_t byte )
{
    uint8_t i;
    uint8_t data = byte;

    printf( "I2C handle = %d, MCP23017 register = 0x%02x.\n", handle, reg );
    printf( "RS EN RW NC DB DB DB DB\n" );
    for ( i = 0; i < BITS_BYTE; i++ )
        printf( "%2d ", ( data >> BITS_BYTE - i - 1 ) & 1 );
    printf( "\n" );
    i2c_smbus_write_byte_data( handle, reg, byte );
}


//  ---------------------------------------------------------------------------
//  Opens I2C device.
//  ---------------------------------------------------------------------------
int8_t i2cOpen( void )
{
    int8_t id;
    // I2C communication is via device file (/dev/i2c-1).
    if ( id = open( i2cDevice, O_RDWR )) < 0 )
    {
        printf( "Couldn't open I2C device %s.\n", i2cDevice );
        printf( "Error code = %d.\n", errno );
        return -1;
    }

    return id;
};

//  ---------------------------------------------------------------------------
//  Closes I2C device.
//  ---------------------------------------------------------------------------
int8_t i2cClose( void )

    if ( close( i2cDevice )) < 0
        printf( "Couldn't close I2C device %s.\n", i2cDevice );
        printf( "Error code = %d.\n", errno );
        return -1;
    }
};

//  ---------------------------------------------------------------------------
//  Initialises MCP23017 registers.
//  ---------------------------------------------------------------------------
int8_t mcp23017_init( uint8_t addr, mcp23017_bits bits, mcp23017_mode mode )
{
    struct Mcp23017 *mcp23017this;  // current MCP23017.
    static bool init = false;           // 1st call.

    int8_t  id = -1;
    uint8_t i;

    // Set all intances of mcp23017 to NULL on first call.
    if ( !init )
    {
        init = true;
        for ( i = 0; i < MCP23017_MAX; i++ )
            mcp23017[i] = NULL;
    }

    // Address must be 0x20 to 0x27.
    if (( addr < 0x20 ) || ( addr > 0x27 )) return -1.

    // Get next available ID.
    for ( i = 0; i < MCP23017_MAX; i++ )
	{
        if ( mcp23017[i] == NULL )  // If not already init.
        {
            id = i;                 // Next available id.
            i = MCP23017_MAX;       // Break.
        }
    }

    if ( id < 0 ) return -1;        // Return if not init.

    // Allocate memory for MCP23017 data structure.
    mcp23017this = malloc( sizeof ( struct Mcp23017 ));

    // Return if unable to allocate memory.
    if ( mcp23017this == NULL ) return -1;

    mcp23017this->addr = address;   // Set address.
    mcp23017this->bits = bits;      // Set 8-bit or 16-bit mode.
    mcp23017this->mode = mode;      // Set byte or sequential R/W mode.
    mcp23017[id] = mcp23017this;    // Create an instance of this device.

    // Set slave address for this device.
    if ( ioctl( id, I2C_SLAVE, mcp23017this->addr ) < 0 )
    {
        printf( "Couldn't set slave address 0x%02x.\n", mcp23017this->addr );
        printf( "Error code = %d.\n", errno );
        return -1;
    }

    // Set registers.

    // Need to set up mode and bits!


    printf( "\nSetting up MCP23017.\n\n" );
    // Set directions to out (PORTA).
    mcp23017WriteByte( mcp23017ID[i], MCP23017_0_IODIRA, 0x00 );
    // Set directions to out (PORTB).
    mcp23017WriteByte( mcp23017ID[i], MCP23017_0_IODIRB, 0x00 );
    // Set all outputs to low (PORTA).
    mcp23017WriteByte( mcp23017ID[i], MCP23017_0_OLATA, 0x00 );
    // Set all outputs to low (PORTB).
    mcp23017WriteByte( mcp23017ID[i], MCP23017_0_OLATB, 0x00 );

    return id;
};

