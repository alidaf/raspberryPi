//  ===========================================================================
/*
    mcp42x1:

    Driver for the MCP42x1 SPI digital potentiometer.

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

#define MCP42X1_VERSION 10001 // v1.1

//  ===========================================================================
/*
    Authors:        D.Faulke    05/01/2016

    Contributors:

    Changelog:

        v1.0    Original version.
        v1.1    Rewrote SPI routines with direct BCM2835 register access.
*/
//  ===========================================================================

#ifndef MCP42X1_H
#define MCP42X1_H


//  MCP42x1 information. ------------------------------------------------------
/*
    The MCP42x1 is an SPI bus operated Dual 7/8-bit digital potentiometer with
    non-volatile memory.

                        +-----------( )-----------+
                        |  Fn  | pin | pin |  Fn  |
                        |------+-----+-----+------|
                        | CS   |  01 | 14  |  VDD | 1.8V -> 5.5V
                        | SCK  |  02 | 13  |  SDO |
                        | SDI  |  03 | 12  | SHDN |
                        | VSS  |  04 | 11  |   NC |
              ,----- // | P1B  |  05 | 10  |  P0B | // -----,
           R [ ]<--- // | P1W  |  06 | 09  |  P0W | // --->[ ] R
              '----- // | P1A  |  07 | 08  |  P0A | // -----'
                        +-------------------------+
            R = 5, 10, 50 or 100 kOhms. Wiper resistance = 75 Ohms.

            Key:
                    +---------------------------------------+
                    | Fn   | Description                    |
                    |------+--------------------------------|
                    | CS   | SPI chip select.               |
                    | SCK  | SPI clock input.               |
                    | SDI  | SPI serial data in.            |
                    | VSS  | GND.                           |
                    | PxA  | Potentiometer pin A.           |
                    | PxW  | Potentiometer wiper.           |
                    | PxB  | Potentiometer pin B.           |
                    | NC   | Not internally connected.      |
                    | SHDN | Hardware shutdown (reset).     |
                    | SDO  | Serial data out.               |
                    | VDD  | Supply voltage (1.8V to 5.5V). |
                    +---------------------------------------+
        Notes:

            NC is not internally connected but should be externally connected
            to either VSS or VDD to reduce noise coupling.

        Device memory map:

                        +--------------------------------+
                        | Addr | Function          | Mem |
                        |------+-------------------+-----|
                        | 00h  | Volatile wiper 0. | RAM |
                        | 01h  | Volatile wiper 1. | RAM |
                        | 02h  | Reserved.         | --- |
                        | 03h  | Reserved.         | --- |
                        | 04h  | Volatile TCON.    | RAM |
                        | 05h  | Status.           | RAM |
                        | 06h+ | Reserved.         | --- |
                        +--------------------------------+
        Notes:

            All 16 locations are 9 bits wide.
            The status register at 05h has 5 status bits, 4 of which are
            reserved. Bit 1 is the shutdown status; 0 = normal, 1 = Shutdown.

            The TCON register (Terminal CONtrol) has 8 control bits, 4 for
            each wiper, as shown:

            +--------------------------------------------------------------+
            | bit8 | bit7 | bit6 | bit5 | bit4 | bit3 | bit2 | bit1 | bit0 |
            |------+------+------+------+------+------+------+------+------|
            |  D8  | R1HW | R1A  | R1W  | R1B  | R0HW | R0A  | R0W  | R0B  |
            +--------------------------------------------------------------+

                RxHW : Forces potentiometer x into shutdown configuration of
                       the SHDN pin; 0 = normal, 1 = forced.
                RxA  : Connects/disconnects potentiometer x pin A to/from the
                       resistor network; 0 = connected, 1 = disconnected.
                RxW  : Connects/disconnects potentiometer x wiper to/from the
                       resistor network; 0 = connected, 1 = disconnected.
                RxB  : Connects/disconnects potentiometer x pin B to/from the
                       resistor network; 0 = connected, 1 = disconnected.

                The SHDN pin, when active, overrides the state of these bits.

            Thw resistance networks are controlled.......
*/


