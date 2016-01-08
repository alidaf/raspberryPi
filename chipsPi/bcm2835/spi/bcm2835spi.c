//  ===========================================================================
/*
    bcm2835spi:

    Library for the BCM2835 SPI controller.

    Copyright 2016 Darren Faulke <darren@alidaf.co.uk>

    Sources:

    Broadcom BCM2835 ARM Peripherals, Reference C6357-M-1398.
        - see https://www.raspberrypi.org/wp-content/uploads/2012/02/
              BCM2835-ARM-Peripherals.pdf

    Raspberry Pi Hardware Reference by Warren Gay
        - ISBN-10: 1484208005
        - ISBN-13: 978-1484208007

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

        gcc -c -Wall -fpic bcm2835spi.c
        gcc -shared -o libbcm2835spi.so bcm2835spi.o

    For Raspberry Pi optimisation use the following flags:

        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
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

//  Companion header.
#include bcm2835spi.h"


//  Macros. -------------------------------------------------------------------

//  Number of known Pi revisions.
#define PI_REVISIONS 17

//  Pi revisions and corresponding ARM processor versions.
#define PI_REVISION { "0002", "0003", "0004", "0005", "0006", "0007",\
                      "0008", "0009", "0010", "0012", "0013", "000d",\
                      "000e", "000f", "a01041", "a21041", "900092" }
#define PI_ARM_VERS { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 6 }
/*

    Recommended way of addressing registers
    see http://accu.org/index.php/journals/281...

    static const uint32_t baseAddress = 0xfffe0000;
    enum registers
    {
        REG1 = 0x00,
        REG2 = 0x01,
        REG3 = 0x02,
        ...etc...
    };

    //  Return register address.
    inline volatile uint8_t *regAddress (registers reg )
    {
        return reinterpret_case<volatile uint32_t *> (baseAddress + reg );
    }

    //  Read from register.
    inline uint32_t regRead( registers reg )
    {
        return *regAddress( reg );
    }

    //  Write to register.
    inline void regWrite ( registers reg, uint32_t data )
    {
        *regAddress( reg ) = data;
    }
*/

//  Inline functions. ---------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns register adddress.
//  ---------------------------------------------------------------------------
inline volatile uint32_t *bcm2835_register_address( registers reg )
{
    return reinterpret_case<volatile uint32_t *>( bcm2835_peri_base + reg );
}

//  ---------------------------------------------------------------------------
//  Returns 32-bit register data.
//  ---------------------------------------------------------------------------
inline uint32_t bcm2835_register_read( registers reg )
{
    return *bcm2835_register_address( reg );
}

//  ---------------------------------------------------------------------------
//  Writes 32-bit data to register.
//  ---------------------------------------------------------------------------
inline void bcm2835_register_write( registers reg, uint32_t data )
{
    *bcm2835_register_address( reg ) = data;
}

