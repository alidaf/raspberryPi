//  ===========================================================================
/*
    mcp42x1:

    Driver for the MCP42x1 SPI digital potentiometer.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>

    Based on the MCP42x1 data sheet:
        - see http://ww1.microchip.com/downloads/en/DeviceDoc/22059b.pdf.

    SPI code originally based on the following sources:
        Raspberry Pi Hardware Reference by Warren Gay
        - ISBN-10: 1484208005
        - ISBN-13: 978-1484208007

        Broadcom BCM2835 ARM Peripherals, Reference C6357-M-1398.
        - see https://www.raspberrypi.org/wp-content/uploads/2012/02/
              BCM2835-ARM-Peripherals.pdf

    Much of the direct access to the BCM2835 registers is based on the BCM2835
    library by Mike McCauley.
        - see http://www.airspayce.com/mikem/bcm2835/

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

        gcc -c -Wall -fpic mcp42x1.c
        gcc -shared -o libmcp42x1.so mcp42x1.o

    For Raspberry Pi optimisation use the following flags:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

*/
//  ===========================================================================
/*
    Authors:        D.Faulke    05/01/2016

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

//  Companion header.
#include "mcp42x1.h"


//  BCM2835 data. -------------------------------------------------------------
/*
    Register base address for SPI peripheral.
    Note: MAP_FAILED is defined in sys/mman.h as ((void*)-1).
          It is the mmap equivalent of NULL.
*/

//  Physical address of the mapped peripherals block.
uint32_t *bcm2835_peripherals_base = (uint32_t *)BCM2835_PERI_BASE;
uint32_t *bcm2835_peripherals_size = (uint32_t *)BCM2835_PERI_SIZE;

//  Virtual memory address of the mapped peripherals block.
uint32_t *bcm2835_peripherals = (uint32_t *)MAP_FAILED;

//  Register base addresses for peripherals.
volatile uint32_t *bcm2835_gpio (uint32_t *)MAP_FAILED; // GPIOs.
volatile uint32_t *bcm2835_clk  (uint32_t *)MAP_FAILED; // Clock.
volatile uint32_t *bcm2835_spi0 (uint32_t *)MAP_FAILED; // SPI0.


//  BCM2835 functions. --------------------------------------------------------

/*
    Note on reading/writing BCM2835 registers:

    According to section 1.3 of the BCM2835 ARM peripherals manual, access to
    the same peripheral will always arrive and return in order. Only when
    switching from one peripheral to another can data arrive out of order. The
    simplest solution to ensure that data is processed in order is to place a
    memory barrier instruction at critical sections in the code, i.e.:

    {}  A memory write barrier should be placed Before the first write to a
        peripheral.
    {}  A memory read barrier should be placed after the last read of a
        peripheral.

    Memory barrier instructions are only needed where it is possible that a
    read or write may be followed by a read or write of a different peripheral.

    Similar precautions should be used with interrupt routines that can
    happen at any point in the code.
*/

//  ---------------------------------------------------------------------------
//  Returns 32-bit reading of a peripheral register with memory barriers.
//  ---------------------------------------------------------------------------
uint32_t bcm2835_peripheral_read( volatile uint32_t *addr )
{
    uint32_t data;

    __sync_synchronize(); // Memory barrier.
    data = *addr;
    __sync_synchronize(); // Memory barrier.

    return data;
}

//  ---------------------------------------------------------------------------
//  Returns 32-bit reading of a peripheral register without memory barriers.
//  ---------------------------------------------------------------------------
uint32_t bcm2835_peripheral_read_no_barrier( volatile uint32_t *addr )
{
    return *addr;
}

//  ---------------------------------------------------------------------------
//  Writes 32-bit data to a peripheral register with memory barriers.
//  ---------------------------------------------------------------------------
void bcm2835_peripheral_write( volatile uint32_t *addr, uint32_t data )
{
    __sync_synchronize(); // Memory barrier.
    *addr = data;
    __sync_synchronize(); // Memory barrier.
}