//  MCP42x1. ------------------------------------------------------------------


#define MCP42X1_MAX 2 // Limited by number of CS (Chip Select) pins on Pi.
/*
    Mote:   It is possible to handle the chip select independently of the SPI
            controller and therefore have many more but that is outside the
            scope of this driver.
*/

//  MCP42x1 register addresses.
#define MCP42X1_REG_WIPER0 0x00
#define MCP42X1_REG_WIPER1 0x01
#define MCP42X1_REG_TCON   0x04
#define MCP42X1_REG_STATUS 0x05

//  TCON register masks.

#define MCP42X1_TCON_R0B    0x01
#define MCP42X1_TCON_R0W    0x02
#define MCP42X1_TCON_R0A    0x03
#define MCP42X1_TCON_R0HW   0x08
#define MCP42X1_TCON_R1B    0x10
#define MCP42X1_TCON_R1W    0x20
#define MCP42X1_TCON_R1A    0x40
#define MCP42X1_TCON_R1HW   0x80

struct mcp42x1
{
    uint8_t id; // SPI handle.
    uint8_t cs; // Chip Select.
};

struct mcp42x1 *mcp42x1[MCP42X1_MAX];

//  ===========================================================================
/*
    This driver includes code to access the SPI interface directly rather than
    rely on external libraries.

    The Pi A+, B & B+ models have a Broadcom BCM2835 chip to provide the pin
    capabilities, while the 2B model has a BCM2836. In order to retain
    backward compatibility, the GPIO functions are based on the BCM2835.

    Much of the direct access to the BCM2835 registers is based on the BCM2835
    library by Mike McCauley.
*/

//  BCM2835 information. ------------------------------------------------------
/*
    Most recent versions of Linux use device trees to hold hardware information
    including peripheral addresses. Mike MCauley's code defines offsets into
    the device tree for the peripheral base address and block size.

    It isn't clear yet why offsets are used but it appears to me that the base
    addresses and sizes should be held by the reg field of the node, e.g.

    see http://lxr.free-electrons.com/source/arch/arm/boot/dts/bcm2835.dtsi

        gpio: gpio@7e200000 {
            ...
            reg = <0x7e200000 0xb4>
            ...
        };
        spi: spi@7e204000 {
            ...
            reg = <0x7e204000 0x1000>;
            ...
        };

    The most current dts file is here...
    https://github.com/raspberrypi/linux/blob/rpi-4.1.y/arch/arm/boot/dts/
            bcm2835.dtsi

    There doesn't appear to be much in it!
*/

//  Device tree information. --------------------------------------------------
#define BCM2835_DT_FILENAME "/proc/device-tree/soc/ranges"
#define BCM2835_DT_PERI_BASE_OFFSET 4 // Why offsets?
#define BCM2835_DT_PERI_SIZE_OFFSET 8 // Why offsets?

//  For non-device tree versions, from Raspberry Pi Hardware Reference.
#define BCM2835_PERI_BASE   0x20000000 // Base address of BCM2835 peripherals.
#define BCM2835_PERI_SIZE   0x01000000 // Size of address block.


//  Offsets for various peripheral registers. ---------------------------------
/*
    #1 BCM2835 ARM Peripherals.
    #2 Raspberry Pi Hardware Reference.
    Shouldn't these be pulled from the device tree too?
*/
#define BCM2835_STIM_BASE   0x003000 // System timer.       // #1 Section 12.
#define BCM2835_PADS_BASE   0x100000 // GPIO pads.          // #2 Table 4-3.
#define BCM2835_CLK_BASE    0x101000 // GP clock control.   // #2 Table 4-3.
#define BCM2835_GPIO_BASE   0x200000 // GPIO.               // #1 Section 6.1
#define BCM2835_SPI0_BASE   0x204000 // SPI0.               // #1 Section 10.5.
#define BCM2835_PWM_BASE    0x20c000 // PWM.                // #2 Table 4-3.
#define BCM2835_BSC0_BASE   0x205000 // Serial controller.  // #1 Section 3.2.
#define BCM2835_BSC1_BASE   0x804000 // Serial controller.  // #1 Section 3.2.
#define BCM2835_BSC2_BASE   0x805000 // Serial controller.  // #1 Section 3.2.

