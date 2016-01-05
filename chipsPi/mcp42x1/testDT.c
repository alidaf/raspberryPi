//  ===========================================================================
/*
    testDT:

    Tests parsing the device tree for the BCM2835 peripherals.

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
*/
//  ===========================================================================


#define Version 10000 // 1.0

//  ---------------------------------------------------------------------------
/*
    Compile with:

    gcc testDT.c mcp42x1.c -Wall -o testDT

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3
*/
//  ---------------------------------------------------------------------------
/*
    Authors:        D.Faulke    04/01/2016

    Contributors:

    Changelog:

        v0.1    Original version.
*/
//  Information. --------------------------------------------------------------
/*
    A device tree is a data structure for describing hardware, implemented as a
    tree of nodes, child nodes and properties.

    It is intended as a means to allow compilation of kernels that are fairly
    hardware agnostic within an architecture family, i.e. between different
    hardware revisions with slightly different peripherals. A significant part
    of the hardware description may be moced out of the kernel binary and into
    a compiled device tree blob, which is passed to the kernel bu the boot
    loader.

    This is especially important for ARM-based Linux distributions such as the
    Raspberry Pi where traditionally, the boot loader has been customised for
    specific boards, creating many variants of kernel need to be compiled for
    each board.

    The Raspberry Pi has a Broadcom BCM2835 or BCM2836 chip (depending on
    version) that provides peripheral support. The latest Raspbian kernel now
    has device tree support. This program experiments with the BCM2835 device
    tree to test parsing and information retrieval to avoid typically using
    lots of unwieldly macro definitions.

    Device trees are written as .dts files in textual form and compiled into
    a flattened device tree or device tree blob (.dtb) file. Additional trees
    with the .dtsi extension may be referenceded via the /include/ card and are
    analogous to .h header files in C.

    For the Raspberry Pi, device trees should be located in /arc/arm/boot/dts/,
    e.g. /arc/arm/boot/dts/bcm2835.dts.

*/
//  ---------------------------------------------------------------------------


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>


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