//  ---------------------------------------------------------------------------
//  Writes 32-bit data to a peripheral register without memory barriers.
//  ---------------------------------------------------------------------------
void bcm2835_peripheral_write_no_barrier( volatile uint32_t *addr,
                                                   uint32_t data )
{
    *addr = data;
}

//  ---------------------------------------------------------------------------
//  Sets specific (masked) bits in a peripheral register.
//  ---------------------------------------------------------------------------
void bcm2835_peri_set_bits( volatile uint32_t *addr,
                                     uint32_t data,
                                     uint32_t mask )
{
    uint32_t temp = bcm2835_peri_read( addr );
    temp = ( temp & ~mask ) | ( data & mask );
    bcm2835_peri_write( addr, temp );
}

//  ---------------------------------------------------------------------------
//  Sets BCM2835 GPIO function.
//  ---------------------------------------------------------------------------
void bcm2835_gpio_fsel( uint8_t gpio, uint8_t mode )
/*
    There are 6 GPFSEL (GPIO Function Select) registers that control blocks of
    10 GPIOs by setting 3 bits for each, i.e. 30 bits in total. The remaining 2
    bits of each register are reserved, e.g.

            +--------------------------------------------------------+
            |       |        GPIO numbers for GPFSEL{n}        |     |
            | bits  |------------+-----------------------------| off |
            |       | Mask / n = |  0 |  1 |  2 |  3 |  4 |  5 |     |
            |-------+------------+----+----+----+----+----+----+-----|
            | 00-02 | 0x00000007 | 00 | 10 | 20 | 30 | 40 | 50 |  0  |
            | 03-05 | 0x00000038 | 01 | 11 | 21 | 31 | 41 | 51 |  1  |
            | 06-08 | 0x000001c0 | 02 | 12 | 22 | 32 | 42 | 52 |  2  |
            | 09-11 | 0x00000e00 | 03 | 13 | 23 | 33 | 43 | 53 |  3  |
            | 12-14 | 0x00007000 | 04 | 14 | 24 | 34 | 44 | 54 |  4  |
            | 15-17 | 0x00038000 | 05 | 15 | 25 | 35 | 45 | 55 |  5  |
            | 18-20 | 0x001c0000 | 06 | 16 | 26 | 36 | 46 | 56 |  6  |
            | 21-23 | 0x00e00000 | 07 | 17 | 27 | 37 | 47 | 57 |  7  |
            | 24-26 | 0x07000000 | 08 | 18 | 28 | 38 | 48 | 58 |  8  |
            | 27-29 | 0x38000000 | 09 | 19 | 29 | 39 | 49 | 59 |  9  |
            | 30-31 | 0xc0000000 | -- | -- | -- | -- | -- | -- |  -  |
            +--------------------------------------------------------+

        There is a thread discussing the implementation of this function here..

        https://groups.google.com/forum/#!msg/bcm2835/L-6miubsU0w/5qUpuQIUCgAJ

            shift = ( gpio % 10 ) * 3.  // Number of logical shifts.
            mask = 0x07 << shift.       // Bit-mask is 0x07 << shift.
            data = bits << shift.       // FSEL bits << shift.

            e.g. data = b001 = 0x1

                +--------------------------------------------+
                | gpio | quo | mod | mask       | data       |
                |------+-----+-----+------------+------------|
                |  15  |  1  |  5  | 0x00038000 | 0x00008000 |
                |  19  |  1  |  9  | 0x38000000 | 0x00000000 |
                |  21  |  2  |  1  | 0x00000038 | 0x00000008 |
                |  30  |  3  |  0  | 0x00000007 | 0x00000001 |
                |  32  |  3  |  2  | 0x000001c0 | 0x00000040 |
                |  37  |  3  |  7  | 0x00e00000 | 0x00200000 |
                |  42  |  4  |  4  | 0x000001c0 | 0x00000040 |
                |  43  |  4  |  4  | 0x00000e00 | 0x00000200 |
                |  54  |  5  |  5  | 0x00007000 | 0x00001000 |
                +--------------------------------------------+

        The calculation of paddr is described by...

        bcm2835_gpio is a pointer to the 32-bit address of the GPIO register.
        We need to calculate the address of the GPFSEL register for a
        particular GPIO. This means incrementing the pointer for each GPFSEL
        register and not the address itself!

        +--------------------------------------------+
        | Register             | Offset | Address    |
        |----------------------|--------|------------|
        | GPIO base            | 0x0000 | 0x20000000 | <- bcm2835_gpio
        | ...                  | ...    | ...        | }
        | ...                  | ...    | ...        | } n = 0
        | ...                  | ...    | ...        | }
        | GPFSEL0 (GPIOs 00-09 | 0x0000 | 0x20000000 | <- bcm2835_gpio + n
        | GPFSEL1 (GPIOs 10-19 | 0x0004 | 0x20000004 | <- bcm2835_gpio + n + 1
        | GPFSEL2 (GPIOs 20-29 | 0x0008 | 0x20000008 | <- bcm2835_gpio + n + 2
        | GPFSEL3 (GPIOs 30-39 | 0x000c | 0x2000000c | <- bcm2835_gpio + n + 3
        | GPFSEL4 (GPIOs 40-49 | 0x0010 | 0x20000010 | <- bcm2835_gpio + n + 4
        | GPFSEL5 (GPIOs 50-59 | 0x0014 | 0x20000014 | <- bcm2835_gpio + n + 5
        +--------------------------------------------+

            Assuming the GPFSEL register offsets are...
            gpfsel[6] = { 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c }
            i.e. n = 2

            quo is the quotient ( gpio / 10 ).
            paddr = *bcm2835_gpio + BCM2835_GPFSEL0/4 + (pin/10)
        =>  paddr = *bcm2835_gpio + 2 + (pin/10)

                        +---------------------------+
                        | gpio | quo | inc | offset |
                        |------+-----+-----+--------|
                        |  15  |  1  |  3  | 0x000c |
                        |  19  |  1  |  3  | 0x000c |
                        |  21  |  2  |  4  | 0x0010 |
                        |  30  |  3  |  5  | 0x0010 |
                        |  32  |  3  |  5  | 0x0010 |
                        |  37  |  3  |  5  | 0x0010 |
                        |  42  |  4  |  6  | 0x0014 |
                        |  43  |  4  |  6  | 0x0014 |
                        |  54  |  5  |  7  | 0x0018 |
                        +---------------------------+

*/
{
    // Caclulate FSEL address
    volatile uint32_t addr = bcm2835_gpio + bcm2835_gpfsel_addr[gpio/10];
    uint8_t     shift = ( gpio % 10 ) * 3;
    uint32_t    mask  = BCM2835_GPIO_FSEL_MASK << shift;
    uint32_t    data  = mode << shift;
    bcm2835_peri_set_bits( addr, data, mask );
}