//  RPi memory specifics - see Raspberry Pi Hardware Reference, .
#define BCM2835_PAGE_SIZE   ( 4 * 1024 )
#define BCM2835_BLOCK_SIZE  ( 4 * 1024 )


//  GPIO register offsets from BCM2835_GPIO_BASE. -----------------------------
/*
    see BCM2835 ARM Peripherals SECTION 6.1.
*/
#define BCM2835_GPFSEL0     0x0000 // GPIO Function Select 0.
#define BCM2835_GPFSEL1     0x0004 // GPIO Function Select 1.
#define BCM2835_GPFSEL2     0x0008 // GPIO Function Select 2.
#define BCM2835_GPFSEL3     0x000c // GPIO Function Select 3.
#define BCM2835_GPFSEL4     0x0010 // GPIO Function Select 4.
#define BCM2835_GPFSEL5     0x0014 // GPIO Function Select 5.
#define BCM2835_GPSET0      0x001c // GPIO Pin Output Set 0.
#define BCM2835_GPSET1      0x0020 // GPIO Pin Output Set 1.
#define BCM2835_GPCLR0      0x0028 // GPIO Pin Output Clear 0.
#define BCM2835_GPCLR1      0x002c // GPIO Pin Output Clear 1.
#define BCM2835_GPLEV0      0x0034 // GPIO Pin Level 0.
#define BCM2835_GPLEV1      0x0038 // GPIO Pin Level 1.
#define BCM2835_GPEDS0      0x0040 // GPIO Pin Event Detect Status 0.
#define BCM2835_GPEDS1      0x0044 // GPIO Pin Event Detect Status 1.
#define BCM2835_GPREN0      0x004c // GPIO Pin Rising Edge Detect Enable 0.
#define BCM2835_GPREN1      0x0050 // GPIO Pin Rising Edge Detect Enable 1.
#define BCM2835_GPFEN0      0x0058 // GPIO Pin Falling Edge Detect Enable 0.
#define BCM2835_GPFEN1      0x005c // GPIO Pin Falling Edge Detect Enable 1.
#define BCM2835_GPHEN0      0x0064 // GPIO Pin High Detect Enable 0.
#define BCM2835_GPHEN1      0x0068 // GPIO Pin High Detect Enable 1.
#define BCM2835_GPLEN0      0x0070 // GPIO Pin Low Detect Enable 0.
#define BCM2835_GPLEN1      0x0074 // GPIO Pin Low Detect Enable 1.
#define BCM2835_GPAREN0     0x007c // GPIO Pin Async. Rising Edge Detect 0.
#define BCM2835_GPAREN1     0x0080 // GPIO Pin Async. Rising Edge Detect 1.
#define BCM2835_GPAFEN0     0x0088 // GPIO Pin Async. Falling Edge Detect 0.
#define BCM2835_GPAFEN1     0x008c // GPIO Pin Async. Falling Edge Detect 1.
#define BCM2835_GPPUD       0x0094 // GPIO Pin Pull-up/down Enable.
#define BCM2835_GPPUDCLK0   0x0098 // GPIO Pin Pull-up/down Enable Clock 0.
#define BCM2835_GPPUDCLK1   0x009c // GPIO Pin Pull-up/down Enable Clock 1.