/*
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
//  Returns 32-bit register data with a memory barrier.
//  ---------------------------------------------------------------------------
inline uint32_t bcm2835_register_read_barrier( registers reg )
{
    __sync_synchronize(); // Memory barrier.
    uint32_t data = *bcm2835_register_address( reg );
    __sync_synchronize(); // Memory barrier.
    return data;
}

//  ---------------------------------------------------------------------------
//  Writes 32-bit data to register with a memory barrier.
//  ---------------------------------------------------------------------------
inline void bcm2835_register_write_barrier( registers reg, uint32_t data )
{
    __sync_synchronize(); // Memory barrier.
    *bcm2835_register_address( reg ) = data;
    __sync_synchronize(); // Memory barrier.
}


//  Local functions. ----------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns Pi revision and sets some global variables.
//  ---------------------------------------------------------------------------
uint8_t getPiRevision( void )
{

// Array of Pi revision numbers and ARM processor versions.
static char *arm_versions[2][PI_REVISIONS] = { PI_REVISION, PI_ARM_VERS };

    FILE *filePtr;                  // File to read in and search.
    static char line[ 512 ];        // Storage for each line read in.
    static char token;              // Storage for character token.
    static unsigned int revision;   // Revision in hex format.

    // Open file for reading.
    filePtr = fopen( "/proc/cpuinfo", "r" );
    if ( filePtr == NULL ) return( -1 );

    // Get Revision.
    while ( fgets( line, sizeof( line ), filePtr ) != NULL )
        if ( strncmp ( line, "Revision", 8 ) == 0 )
            if ( !strncasecmp( "revision\t", line, 9 ))
            {
                // line should be "string" "colon" "hex string" "line break"
                sscanf( line, "%s%s%x%c", &line, &token, &revision, &token );
                    if ( token == '\n' ) break;
            }
    fclose ( filePtr ) ;

    // Now compare against arrays to find ARM version.
    static uint8_t arm = 0;
    static uint8_t i;
    for ( i = 0; i < PI_REVISIONS; i++ )
        if ( revision == strtol( armVersions[0][i], NULL, 16 ))
            arm = armVersions[1][i];

    // Set peripheral base address according to ARM version.
    switch( arm )
    {
        case 6  : bcm2835_peri_base = 0x20000000; break;
        case 7  : bcm2835_peri_base = 0x3f000000; break;
        default : bcm2835_peri_base = 0x20000000; break;
    }

    if ( arm < 0 ) return 0; // If not found.

    return revision;
}


//  BCM2835 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Sets specific (masked) bits in a peripheral register.
//  ---------------------------------------------------------------------------
void bcm2835_set_register_bits( volatile uint32_t reg,
                                         uint32_t data,
                                         uint32_t mask )
{
    //  Read register bits.
    uint32_t temp = bcm2835_register_read_barrier( reg );
    //  Create new bit field using mask to preserve other settings.
    temp = ( temp & ~mask ) | ( data & mask );
    //  Write new bit field to register.
    bcm2835_register_write_barrier( reg, temp )
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
    volatile uint32_t paddr = bcm2835_gpio + BCM2835_GPFSEL0/4 + ( pin/10 );
    uint8_t     shift = ( gpio % 10 ) * 3;
    uint32_t    mask  = BCM2835_GPIO_FSEL_MASK << shift;
    uint32_t    data  = mode << shift;
    bcm2835_register_set_bits( paddr, data, mask );
}

//  SPI functions. ------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Enables SPI function on default GPIOs.
//  ---------------------------------------------------------------------------
void bcm2835_spi_open( void )
{
    // This driver uses the default pins with GPFSEL alternative function ALT0.
    bcm2835_gpio_fsel( SPI_GPIO_CE0,  BCM2835_GPFSEL_ALT0 );
    bcm2835_gpio_fsel( SPI_GPIO_CE1,  BCM2835_GPFSEL_ALT0 );
    bcm2835_gpio_fsel( SPI_GPIO_MISO, BCM2835_GPFSEL_ALT0 );
    bcm2835_gpio_fsel( SPI_GPIO_MOSI, BCM2835_GPFSEL_ALT0 );
    bcm2835_gpio_fsel( SPI_GPIO_CLK,  BCM2835_GPFSEL_ALT0 );

    // Set SPI CS register to default values (0).
    volatile uint32_t *paddr;
    paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
    bcm2835_peri_write( paddr, 0 );

    // Clear FIFOs.
    bcm2835_peri_write_nb( paddr, BCM2835_SPIO_CS_CLEAR );
}

//  ---------------------------------------------------------------------------
//  Returns default SPI GPIOs back to inputs.
//  ---------------------------------------------------------------------------
void bcm2835_spi_close( void )
{
    // Set all SPI GPIO pins to input mode.
    bcm2835_gpio_fsel( SPI_GPIO_CE0,  BCM2835_GPFSEL_INPUT );
    bcm2835_gpio_fsel( SPI_GPIO_CE1,  BCM2835_GPFSEL_INPUT );
    bcm2835_gpio_fsel( SPI_GPIO_MISO, BCM2835_GPFSEL_INPUT );
    bcm2835_gpio_fsel( SPI_GPIO_MOSI, BCM2835_GPFSEL_INPUT );
    bcm2835_gpio_fsel( SPI_GPIO_CLK,  BCM2835_GPFSEL_INPUT );
}

//  ---------------------------------------------------------------------------
//  Sets clock divider to determine SPI bus speed.
//  ---------------------------------------------------------------------------
void bcm2835_spi_setDivider( uint16_t divider );
/*
    SPI0_CLK register:

            +---------------------------------------+
            | Bits  | Function  | Description       |
            |-------+-----------+-------------------|
            | 16-31 | ------    | Reserved.         |
            | 00-15 | CDIV      | Clock divider.    |
            +---------------------------------------+

            SCLK = Core clock / CDIV.
            if CDIV = 0, divisor is 65536.

    According to the datasheet, CDIV must be a power of 2 with odd numbers
    rounded down, i.e.

            CDIV = n^2, where n = 2, 4, 6, 8...256.

    However, a forum thread suggests that this may be an error and CDIV may
    actually be any even multiple of 2, i.e.

            CDIV = n * 2, where n = 2, 4, 6, 8...32768.

    see https://www.raspberrypi.org/forums/viewtopic.php?f=44&t=43442&p=347073

    The fastest possible bus speed is with divider = 2, i.e.

                    250MHz / 2 = 125MHz.
*/
{
    volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CLK / 4;
    bcm2835_peri_write( paddr, divider );
}

