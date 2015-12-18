/*
//  ===========================================================================

    testmcp23017:

    Tests MCP23017 driver for the Raspberry Pi.

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

#define Version "Version 0.1"

/*
//  ---------------------------------------------------------------------------

    Compile with:

    gcc testmcp23017.c mcp23017.c -Wall -o testmcp23017

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    17/12/2015  This program.

    Contributors:

    Changelog:

        v0.1    Original version.

//  Information. --------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander.

    For testing, LEDs were connected via a breadboard as follows:

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
             100R  LED  |------+-----+-----+------|  LED  100R
      GND<--/\/\/--|<|--| GPB0 |  01 | 28  | GPA7 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB1 |  02 | 27  | GPA6 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB2 |  03 | 26  | GPA5 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB3 |  04 | 25  | GPA4 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB4 |  05 | 24  | GPA3 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB5 |  06 | 23  | GPA2 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB6 |  07 | 22  | GPA1 |--|>|--/\/\/-->GND
      GND<--/\/\/--|<|--| GPB7 |  08 | 21  | GPA0 |--|>|--/\/\/-->GND
              +3.3V <---|  VDD |  09 | 20  | INTA |
                GND <---|  VSS |  10 | 19  | INTB |
                        |   NC |  11 | 18  | RST  |---> +3.3V.
            I2C CLK <---|  SCL |  12 | 17  | A2   |---> GND }
            I2C I/O <---|  SDA |  13 | 16  | A1   |---> GND } Address = 0x20.
                        |   NC |  14 | 15  | A0   |---> GND }
                        +-------------------------+

    The LEDs have a forward voltage and current of 1.8V and 20mA respectively
    so 100Ohm resistors are reasonable although 80Ohms is more ideal.

                    R = (3.3 - 1.8) / 20x10-3 = 75Ohm.

//  ---------------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include "mcp23017.h"

int main()
{

    int8_t id;  // Handle for MCP23017;
    // Initialise chip.
    id = mcp23017Init( 0x20, BITS_BYTE, MODE_BYTE );
    if ( id < 0 )
        {
            printf( "Couldn't init.\n" );
            return -1;
        };

    printf( "ID = %d.\n", id );


    uint8_t i;
    // Write a byte to light LEDs corresponding to byte value.
    for ( i = 0; i <= 0xff; i++ )
    {
        mcp23017WriteRegisterWord( id, OLATB, i );
        sleep( 1 );
    }
}