//  BCM2835 GPIO function select registers (GPFSELn). -------------------------
/*
    Each of the BCM2835 GPIO pins has at least two alternative functions. The
    FSEL{n} field determines the function of the nth GPIO pin as:

        see BCM2835 ARM Peripherals, Tables 6.2 - 6.7.

        b000 (0x0) = GPIO pin as an input.
        b001 (0x1) = GPIO pin as an output.
        b100 (0x4) = GPIO pin as alternate function 0, ALT0.
        b101 (0x5) = GPIO pin as alternate function 1, ALT1.
        b110 (0x6) = GPIO pin as alternate function 2, ALT2.
        b111 (0x7) = GPIO pin as alternate function 3, ALT3.
        b011 (0x3) = GPIO pin as alternate function 4, ALT4.
        b010 (0x2) = GPIO pin as alternate function 5, ALT5.
        Mask for GPFSEL bits = b111 (0x7).
*/

//  Array of GPFSEL{n} addresses.
uint16_t bcm2835_gpfsel_addr[6] = { BCM2835_GPFSEL0, BCM2835_GPFSEL1,
                                    BCM2835_GPFSEL2, BCM2835_GPFSEL3,
                                    BCM2835_GPFSEL4, BCM2835_GPFSEL5 }

typedef enum
{
    BCM2835_GPFSEL_INPUT  = 0x00,
    BCM2835_GPFSEL_OUTPUT = 0x01,
    BCM2835_GPFSEL_ALTFN0 = 0x04,
    BCM2835_GPFSEL_ALTFN1 = 0x05,
    BCM2835_GPFSEL_ALTFN2 = 0x06,
    BCM2835_GPFSEL_ALTFN3 = 0x07,
    BCM2835_GPFSEL_ALTFN4 = 0x03,
    BCM2835_GPFSEL_ALTFN5 = 0x02,
    BCM2835_GPFSEL_MASK   = 0x07
}   bcm2835_gpfsel;

//  GPPUD register states, see BCM2835 ARM Peripherals Table 6-28.
typedef enum
{
    BCM2835_GPIO_PUD_OFF    = 0x00,
    BCM2835_GPIO_PUD_DOWN   = 0x01,
    BCM2835_GPIO_PUD_UP     = 0x02
//  Reserved                = 0x03
}   bcm2835_gppud;

#define BCM2835_CORE_FREQ 250000000 // Core clock frequency (250MHz).


//  SPI specific. -------------------------------------------------------------
/*
    The Raspberry Pi has one main SPI controller with 2 slave selects (CS).
    The auxiliary SPI controller is not supported in the Linux kernel.

    The main SPI controller is referred to as SPI0.

    The BCM2835/6 peripheral registers start at 0x20000000.
    The SPI0 peripheral registers start at 0x20204000, i.e. offset = 0x204000.

    The GPIO pins on the Raspberry Pi header are:

            +-------------------+
            | Func | Pin | GPIO |
            |------+-----+------|
            | MOSI |  19 |   10 |   Master Out, Slave In.
            | MISO |  21 |   09 |   Master In, Slave Out.
            | CLK  |  23 |   11 |   SPI clock.
            | CE0  |  24 |   08 |   Chip Enable (aka CS, Chip Select).
            | CE1  |  26 |   07 |   Chip Enable (aka CS, Chip Select).
            +-------------------+

    The SPI controller can operate in standard 3-wire mode, which is used here,
    or 2-wire bidirectional mode, where input and output are on the same line.
*/

//  SPI register offset address map. ------------------------------------------
/*
    See BCM2835 ARM Peripherals, Section 10.5.
*/
#define BCM2835_SPI0_CS     0x00000000 // SPI master control and status.
#define BCM2835_SPI0_FIFO   0x00000004 // SPI master Tx and Rx FIFOs.
#define BCM2835_SPI0_CLK    0x00000008 // SPI master clock divider.
#define BCM2835_SPI0_DLEN   0x0000000c // SPI master data length.
#define BCM2835_SPI0_LTOH   0x00000010 // SPI LOSSI mode TOH.
#define BCM2835_SPI0_DC     0x00000014 // SPI DMA DREQ controls.