//  ---------------------------------------------------------------------------
//  Sets SPI data mode.
//  ---------------------------------------------------------------------------
/*
    There are 4 modes, which are set by the CPOL (clock polarity) and CPHA
    (clock phase) bits in the CS register:

        +---------------------------------------------------------------+
        | CPOL | CPHA | Mode | Description                              |
        |------+------+------+------------------------------------------|
        |   0  |   0  |   0  | Normal clock, sampled on rising edge.    |
        |   0  |   1  |   1  | Normal clock, sampled on falling edge.   |
        |   1  |   0  |   2  | Inverted clock, sampled on rising edge.  |
        |   1  |   1  |   3  | Inverted clock, sampled on falling edge. |
        +---------------------------------------------------------------+

    All slaves on the bus must operate in the same mode.
*/
void bcm2835_spi_setDataMode( uint8_t mode )
{
    volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
    uint32_t mask = ( BCM2835_SPI0_CS_CPOL | BCM2835_SPI0_CS_CPHA );
    uint32_t data = mode << 2; // CPHA and CPOL are bits 2 and 3 of CS reg.
    bcm2835_peri_set_bits( paddr, data, mask );
}

//  ---------------------------------------------------------------------------
//  Transfers 1 byte on SPI bus in polled mode (simultaneous read and write).
//  ---------------------------------------------------------------------------
uint8_t bcm2835_spi_transferBytePolled( uint8_t data )
/*
    Polled software operation from BCM2835 ARM Peripherals:

    (a) Set CS, CPOL and CPHA as required and set TA flag.
    (b) Poll TXD writing bytes to SPI_FIFO, RXD reading bytes from SPI_FIFO
        until all data written.
    (c) Poll DONE until = 1.
    (d) Set TA = 0.
*/
{
    volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
    volatile uint32_t *fifo  = bcm2835_spi0 + BCM2835_SPI0_FIFO / 4;
    volatile uint32_t bits;
    volatile uint32_t mask;

    // Clear FIFOs
    bits = BCM2835_SPI0_CS_CLEAR;
    mask = BCM2835_SPI0_CS_CLEAR;
    bcm2835_peri_set_bits( paddr, bits, mask );

    // (a) Mode should already be set before calling this function.

    // (a) Set TA (transfer active flag).
    bits = BCM2835_SPI0_CS_TA;
    mask = BCM2835_SPI0_CS_TA;
    bcm2835_peri_set_bits( paddr, bits, mask );

    // (b) Wait for TX FIFO to have space for at least 1 byte (TXD flag = 1).
    while ( !( bcm2835_peri_read( paddr ) & BCM2835_SPI0_CS_TXD ));

    // Write to FIFO.
    bcm2835_peri_write_nb( fifo, data );

    // (c) Wait until DONE flag is set.
    while ( !( bcm2835_peri_read_nb( paddr ) & BCM2835_SPI0_CS_DONE ));

    // Read byte from slave.
    uint32_t ret;
    ret = bcm2835_peri_read_nb( fifo );

    // (d) Clear TA (transfer active flag).
    bits = 0;
    mask = BCM2835_SPI0_CS_TA;
    bcm2835_peri_set_bits( paddr, bits, mask );

    return ret;
}

//  ---------------------------------------------------------------------------
//  Sets appropriate CS bits for chip select.
//  ---------------------------------------------------------------------------
void bcm2835_spi_cs( uint8_t cs )
{
    volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
    uint32_t data = cs;
    uint32_t mask = BCM2835_SPI0_CS_CS;
    bcm2835_peri_set_bits( paddr, data, mask );
}

//  ---------------------------------------------------------------------------
//  Sets chip select polarity.
//  ---------------------------------------------------------------------------
void bcm2835_spi_setPolarity( uint8_t cs, uint8_t polarity )
/*
    cs is 0, 1 or 2.
    polarity is 0 (active low) or 1 (active high).
*/
{
    volatile uint32_t *paddr = bcm2835_spi0 + BCM2835_SPI0_CS / 4;
    uint8_t shift = cs + 21; // Lowest CS bit is 21 (CSPOL0).
    uint32_t data = polarity << shift;
    uint32_t mask = 1 << shift;
    bcm2835_peri_set_bits( paddr, data, mask );
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

