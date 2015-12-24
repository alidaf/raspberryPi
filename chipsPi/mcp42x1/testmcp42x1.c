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

#define Version "Version 0.1"

/*
//  ---------------------------------------------------------------------------

    Compile with:

    gcc testmcp42x1.c mcp42x1.c -Wall -o testmcp42x1

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

//  ---------------------------------------------------------------------------

    Authors:        D.Faulke    24/12/2015

    Contributors:

    Changelog:

        v0.1    Original version.

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
     +5V <--------------| P1W  |  06 | 09  |  P0W |-------> +5V
     GND <--/\/\/--|<|--| P1A  |  07 | 08  |  P0A |--|>|--/\/\/--> GND
             75R        +-------------------------+        75R

        The LEDs have a forward voltage and current of 1.8V and 20mA
        respectively so a 160Ohms resistance is ideal (for 5V VDD) for placing
        in series with it. However, the wiper resistance is 75Ohms so only an
        85Ohms resistor is needed. The closest I have is 75Ohms, which seems
        fine.

                    R = (5 - 1.8) / 20x10-3 = 160 Ohms.

        NC is not internally connected but can be externally connected to VDD
        or VSS to reduce noise coupling.

//  Information. --------------------------------------------------------------

    The MCP23017 is an I2C bus operated 16-bit I/O port expander.


                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
             100R  LED  |------+-----+-----+------|
        .---/\/\/--|<|--| GPB0 |  01 | 28  | GPA7 |---/ ---.
        |---/\/\/--|<|--| GPB1 |  02 | 27  | GPA6 |---/ ---|
        |---/\/\/--|<|--| GPB2 |  03 | 26  | GPA5 |---/ ---|
        |---/\/\/--|<|--| GPB3 |  04 | 25  | GPA4 |---/ ---|  8 port
        |---/\/\/--|<|--| GPB4 |  05 | 24  | GPA3 |---/ ---| dip switch
        |---/\/\/--|<|--| GPB5 |  06 | 23  | GPA2 |---/ ---|
        |---/\/\/--|<|--| GPB6 |  07 | 22  | GPA1 |---/ ---|
        |---/\/\/--|<|--| GPB7 |  08 | 21  | GPA0 |---/ ---|
        |     +3.3V <---|  VDD |  09 | 20  | INTA |        |
 GND <--'---------------|  VSS |  10 | 19  | INTB |        |
                        |   NC |  11 | 18  | RST  |--------'----> +3.3V.
            I2C CLK <---|  SCL |  12 | 17  | A2   |---> GND }
            I2C I/O <---|  SDA |  13 | 16  | A1   |---> GND } Address = 0x20.
                        |   NC |  14 | 15  | A0   |---> GND }
                        +-------------------------+


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

    int8_t err = 0; // Return code.
    int8_t num = 1; // Number of MCP23017 devices.

    // Initialise MCP23017. Only using 1 but should work for up to 8.
    err = mcp23017Init( 0x20 );
    if ( err < 0 )
    {
        printf( "Couldn't init.\n" );
        return -1;
    }

    // Print properties for each device.
    printf( "Properties.\n" );

    uint8_t i; // Loop counter.

    for ( i = 0; i < num; i++ )
    {
        // Start off with BANK = 0.
        mcp23017[i]->bank = 0;
        mcp23017WriteByte( mcp23017[i], IOCONA, 0x00 );

        // Make sure MCP23017s have been initialised OK.
        printf( "\tDevice %d:\n", i );
        printf( "\tHandle = %d,\n", mcp23017[i]->id );
        printf( "\tAddress = 0x%02x,\n", mcp23017[i]->addr );
        printf( "\tBank mode = %1d.\n", mcp23017[i]->bank );

        // Set direction of GPIOs and clear latches.
        mcp23017WriteByte( mcp23017[i], IODIRA, 0xff ); // Input.
        mcp23017WriteByte( mcp23017[i], IODIRB, 0x00 ); // Output.

        // Writes to latches are the same as writes to GPIOs.
        mcp23017WriteByte( mcp23017[i], OLATA, 0x00 ); // Clear pins.
        mcp23017WriteByte( mcp23017[i], OLATB, 0x00 ); // Clear pins.

    }
    printf( "\n" );

    //  Test setting BANK modes: ----------------------------------------------

    printf( "Testing writes in both BANK modes.\n" );

    uint8_t j, k;   // Loop counters.

    for ( i = 0; i < num; i++ ) // For each MCP23017.
    {
        printf( "MCP23017 %d:\n", i );

        for ( j = 0; j < 2; j++ ) // Toggle BANK bit twice.
        {
            printf( "\tBANK %d.\n", mcp23017[i]->bank );

            for ( k = 0; k < 0xff; k++ ) // Write 0x00 to 0xff.
            {
                mcp23017WriteByte( mcp23017[i], OLATB, k );
                usleep( 50000 );
            }

            // Reset all LEDs.
            mcp23017WriteByte( mcp23017[i], OLATB, 0x00 );

            // Toggle BANK bit.
            if ( mcp23017[i]->bank == 0 )
                mcp23017WriteByte( mcp23017[i], IOCONA, 0x80 );
            else
                mcp23017WriteByte( mcp23017[i], IOCONA, 0x00 );
            mcp23017[i]->bank = !mcp23017[i]->bank;
        }

        // Next MCP23017.
        printf( "\n" );
    }

    //  Test read & check bits functions. -------------------------------------

    printf( "Testing read and bit checks.\n" );

    // Check value of GPIO register set as inputs (PORT A).
    uint8_t data;
    bool match = false;
    for ( i = 0; i < num; i++ )
    {
        printf( "MCP23017 %d:\n", i );

        data = mcp23017ReadByte( mcp23017[i], GPIOA );
        mcp23017WriteByte( mcp23017[i], OLATB, data );
        printf( "\tGPIOA = 0x%02x, checking...", data );
        for ( j = 0; j < 0xff; j++ )
        {
            match = mcp23017CheckBitsByte( mcp23017[i], GPIOA, j );
            if ( match ) printf( "matched to 0x%02x.\n", j );
            match = false;
        }
        // Next MCP23017.
        printf( "\n" );
    }

    //  Continuously loop through the next tests. -----------------------------

    while ( 1 )
    {

        //  Test toggle bits function.

        printf( "Testing toggle bits.\n" );

        for ( i = 0; i < num; i++ )
        {
            printf( "MCP23017 %d:\n", i );

            printf( "\tAlternating bits.\n" );
            for ( j = 0; j < 10; j++ )
            {
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0x55 );
                usleep( 100000 );
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0x55 );
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0xaa );
                usleep( 100000 );
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0xaa );
            }

            printf( "\tAlternating nibbles.\n" );
            for ( j = 0; j < 10; j++ )
            {
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0x0f );
                usleep( 100000 );
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0x0f );
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0xf0 );
                usleep( 100000 );
                mcp23017ToggleBitsByte( mcp23017[i], OLATB, 0xf0 );
            }
            // Next MCP23017.
            printf( "\n" );
        }

        //  Test set bits function. -------------------------------------------

        printf( "Testing set and clear bits.\n" );

        // Set all LEDs off and light in sequence.
        uint8_t setBits;
        uint8_t clearBits;
        for ( i = 0; i < num; i++ )
        {
            printf( "MCP23017 %d:\n", i );

            printf( "Setting and clearing bits 0 - 7 in sequence.\n" );
            for ( j = 0; j < 10; j++ ) // Sequence through LEDs 10x.
            {
                mcp23017WriteByte( mcp23017[i], GPIOB, 0x00 );
                usleep( 50000 );
                for ( k = 0; k < 8; k++ )
                {
                    setBits = 1 << k;
                    mcp23017SetBitsByte( mcp23017[i], GPIOB, setBits );
                    usleep( 50000 );
                }
                usleep( 50000 );
                for ( k = 0; k < 8; k++ )
                {
                    clearBits = 1 << k;
                    mcp23017ClearBitsByte( mcp23017[i], GPIOB, clearBits );
                    usleep( 50000 );
                }
            }

            // Next MCP23017.
            printf( "\n" );
        }

    }

    //  End of tests. ---------------------------------------------------------

    return 0;
}