/*
    For SPI, the GPIO pins and alternate functions are:

                +----------------------------+
                | GPIO | Function   | GPFSEL |
                |------+------------+--------|
                |   07 | SPI0_CE1_N |  ALT0  |
                |   08 | SPI0_CE0_N |  ALT0  |
                |   09 | SPI0_MISO  |  ALT0  |
                |   10 | SPI0_MOSI  |  ALT0  |
                |   11 | SPI0_CLK   |  ALT0  |
                |------+------------+--------|
                |   16 | SPI1_CE2_N |  ALT4  |
                |   17 | SPI1_CE1_N |  ALT4  |
                |   18 | SPI1_CE0_N |  ALT4  |
                |   19 | SPI1_MISO  |  ALT4  |
                |   20 | SPI1_MOSI  |  ALT4  |
                |   21 | SPI1_CLK   |  ALT4  |
                |------+------------+--------|
                |   35 | SPI0_CE1_N |  ALT0  |
                |   36 | SPI0_CE0_N |  ALT0  |
                |   37 | SPI0_MISO  |  ALT0  |
                |   38 | SPI0_MOSI  |  ALT0  |
                |   39 | SPI0_CLK   |  ALT0  |
                +------+------------+--------+
*/

//  BCM2835 GPIO pins for SPI. ------------------------------------------------
/*
    This driver assumes the default pins with GPFSEL ALT0.
*/

typedef enum
{
    SPI0_GPIO_CE1  =  7, // GPIO pin for SPI CE1 (Chip Select 1).
    SPI0_GPIO_CE0  =  8, // GPIO pin for SPI CE0 (Chip Select 0).
    SPI0_GPIO_MISO =  9, // GPIO pin for SPI MISO.
    SPI0_GPIO_MOSI = 10, // GPIO pin for SPI MOSI.
    SPI0_GPIO_CLK  = 11  // GPIO pin for SPI CLK
}   spi0_gpio;


//  SPI0_CS register. ---------------------------------------------------------
/*
    See MCP2835 ARM Peripherals, Section 10.5.
*/
#define SPI0_CS_LEN_LONG    0x01000000 // 8-bit or 16-bit FIFO LOSSI mode.
#define SPI0_CS_DMA_LEN     0x00800000 // Enable DMA LOSSI mode.
#define SPI0_CS_CSPOL2      0x00400000 // Chip select polarity.
#define SPI0_CS_CSPOL1      0x00200000 // Chip select polarity.
#define SPI0_CS_CSPOL0      0x00100000 // Chip select polarity.
#define SPI0_CS_RXF         0x00080000 // Rx FIFO full.
#define SPI0_CS_RXR         0x00040000 // Rx FIFO needs reading.
#define SPI0_CS_TXD         0x00020000 // Tx FIFO contains data.
#define SPI0_CS_RXD         0x00010000 // Rx FIFO contains data.
#define SPI0_CS_DONE        0x00008000 // Transfer done.
#define SPI0_CS_TE_EN       0x00004000 // Not used.
#define SPI0_CS_LMONO       0x00002000 // Not used.
#define SPI0_CS_LEN         0x00001000 // LEN LOSSI enable.
#define SPI0_CS_REN         0x00000800 // Read enable.
#define SPI0_CS_ADCS        0x00000400 // Auto deassert chip select.
#define SPI0_CS_INTR        0x00000200 // Interrupt on RxR
#define SPI0_CS_INTD        0x00000100 // Interrupt on DONE.
#define SPI0_CS_DMAEN       0x00000080 // DMA enable.
#define SPI0_CS_TA          0x00000040 // Transfer active.
#define SPI0_CS_CSPOL       0x00000020 // Chip select polarity.
#define SPI0_CS_CLEAR       0x00000030 // Clear Tx and Rx.
#define SPI0_CS_CLEARRX     0x00000020 // Clear Rx only.
#define SPI0_CS_CLEARTX     0x00000010 // Clear Tx only.
#define SPI0_CS_CPOL        0x00000008 // Clock polarity.
#define SPI0_CS_CPHA        0x00000004 // Clock phase.
#define SPI0_CS_CS          0x00000003 // All CS bits.
#define SPI0_CS_CS2         0x00000002 // CS2.
#define SPI0_CS_CS1         0x00000001 // CS1.
#define SPI0_CS_CS0         0x00000000 // CS0.


