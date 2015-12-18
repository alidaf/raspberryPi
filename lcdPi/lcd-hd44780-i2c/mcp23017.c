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

    Authors:        D.Faulke    17/12/2015.

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
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "mcp23017.h"

//  Data structures. ----------------------------------------------------------

uint8_t mcp23017Register[MCP23017_REGISTERS][MCP23017_BANKS] =
/*
    Register address can be reference with enumerated type
         {          BANK0, BANK1          }
*/
        {{   BANK0_IODIRA, BANK1_IODIRA   },
         {   BANK0_IODIRB, BANK1_IODIRB   },
         {    BANK0_IPOLA, BANK1_IPOLA    },
         {    BANK0_IPOLB, BANK1_IPOLB    },
         { BANK0_GPINTENA, BANK1_GPINTENA },
         { BANK0_GPINTENB, BANK1_GPINTENB },
         {  BANK0_DEFVALA, BANK1_DEFVALA  },
         {  BANK0_DEFVALB, BANK1_DEFVALB  },
         {  BANK0_INTCONA, BANK1_INTCONA  },
         {  BANK0_INTCONB, BANK1_INTCONB  },
         {   BANK0_IOCONA, BANK1_IOCONA   },
         {   BANK0_IOCONB, BANK1_IOCONB   },
         {    BANK0_GPPUA, BANK1_GPPUA    },
         {    BANK0_GPPUB, BANK1_GPPUB    },
         {    BANK0_INTFA, BANK1_INTFA    },
         {    BANK0_INTFB, BANK1_INTFB    },
         {  BANK0_INTCAPA, BANK1_INTCAPA  },
         {  BANK0_INTCAPB, BANK1_INTCAPB  },
         {    BANK0_GPIOA, BANK1_GPIOA    },
         {    BANK0_GPIOB, BANK1_GPIOB    },
         {    BANK0_OLATA, BANK1_OLATA    },
         {    BANK0_OLATB, BANK1_OLATB    }};

//  MCP23017 functions. -------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017WriteRegisterByte( struct mcp23017_s *mcp23017,
                                  uint8_t reg, uint8_t data )
{
    uint8_t handle = mcp23017->id;
    uint8_t bank = mcp23017->bank;
    uint8_t addr = mcp23017Register[reg][bank];
    return i2c_smbus_write_byte_data( handle, addr, data );
}

//  ---------------------------------------------------------------------------
//  Writes word to register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017WriteRegisterWord( struct mcp23017_s *mcp23017,
                                  uint8_t reg, uint16_t data )
{
    uint8_t handle = mcp23017->id;
    uint8_t bank = mcp23017->bank;
    uint8_t addr = mcp23017Register[reg][bank];
    return i2c_smbus_write_word_data( handle, addr, data );
}

//  ---------------------------------------------------------------------------
//  Reads byte from register of MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017ReadRegisterByte( struct mcp23017_s *mcp23017, uint8_t reg )
{
    uint8_t handle = mcp23017->id;
    uint8_t bank = mcp23017->bank;
    uint8_t addr = mcp23017Register[reg][bank];
    return i2c_smbus_read_byte_data( handle, addr );
}

//  ---------------------------------------------------------------------------
//  Reads word from register of MCP23017.
//  ---------------------------------------------------------------------------
int16_t mcp23017ReadRegisterWord( struct mcp23017_s *mcp23017, uint8_t reg )
{
    uint8_t handle = mcp23017->id;
    uint8_t bank = mcp23017->bank;
    uint8_t addr = mcp23017Register[reg][bank];
    return i2c_smbus_read_word_data( handle, addr );
}

//  ---------------------------------------------------------------------------
//  Initialises MCP23017. Call for each MCP23017.
//  ---------------------------------------------------------------------------
int8_t mcp23017Init( uint8_t addr )
{
    struct mcp23017_s *mcp23017this;  // current MCP23017.
    static bool init = false;         // 1st call.
    static uint8_t index = 0;

    int8_t  id = -1;
    uint8_t i;

    // Set all intances of mcp23017 to NULL on first call.
    if ( !init )
        for ( i = 0; i < MCP23017_MAX; i++ )
            mcp23017[i] = NULL;

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
/*
    Note: I2C file system path for revision 1 is "/dev/i2c-0".
*/
    static const char *i2cDevice = "/dev/i2c-1"; // Path to I2C file system.

    if (!init)
    {
        // I2C communication is via device file (/dev/i2c-1).
        if (( id = open( i2cDevice, O_RDWR )) < 0 )
        {
            printf( "Couldn't open I2C device %s.\n", i2cDevice );
            printf( "Error code = %d.\n", errno );
            return -1;
        }

        printf( "Opened I2C device OK.\n" );
        init = true;
    }

    // Create an instance of this device.
    mcp23017this->id = id;          // I2C handle.
    mcp23017this->addr = addr;      // Address of MCP23017.
    mcp23017this->bank = 0;         // BANK mode 0 (default).
    mcp23017[index] = mcp23017this; // Copy into instance.
    index++;                        // Increment index for next MCP23017.

    // Set slave address for this device.
    if ( ioctl( mcp23017this->id, I2C_SLAVE, mcp23017this->addr ) < 0 )
    {
        printf( "Couldn't set slave address 0x%02x.\n", mcp23017this->addr );
        printf( "Error code = %d.\n", errno );
        return -1;
    }

    return id;
};

