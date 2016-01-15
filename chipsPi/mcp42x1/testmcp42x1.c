/*
//  ===========================================================================

    testmcp42x1:

    Tests MCP42x1 driver for the Raspberry Pi.

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
*/

#define TESTMCP42X1_VERSION 01.02

/*
//  ---------------------------------------------------------------------------

    Compile with:

    gcc testmcp42x1.c mcp42x1.c -Wall -o testmcp42x1 -lpigpio lpthread

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke            15/01/2016

    Contributors:

    Changelog:

        v01.00      Original version.
        v01.01      Modified init routine.
        v01.02      Modified test circuit.

//  Information. --------------------------------------------------------------

    The MCP42x1 is an SPI bus operated Dual 7/8-bit digital potentiometer with
    non-volatile memory.

    For testing, LEDs were connected via a breadboard as follows:

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
               CE0 <----| CS   |  01 | 14  |  VDD |-----------> +5V
             SCKL1 <----| SCK  |  02 | 13  |  SDO |----> MISO
              MOSI <----| SDI  |  03 | 12  | SHDN |
   GND <-;--------------| VSS  |  04 | 11  |   NC |--------------;-> GND
         |         R    | P1B  |  05 | 10  |  P0B |    R         |
         '--|<|--/\/\/--| P1W  |  06 | 09  |  P0W |--/\/\/--|>|--'
            //        ,-| P1A  |  07 | 08  |  P0A |-,        \\
            LED       | +-------------------------+ |       LED
                      |                             |
                      '-----------------------------'---------> +5V

        The LEDs have a forward voltage of 1.8V and a max current of 20mA,
        therefore a 160 Ohm resistance is ideal (for 5V VDD) for placing
        in series with it:

                    R = V / I.
                    R = (5 - 1.8) / 20x10-3 = 160 Ohms.

        However, the wiper resistance is already 75 Ohms so only an 85 Ohm
        resistor is needed. The closest I have is 75 Ohms, which seems fine.

        NC is not internally connected but can be externally connected to VDD
        or VSS to reduce noise coupling.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <pigpio.h>

#include "mcp42x1.h"

int main()
{

    uint8_t spi; // SPI handle.
    uint8_t device[MCP42X1_DEVICES * MCP42X1_WIPERS]; // MCP42x1 handles.

    /*
        Setting the SPI flags:

        +-----------------------------------------------------------------+
        |21|20|19|18|17|16|15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
        |-----------------+--+--+-----------+--+--+--+--+--+--+--+--+-----|
        | word size       | R| T| num bytes | W| A|u2|u1|u0|p2|p1|p0| mode|
        +-----------------------------------------------------------------+

        MCP42x1 can only operate in 0,0 or 1,1 mode; 0,0 is default.
        All other values can stay at default = 0.
        Therefore SPI flags = 0.
    */

    uint8_t  flags = 0; // SPI flags for default operation.
    uint16_t i;         // Loop counter.
    uint16_t data;      // Register data.

    printf( "Initialising.\n\n" );

    gpioInitialise(); // Initialise pigpio.
    printf( "GPIO initialised ok!\n" );

    spi = spiOpen( 0, MCP42X1_SPI_BAUD, flags ); // Initialise SPI for CS = 0.
    if ( spi < 0 ) return -1;
    printf( "SPI initialised ok!\n" );
    printf( "SPI handle = %d.\n", spi );

    // Initialise MCP42X1 twice, once for each wiper.
    mcp42x1Init( spi, 0 ); // Wiper 0.
    mcp42x1Init( spi, 1 ); // Wiper 1.

    // Check that devices have initialised.
    for ( i = 0; mcp42x1[i] != NULL; i++ )
        if ( device[i] < 0 ) return -1;
    printf( "Devices initialised ok!\n\n" );

    // Print properties for each device.
    printf( "Properties:\n\n" );
    for ( i = 0; (mcp42x1[i] != NULL); i++ )
    {
        printf( "Device %d.\n", i );
        printf( "SPI handle = %d,\n",   mcp42x1[i]->spi );
        printf( "Wiper      = 0d.\n", mcp42x1[i]->wiper );
        printf( "\n" );
    }

    //  Test reading status register. -----------------------------------------

    printf( "Reading Registers:\n\n" );
    data = mcp42x1ReadReg( 0, MCP42X1_REG_TCON );
    printf( "TCON register   = 0x%04x,\n", data );
    data = mcp42x1ReadReg( 0, MCP42X1_REG_STATUS );
    printf( "Status register = 0x%04x,\n", data );
    data = mcp42x1ReadReg( 0, MCP42X1_REG_WIPER0 );
    printf( "Wiper0 register = 0x%04x,\n", data );
    data = mcp42x1ReadReg( 0, MCP42X1_REG_WIPER1 );
    printf( "Wiper1 register = 0x%04x.\n", data );
    printf( "\n" );

    //  Test setting wiper values. --------------------------------------------

    //  Set wiper values.
    mcp42x1SetResistance ( mcp42x1[0]->spi, mcp42x1[0]->wiper, MCP42X1_RMAX );
    mcp42x1SetResistance ( mcp42x1[1]->spi, mcp42x1[1]->wiper, MCP42X1_RMIN );
    printf( "Set starting values.\n\n" );

    //  Make sure values have stuck.
    data = mcp42x1ReadReg( 0, MCP42X1_REG_WIPER0 );
    printf( "Wiper0 register = 0x%04x,\n", data );
    data = mcp42x1ReadReg( 0, MCP42X1_REG_WIPER1 );
    printf( "Wiper1 register = 0x%04x.\n", data );
    printf( "\n" );


    //  Test increment and decrement. -----------------------------------------

    printf( "Cycling wiper resistances.\n" );
    for ( ; ; ) // Continual loop.
    {
        for ( i = MCP42X1_RMIN; i < MCP42X1_RMAX; i++ )
        {
//            mcp42x1DecResistance( mcp42x1[0]->spi, mcp42x1[0]->wiper );
//            mcp42x1IncResistance( mcp42x1[1]->spi, mcp42x1[1]->wiper );
            mcp42x1SetResistance ( mcp42x1[0]->spi,
                                   mcp42x1[0]->wiper, i );
            mcp42x1SetResistance ( mcp42x1[1]->spi,
                                   mcp42x1[1]->wiper, MCP42X1_RMAX - i );
            gpioDelay( 10000 );
        }
        for ( i = MCP42X1_RMIN; i < MCP42X1_RMAX; i++ )
        {
//            mcp42x1IncResistance( mcp42x1[0]->spi, mcp42x1[0]->wiper );
//            mcp42x1DecResistance( mcp42x1[1]->spi, mcp42x1[1]->wiper );
            mcp42x1SetResistance ( mcp42x1[0]->spi,
                                   mcp42x1[0]->wiper, MCP42X1_RMAX - i );
            mcp42x1SetResistance ( mcp42x1[1]->spi,
                                   mcp42x1[1]->wiper, i );
            gpioDelay( 10000 );
        }
    }

    printf( "Finished.\n" );
    return 0;
}