//  SPI0_CLK register. --------------------------------------------------------
/*
            +---------------------------------------+
            | Bits  | Function  | Description       |
            |-------+-----------+-------------------|
            | 16-31 | ------    | Reserved.         |
            | 00-15 | CDIV      | Clock divider.    |
            +---------------------------------------+

            SCLK = Core clock / CDIV.
            if CDIV = 0, divisor is 65536.

            According to the datasheet, CDIV must be a power of 2 but a forum
            thread suggests that this may be an error and CDIV must actually
            be a multiple of 2.
            -see https://www.raspberrypi.org/forums/
                 viewtopic.php?f=44&t=43442&p=347073

*/

//  SPI0_DLEN register. -------------------------------------------------------
/*
            +---------------------------------------+
            | Bits  | Function  | Description       |
            |-------+-----------+-------------------|
            | 16-31 | ------    | Reserved.         |
            | 00-15 | LEN       | Data length.      |
            +---------------------------------------+
*/

//  SPIO_LTOH register. -------------------------------------------------------
/*
            +-------------------------------------------+
            | Bits  | Function  | Description           |
            |-------+-----------+-----------------------|
            | 16-31 | ------    | Reserved.             |
            | 00-15 | TOH       | Output hold delay.    |
            +-------------------------------------------+
*/

//  SPIO_DC register. ---------------------------------------------------------
/*
            +---------------------------------------------------+
            | Bits  | Function  | Description                   |
            |-------+-----------+-------------------------------|
            | 24-31 | RPANIC    | DMA read panic threshold.     |
            | 16-23 | RDREQ     | DMA request threshold.        |
            | 08-15 | TPANIC    | DMA write panic threshold.    |
            | 00-07 | TDREQ     | DMA write request threshold.  |
            +---------------------------------------------------+
*/

//  Software operation. -------------------------------------------------------
/*
    Polled:
        a)  Set CS, CPOL, CPHA as required and set TA = 1.
        b)  Poll TXD writing bytes to SPI_FIFO, RXD reading bytes from SPI_FIFO
            until all data written.
        c)  Poll DONE until it sets to 1.
        d)  Set TA = 0.

    Interrupt:  Too advanced for me!

    DMA:        Too advanced for me!

*/

volatile uint32_t *spi0;


//  BCM2835 functions. --------------------------------------------------------




//  MCP42x1 functions. --------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Writes byte to register of MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1WriteByte( struct mcp42x1 *mcp42x1, uint8_t reg, uint8_t data );

//  ---------------------------------------------------------------------------
//  Reads byte from register of MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1ReadByte( struct mcp42x1 *mcp42x1, uint8_t reg );

//  ---------------------------------------------------------------------------
//  Initialises MCP42x1. Call for each MCP42x1.
//  ---------------------------------------------------------------------------
int8_t mcp42x1Init( uint8_t device, uint8_t mode, uint8_t bits, uint16_t divider );
/*
    device  : 0 = CS0
              1 = CS1
    mode    : SPI_MODE_0 : CPOL = 0, CPHA = 0.
              SPI_MODE_1 : CPOL = 0, CPHA = 1.
              SPI_MODE_2 : CPOL = 1, CPHA = 0.
              SPI_MODE_3 : CPOL = 1, CPHA = 1.
    bits    : bits per word, usually 8.
    divider : SPI bus clock divider, multiple of 2 up to 65536.
              SCLK = Core clock / divider.
              Core clock = 250MHz.
              Values of 0, 1 and 65536 are functionally equivalent.
              The fastest speed is with a value of 2, i.e. 125MHz.
*/

#endif
