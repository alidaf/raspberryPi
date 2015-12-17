/*
//  ===========================================================================

    mcp23017:

    Driver for the MCP23017 port expander.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on the following guides and codes:
        MCP23017 data sheet.
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/21952b.pdf.

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

        gcc -c -Wall -fpic mcp23017.c
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
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

//#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "mcp23017.h"

//  I2C functions. ------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Opens I2C device, returns handle.
//  ---------------------------------------------------------------------------
int8_t i2cOpen( uint8_t id )
{
    // I2C communication is via device file (/dev/i2c-1).
    if (( id = open( i2cDevice, O_RDWR )) < 0 )
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
//int8_t i2cClose( void )

//    if ( close( i2cDevice ) < 0 )
//    {
//        printf( "Couldn't close I2C device %s.\n", i2cDevice );
//        printf( "Error code = %d.\n", errno );
//        return -1;
//    }
//};

//  MCP23017 functions. -------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017WriteRegisterByte( uint8_t handle, uint8_t reg, uint8_t data )
{
    return i2c_smbus_write_byte_data( handle, reg, data );
}

//  ---------------------------------------------------------------------------
//  Writes word to register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017WriteRegisterWord( uint8_t handle, uint8_t reg, uint8_t data )
{
    return i2c_smbus_write_word_data( handle, reg, data );
}

//  ---------------------------------------------------------------------------
//  Reads byte from register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017ReadRegisterByte( uint8_t handle, uint8_t reg )
{
    return i2c_smbus_read_byte_data( handle, reg );
}

//  ---------------------------------------------------------------------------
//  Reads word from register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017ReadRegisterWord( uint8_t handle, uint8_t reg )
{
    return i2c_smbus_read_word_data( handle, reg );
}

//  ---------------------------------------------------------------------------
//  Initialises MCP23017 registers.
//  ---------------------------------------------------------------------------
int8_t mcp23017Init( uint8_t addr, mcp23017Bits_t bits, mcp23017Mode_t mode )
{
    struct mcp23017_s *mcp23017this;  // current MCP23017.
    static bool init = false;         // 1st call.

    int8_t  id = -1;
    uint8_t i;

    // Set all intances of mcp23017 to NULL on first call.
    if ( !init )
    {
//        init = true;
        for ( i = 0; i < MCP23017_MAX; i++ )
            mcp23017[i] = NULL;
    }

    // Address must be 0x20 to 0x27.
    if (( addr < 0x20 ) || ( addr > 0x27 )) return -1;

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
    mcp23017this = malloc( sizeof ( struct mcp23017_s ));

    // Return if unable to allocate memory.
    if ( mcp23017this == NULL ) return -1;

    if (!init)
    {
        // I2C communication is via device file (/dev/i2c-1).
        if (( id = open( i2cDevice, O_RDWR )) < 0 )
        {
            printf( "Couldn't open I2C device %s.\n", i2cDevice );
            printf( "Error code = %d.\n", errno );
            return -1;
        }

        init = true;
    }

    mcp23017this->addr = addr;    // Set address.
    mcp23017this->bits = bits;    // Set 8-bit or 16-bit mode.
    mcp23017this->mode = mode;    // Set byte or sequential R/W mode.
    mcp23017[id] = mcp23017this;  // Create an instance of this device.

    // Set slave address for this device.
    if ( ioctl( id, I2C_SLAVE, mcp23017this->addr ) < 0 )
    {
        printf( "Couldn't set slave address 0x%02x.\n", mcp23017this->addr );
        printf( "Error code = %d.\n", errno );
        return -1;
    }

    // Set registers.

    // Need to set up mode and bits!

    // Use mcp23017Reg_t as index to get address for port.
    mcp23017Reg_t index;
    uint8_t port = 0;

    // Set directions to out (PORTA).
    index = IODIRA;
    mcp23017WriteRegisterByte( id, mcp23017Register[index][port], 0x00 );
    // Set directions to out (PORTB).
    index = IODIRB;
    mcp23017WriteRegisterByte( id, mcp23017Register[index][port], 0x00 );
    // Set all outputs to low (PORTA).
    index = OLATA;
    mcp23017WriteRegisterByte( id, mcp23017Register[index][port], 0x00 );
    // Set all outputs to low (PORTB).
    index = OLATB;
    mcp23017WriteRegisterByte( id, mcp23017Register[index][port], 0x00 );

    return id;
};