void spi_open( void )
{
    // This driver assumes the default pins with GPFSEL alternative function ALT0.
    gpio_fn_select( GPIO_PIN_CE0,  GPFSEL_ALTFN0 );
    gpio_fn_select( GPIO_PIN_CE1,  GPFSEL_ALTFN0 );
    gpio_fn_select( GPIO_PIN_MISO, GPFSEL_ALTFN0 );
    gpio_fn_select( GPIO_PIN_MOSI, GPFSEL_ALTFN0 );
    gpio_fn_select( GPIO_PIN_CLK,  GPFSEL_ALTFN0 );

    // Set SPI CS register to default values (0).
    volatile uint32_t *addr;
    addr = spi0 + SPI0_CS / 4; // Why / 4?
    spi_write( addr, 0 );

    // Clear FIFOs.
    spi_write_nb( addr, SPIO_CS_CLEAR );
}

void spi_close( void )
{
    // Set all SPI GPIO pins to input mode.
    gpio_fn_select( GPIO_CE0,  GPFSEL_INPUT );
    gpio_fn_select( GPIO_CE1,  GPFSEL_INPUT );
    gpio_fn_select( GPIO_MISO, GPFSEL_INPUT );
    gpio_fn_select( GPIO_MOSI, GPFSEL_INPUT );
    gpio_fn_select( GPIO_CLK,  GPFSEL_INPUT );
}

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

