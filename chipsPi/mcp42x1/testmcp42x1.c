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

#define TESTMCP42X1_VERSION 01.00

/*
//  ---------------------------------------------------------------------------

    Compile with:

    gcc testmcp42x1.c mcp42x1.c -Wall -o testmcp42x1

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke            11/01/2016

    Contributors:

    Changelog:

        v01.00      Original version.

//  Information. --------------------------------------------------------------

    The MCP42x1 is an SPI bus operated Dual 7/8-bit digital potentiometer with
    non-volatile memory.

    For testing, LEDs were connected via a breadboard as follows:

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
               CE0 <----| CS   |  01 | 14  |  VDD |---> +5V
             SCKL1 <----| SCK  |  02 | 13  |  SDO |----> MISO
              MOSI <----| SDI  |  03 | 12  | SHDN |
               GND <----| VSS  |  04 | 11  |   NC |----> GND
                        | P1B  |  05 | 10  |  P0B |
     +5V <--------------| P1W  |  06 | 09  |  P0W |--------------> +5V
     GND <--/\/\/--|<|--| P1A  |  07 | 08  |  P0A |--|>|--/\/\/--> GND
             75R   //   +-------------------------+   \\    75R
                   LED                               LED

        The LEDs have a forward voltage and current of 1.8V and 20mA
        respectively so a 160Ohms resistance is ideal (for 5V VDD) for placing
        in series with it. However, the wiper resistance is 75Ohms so only an
        85Ohms resistor is needed. The closest I have is 75Ohms, which seems
        fine.

                    R = (5 - 1.8) / 20x10-3 = 160 Ohms.

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

    uint8_t  cs    = 0; // Chip select.
    uint8_t  flags = 0; // SPI flags.
    uint16_t i, j, k;   // Loop counters.

    // Initialise MCP42X1.
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

    printf( "Initialising.\n\n" );
    gpioInitialise();
    printf( "Wiper 0 SPI %d.\n", mcp42x1Init( cs, 0, flags )); // Wiper 0.
    printf( "Wiper 1 SPI %d.\n", mcp42x1Init( cs, 1, flags )); // Wiper 1.
    printf( "\n" );

    // Print properties for each device.
    printf( "Properties.\n\n" );

    // Make sure MCP42x1s have been initialised OK.
    for ( i = 0; (mcp42x1[i] != NULL); i++ )
    {
        printf( "\tDevice %d:\n", i );
        printf( "\tDevice handle = %d,\n", mcp42x1[i]->id  );
        printf( "\tSPI handle    = %d,\n", mcp42x1[i]->spi );
        printf( "\tChip select   = %d.\n", mcp42x1[i]->cs  );
        printf( "\tWiper         = %d.\n", mcp42x1[i]->wip );
        printf( "\n" );
    }

    //  Test reading status register. -----------------------------------------

    int16_t status = mcp42x1ReadReg( 0, MCP42X1_REG_STATUS );
    printf( "Status register = 0x%04x.\n", status );

    //  Test setting wiper values. --------------------------------------------

    printf( "Starting test. Press a key to continue." );
    getchar();

    //  Set wiper values to max (fully dimmed).
    mcp42x1SetResistance ( 0, MCP42X1_RMAX );
    printf( "Set to maximum.\n" );

    //  Cycle wiper values.
    for ( i = 0; i < 10; i++ )
    {
        printf( "Decreasing.\n" );
        for ( j = MCP42X1_RMAX; j > MCP42X1_RMIN; j-- )
        {
            mcp42x1DecResistance( 0 );
            for ( k = 0; k < 0xffff; k++ ); // Delay.
        }
        printf( "Increasing.\n" );
        for ( j = MCP42X1_RMIN; j < MCP42X1_RMAX; j++ )
        {
            mcp42x1IncResistance( 0 );
            for ( k = 0; k < 0xffff; k++ ); // Delay.
        }
    }

    return 0;
}
