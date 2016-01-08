//  ===========================================================================
/*
    bcm2835spi:

    Library for the BCM2835 SPI controller.

    Copyright 2016 Darren Faulke <darren@alidaf.co.uk>

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

#define BCM2835SPI_VERSION 0100

//  ===========================================================================
/*
    Authors:        D.Faulke    08/01/2016

    Contributors:

    Changelog:

        v1.00   Original version.
*/
//  ===========================================================================

#ifndef MCP42X1_H
#define MCP42X1_H

/*
    This is an attempt to provide a small self contained library specifically
    for the SPI interfaces of the Raspberry Pi so that it may be included into
    smaller driver codes. It is also a learning exercise for low level device
    programming.

    The Pi A+, B & B+ models have a Broadcom BCM2835 chip to provide the pin
    capabilities, while the 2B model has a BCM2836. In order to retain
    backward compatibility, the GPIO functions are based on the BCM2835.
*/

//  BCM2835 information. ------------------------------------------------------
/*
    Most recent versions of Linux use device trees to hold hardware information
    including peripheral addresses.

    The latest dts and dtsi files are in /arch/arm/boot/dts of the linux source
    code, e.g.

    https://github.com/raspberrypi/linux/blob/rpi-4.1.y/arch/arm/boot/dts/

    Relevant files:

        bcm2708-rpi-b-plus.dts
        bcm2708-rpi-b.dts
        bcm2708-rpi-cm.dts
        bcm2708-rpi-cm.dtsi
        bcm2708.dtsi
        bcm2708_common.dtsi
        bcm2709-rpi-2-b.dts
        bcm2709.dtsi
        bcm2835-rpi-b-plus.dts
        bcm2835-rpi-b.dts
        bcm2835-rpi.dtsi
        bcm2835.dtsi

    Device trees are relatively new for the Pi and there appear to be no
    efforts to integrate the into the more well known hardware libraries just
    yet.
*/


//  Peripheral base address (for non-deice tree versions). --------------------
static volatile uint32_t bcm2835_peri_base = 0x20000000; // Model 1.
//  Model 2 has a base address of 0x3f000000 but this can be set later.

//  Offsets for specific peripheral registers. ---------------------------------
static volatile uint32_t bcm2835_gpio_base = bcm2835_peri_base + 0x200000;
static volatile uint32_t bcm2835_spi_base  = bcm2835_peri_base + 0x204000;
static volatile uint32_t bcm2835_aux_base  = bcm2835_peri_base + 0x215000;


//  RPi memory specifics - see Raspberry Pi Hardware Reference. ---------------
static const uint8_t bcm2835_page_size  = 4 * 1024;
static const uint8_t bcm2835_block_size = 4 * 1024;


//  Register address offsets. -------------------------------------------------
enum bcm2835_registers
{

    //  GPIO registers - "BCM2835 ARM Peripherals", section 6.1.

    GPFSEL0     = bcm2835_gpio_base + 0x00, // GPIO Function Select 0.
    GPFSEL1     = bcm2835_gpio_base + 0x04, // GPIO Function Select 1.
    GPFSEL2     = bcm2835_gpio_base + 0x08, // GPIO Function Select 2.
    GPFSEL3     = bcm2835_gpio_base + 0x0c, // GPIO Function Select 3.
    GPFSEL4     = bcm2835_gpio_base + 0x10, // GPIO Function Select 4.
    GPFSEL5     = bcm2835_gpio_base + 0x14, // GPIO Function Select 5.
    //          = bcm2835_gpio_base + 0x18, // Reserved
    GPSET0      = bcm2835_gpio_base + 0x1c, // GPIO pin output set 0.
    GPSET1      = bcm2835_gpio_base + 0x20, // GPIO pin output set 1.
    //          = bcm2835_gpio_base + 0x24, // Reserved.
    GPCLR0      = bcm2835_gpio_base + 0x28, // GPIO pin output clear 0.
    GPCLR1      = bcm2835_gpio_base + 0x2c, // GPIO pin output clear 1.
    //          = bcm2835_gpio_base + 0x30, // Reserved.
    GPLEV0      = bcm2835_gpio_base + 0x34, // GPIO pin level 0.
    GPLEV1      = bcm2835_gpio_base + 0x38, // GPIO pin level 1.
    //          = bcm2835_gpio_base + 0x3c, // Reserved.
    GPEDS0      = bcm2835_gpio_base + 0x40, // GPIO pin event detect status 0.
    GPEDS1      = bcm2835_gpio_base + 0x44, // GPIO pin event detect status 1.
    //          = bcm2835_gpio_base + 0x48, // Reserved.
    GPREN0      = bcm2835_gpio_base + 0x4c, // GPIO pin rising edge detect enable 0.
    GPREN1      = bcm2835_gpio_base + 0x50, // GPIO pin rising edge detect enable 1.
    //          = bcm2835_gpio_base + 0x54, // Reserved.
    GPFEN0      = bcm2835_gpio_base + 0x58, // GPIO pin falling edge detect enable 0.
    GPFEN1      = bcm2835_gpio_base + 0x5c, // GPIO pin falling edge detect enable 1.
    //          = bcm2835_gpio_base + 0x60, // Reserved.
    GPHEN0      = bcm2835_gpio_base + 0x64, // GPIO pin high detect enable 0.
    GPHEN1      = bcm2835_gpio_base + 0x68, // GPIO pin high detect enable 1.
    //          = bcm2835_gpio_base + 0x6c, // Reserved.
    GPLEN0      = bcm2835_gpio_base + 0x70, // GPIO pin low detect enable 0.
    GPLEN0      = bcm2835_gpio_base + 0x74, // GPIO pin low detect enable 1.
    //          = bcm2835_gpio_base + 0x78, // Reserved.
    GPAREN0     = bcm2835_gpio_base + 0x7c, // GPIO pin async rising edge detect 0.
    GPAREN0     = bcm2835_gpio_base + 0x80, // GPIO pin async rising edge detect 1.
    //          = bcm2835_gpio_base + 0x84, // Reserved.
    GPAFEN0     = bcm2835_gpio_base + 0x88, // GPIO pin async falling edge detect 0.
    GPAFEN1     = bcm2835_gpio_base + 0x8c, // GPIO pin async falling edge detect 1.
    //          = bcm2835_gpio_base + 0x90, // Reserved.
    GPPUD       = bcm2835_gpio_base + 0x94, // GPIO pin pull-up/down enable.
    GPPUDCLK0   = bcm2835_gpio_base + 0x98, // GPIO pin pull-up/down enable clock 0.
    GPPUDCLK0   = bcm2835_gpio_base + 0x9c  // GPIO pin pull-up/down enable clock 1.
    //          = bcm2835_gpio_base + 0xa0, // Reserved.
    //          = bcm2835_gpio_base + 0xb0  // Test.

    //  AUX registers.

    AUXIRQ       = bcm2835_aux_base + 0x00, // Auxiliary interrupt status.
    AUXENABLES   = bcm2835_aux_base + 0x04, // Auxiliary enables.
    AUXMUIO      = bcm2835_aux_base + 0x40, // Mini UART I/O data.
    AUXMUIER     = bcm2835_aux_base + 0x44, // Mini UART interrupt enable.
    AUXMUIIR     = bcm2835_aux_base + 0x48, // Mini UART interrupt identify.
    AUXMULCR     = bcm2835_aux_base + 0x4c, // Mini UART line control.
    AUXMUMCR     = bcm2835_aux_base + 0x50, // Mini UART MODEM control.
    AUXMULSR     = bcm2835_aux_base + 0x54, // Mini UART line status.
    AUXMUMSR     = bcm2835_aux_base + 0x58, // Mini UART MODEM status.
    AUXMUSCRATCH = bcm2835_aux_base + 0x5c, // Mini UART scratch.
    AUXMUCNTL    = bcm2835_aux_base + 0x60, // Mini UART extra control.
    AUXMUSTAT    = bcm2835_aux_base + 0x64, // Mini UART extra status.
    AUXMUBAUD    = bcm2835_aux_base + 0x68, // Mini UART BAUD rate.
    AUXSPI0CNTL0 = bcm2835_aux_base + 0x80, // SPI1 control register 0.
    AUXSPI0CNTL1 = bcm2835_aux_base + 0x84, // SPI1 control register 1.
    AUXSPI0STAT  = bcm2835_aux_base + 0x88, // SPI1 status.
    AUXSPI0IO    = bcm2835_aux_base + 0x90, // SPI1 data.
    AUXSPI0PEEK  = bcm2835_aux_base + 0x94, // SPI1 peek.
    AUXSPI0CNTL0 = bcm2835_aux_base + 0xc0, // SPI2 control register 0.
    AUXSPI0CNTL1 = bcm2835_aux_base + 0xc4, // SPI2 control register 1.
    AUXSPI0STAT  = bcm2835_aux_base + 0xc8, // SPI2 status.
    AUXSPI0IO    = bcm2835_aux_base + 0xd0, // SPI2 data.
    AUXSPI0PEEK  = bcm2835_aux_base + 0xd4  // SPI2 peek.
};


//  BCM2835 GPIO function select registers (GPFSELn). -------------------------
/*
    Each of the BCM2835 GPIO pins has at least two alternative functions. The
    FSEL{n} field determines the function of the nth GPIO pin as:
    see BCM2835 ARM Peripherals, Tables 6.2 - 6.7.
*/
enum bcm2835_gpfsel
{
    BCM2835_GPFSEL_INPUT  = 0x00, // GPIO pin is an input.
    BCM2835_GPFSEL_OUTPUT = 0x01, // GPIO pin is an output.
    BCM2835_GPFSEL_ALTFN0 = 0x04, // GPIO pin takes alt function 0, ALT0.
    BCM2835_GPFSEL_ALTFN1 = 0x05, // GPIO pin takes alt function 1, ALT1.
    BCM2835_GPFSEL_ALTFN2 = 0x06, // GPIO pin takes alt function 2, ALT2.
    BCM2835_GPFSEL_ALTFN3 = 0x07, // GPIO pin takes alt function 3, ALT3.
    BCM2835_GPFSEL_ALTFN4 = 0x03, // GPIO pin takes alt function 4, ALT4.
    BCM2835_GPFSEL_ALTFN5 = 0x02, // GPIO pin takes alt function 5, ALT5.
    BCM2835_GPFSEL_MASK   = 0x07  // Mask for GPFSEL bits.
}

//  GPPUD register states, see BCM2835 ARM Peripherals Table 6-28. ------------
enum bcm2835_gppud
{
    BCM2835_GPIO_PUD_OFF    = 0x00,
    BCM2835_GPIO_PUD_DOWN   = 0x01,
    BCM2835_GPIO_PUD_UP     = 0x02
//  Reserved                = 0x03
};


//  Core clock. ---------------------------------------------------------------
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
enum bcm2835_spi_reg // See BCM2835 ARM Peripherals, Section 10.5.
{
    BCM2835_SPI_CS   // SPI master control and status.
    BCM2835_SPI_FIFO // SPI master Tx and Rx FIFOs.
    BCM2835_SPI_CLK  // SPI master clock divider.
    BCM2835_SPI_DLEN // SPI master data length.
    BCM2835_SPI_LTOH // SPI LOSSI mode TOH.
    BCM2835_SPI_DC   // SPI DMA DREQ controls.
};


//  BCM2835 GPIO pins for SPI. ------------------------------------------------
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

enum bcm2835_spi_gpio_alt0       // Standard SPI controller (ALT0).
{
    RPI_SPI_ALT0_GPIO_CE1  =  7, // Default GPIO pin for SPI0 CE1 (CS 1).
    RPI_SPI_ALT0_GPIO_CE0  =  8, // Default GPIO pin for SPI0 CE0 (CS 0).
    RPI_SPI_ALT0_GPIO_MISO =  9, // Default GPIO pin for SPI0 MISO.
    RPI_SPI_ALT0_GPIO_MOSI = 10, // Default GPIO pin for SPI0 MOSI.
    RPI_SPI_ALT0_GPIO_SCLK = 11  // Default GPIO pin for SPI0 SCLK.
};

/*
    The BCM2835 has two secondary low throughput SPI interfaces, SPI1 & SPI2,
    that need to be enabled in the AUX register before using. One of these
    may be needed if the primary SPI bus is being used for high throughput
    data like audio. The aux controller has an additional chip select pin.
*/

enum bcm2835_spi_gpio_alt4       // Aux SPI controller (ALT4).
{
    RPI_SPI_ALT4_GPIO_CE2  = 16, // Default GPIO pin for SPIn CE2 (CS2).
    RPI_SPI_ALT4_GPIO_CE1  = 17, // Default GPIO pin for SPIn CE1 (CS1).
    RPI_SPI_ALT4_GPIO_CE0  = 18, // Default GPIO pin for SPIn CE0 (CS0).
    RPI_SPI_ALT4_GPIO_MISO = 19, // Default GPIO pin for SPIn MISO.
    RPI_SPI_ALT4_GPIO_MOSI = 20, // Default GPIO pin for SPIn MOSI.
    RPI_SPI_ALT4_GPIO_SCLK = 21  // Default GPIO pin for SPIn SCLK.
};


//  SPI CS register. ----------------------------------------------------------
/*
    See MCP2835 ARM Peripherals, Section 10.5.
*/
#define BCM2835_SPI_CS_LEN_LONG     (1 << 25) // FIFO LOSSI mode.
#define BCM2835_SPI_CS_DMA_LEN      (1 << 24) // Enable DMA LOSSI mode.
#define BCM2835_SPI_CS_CSPOL(x)   ((x) << 21) // Chip select polarity.
#define BCM2835_SPI_CS_RXF          (1 << 20) // Rx FIFO full.
#define BCM2835_SPI_CS_RXR          (1 << 19) // Rx FIFO needs reading.
#define BCM2835_SPI_CS_TXD          (1 << 18) // Tx FIFO contains data.
#define BCM2835_SPI_CS_RXD          (1 << 17) // Rx FIFO contains data.
#define BCM2835_SPI_CS_DONE         (1 << 16) // Transfer done.
#define BCM2835_SPI_CS_TE_EN        (1 << 15) // Not used.
#define BCM2835_SPI_CS_LMONO        (1 << 14) // Not used.
#define BCM2835_SPI_CS_LEN          (1 << 13) // LEN LOSSI enable.
#define BCM2835_SPI_CS_REN          (1 << 12) // Read enable.
#define BCM2835_SPI_CS_ADCS         (1 << 11) // Auto deassert chip select.
#define BCM2835_SPI_CS_INTR         (1 << 10) // Interrupt on RxR.
#define BCM2835_SPI_CS_INTD         (1 <<  9) // Interrupt on DONE.
#define BCM2835_SPI_CS_DMAEN        (1 <<  8) // DMA enable.
#define BCM2835_SPI_CS_TA           (1 <<  7) // Transfer active.
#define BCM2835_SPI_CS_CSPOL(x)   ((x) <<  6) // Chip select polarity.
#define BCM2835_SPI_CS_CLEAR(x)   ((x) <<  4) // Clear Tx/Rx.
#define BCM2835_SPI_CS_MODE(x)    ((x) <<  2) // Chip select mode.
#define BCM2835_SPI_CS_CS(x)      ((x) <<  0) // Chip select.

enum bcm2835_spi_cs_mode    // Chip select modes.
{                           //  (CPOL, CPHA)
    BCM2835_SPI_CS_MODE0,   //     (0, 0)
    BCM2835_SPI_CS_MODE1,   //     (0, 1)
    BCM2835_SPI_CS_MODE2,   //     (1, 0)
    BCM2835_SPI_CS_MODE3    //     (1, 1)
};

enum bcm2835_spi_cs_cs      // Chip select values.
{
    BCM2835_SPI_CS_CS0,     // Chip select 0.
    BCM2835_SPI_CS_CS1,     // Chip select 1.
    BCM2835_SPI_CS_CS2      // Chip select 2.
};


//  Aux ENB register. ---------------------------------------------------------

#define BCM2835_AUX_ENB_SPI2    (1 << 2) // SPI2 enable.
#define BCM2835_AUX_ENB_SPI1    (1 << 1) // SPI1 enable.
#define BCM2835_AUX_ENB_UART    (1 << 0) // UART enable.


//  Aux SPI CNTL0 register. ---------------------------------------------------

#define BCM2835_AUX_SPI_CNTL0_SPEED(x)    ((x) << 20) // SPI speed.
#define BCM2835_AUX_SPI_CNTL0_CS(x)       ((x) << 17) // Chip selects.
#define BCM2835_AUX_SPI_CNTL0_POSTINP       (1 << 16) // Post-input mode.
#define BCM2835_AUX_SPI_CNTL0_VARCS         (1 << 15) // Variable CS.
#define BCM2835_AUX_SPI_CNTL0_VARWIDTH      (1 << 14) // Variable width.
#define BCM2835_AUX_SPI_CNTL0_DOUTHOLD    ((x) << 12) // DOUT hold time.
#define BCM2835_AUX_SPI_CNTL0_ENABLE        (1 << 11) // Enable.
#define BCM2835_AUX_SPI_CNTL0_IRISING(x)  ((x) << 10) // In rising.
#define BCM2835_AUX_SPI_CNTL0_CLRFIFOS      (1 <<  9) // Clear FIFOs.
#define BCM2835_AUX_SPI_CNTL0_ORISING(x)  ((x) <<  8) // Out rising.
#define BCM2835_AUX_SPI_CNTL0_INVCLK(x)   ((x) <<  7) // Invert SPI.
#define BCM2835_AUX_SPI_CNTL0_MSB1ST(x)   ((x) <<  6) // Shift out MBB first.
#define BCM2835_AUX_SPI_CNTL0_SHIFTLEN(x) ((x) <<  0) // Shift length.


//  Aux SPI CNTL1 register. ---------------------------------------------------

#define BCM2835_AUX_SPI_CNTL1_CSHIGH(x)    ((x) << 8) // CS high time.
#define BCM2835_AUX_SPI_CNTL1_TXIRQ          (1 << 7) // TX empty IRQ.
#define BCM2835_AUX_SPI_CNTL1_DONEIRQ        (1 << 6) // Done IRQ.
#define BCM2835_AUX_SPI_CNTL1_MSB1ST(x)    ((x) << 1) // Shift in MSB first.
#define BCM2835_AUX_SPI_CNTL1_KEEPINP        (1 << 0) // Keep input.


//  Aux SPI STAT register. ----------------------------------------------------

#define BCM2835_AUX_SPI_STAT_TXFIFO(x)    ((x) << 28) // Tx FIFO level.
#define BCM2835_AUX_SPI_STAT_RXFIFO(x)    ((x) << 20) // Rx FIFO level.
#define BCM2835_AUX_SPI_STAT_TXFULL         (1 << 10) // Tx full.
#define BCM2835_AUX_SPI_STAT_TXEMPTY        (1 <<  9) // Tx empty.
#define BCM2835_AUX_SPI_STAT_RXEMPTY        (1 <<  7) // Rx empty.
#define BCM2835_AUX_SPI_STAT_BUSY           (1 <<  6) // Busy.
#define BCM2835_AUX_SPI_STAT_BITCNT(x)    ((x) <<  0) // Bit count.


//  SPI CLK register. ---------------------------------------------------------
/*
    This register allows the SPI clock rate to be set. For future use!

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

//  SPI DLEN register. --------------------------------------------------------
/*
    This register allows the SPI data length rate to be set. For future use!
            +---------------------------------------+
            | Bits  | Function  | Description       |
            |-------+-----------+-------------------|
            | 16-31 | ------    | Reserved.         |
            | 00-15 | LEN       | Data length.      |
            +---------------------------------------+
*/

//  SPI LTOH register. --------------------------------------------------------
/*
    This register allows the LOSSI output hold delay to be set. For future use!
            +-------------------------------------------+
            | Bits  | Function  | Description           |
            |-------+-----------+-----------------------|
            | 16-31 | ------    | Reserved.             |
            | 00-15 | TOH       | Output hold delay.    |
            +-------------------------------------------+
*/

//  SPI DC register. ----------------------------------------------------------
/*
    This register controls the generation of the DREQ and panic signals to an
    external DMA engine. For future use!

            +---------------------------------------------------+
            | Bits  | Function  | Description                   |
            |-------+-----------+-------------------------------|
            | 24-31 | RPANIC    | DMA read panic threshold.     |
            | 16-23 | RDREQ     | DMA request threshold.        |
            | 08-15 | TPANIC    | DMA write panic threshold.    |
            | 00-07 | TDREQ     | DMA write request threshold.  |
            +---------------------------------------------------+
*/
#define BCM2835_SPI_DC_RPANIC(x) ((x) << 24) // DMA read panic threshold.
#define BCM2835_SPI_DC_RDREQ(x)  ((x) << 16) // DMA read request threshold.
#define BCM2835_SPI_DC_TPANIC(x) ((x) <<  8) // DMA write panic threshold.
#define BCM2835_SPI_DC_TDREQ(x)  ((x) <<  0) // DMA write request threshold.


//  Software operation. -------------------------------------------------------
/*
    Polled:
        a)  Set CS, CPOL, CPHA as required and set TA = 1.
        b)  Poll TXD writing bytes to SPI_FIFO, RXD reading bytes from SPI_FIFO
            until all data written.
        c)  Poll DONE until it sets to 1.
        d)  Set TA = 0.

    Interrupt:
        a)  Set INTR and INTD.
        b)  Set CS, CPOL, CPHA as required and set TA = 1.
        c)  On interrupt...
        d)  If DONE is set and data to write, write up to 16 bytes to SPI_FIFO.
            If DONE is set and no more data, set TA=0. Read trailing data from
            SPI_FIFO until RxD is 0.
        e)  If RxR is set, read 12 bytes from SPI_FIFO and if more data to
            write, write up to 12 bytes to SPI_FIFO.

    DMA:
        a)  Enable DMA DREQs by setting DMAEN bit and ADCS if required.
        b)  Program two DMA control blocks, one for each controller.
        c)  DMA channel 1 control block should have its PER_MAP set to x and
            should be set to write "transfer length" + 1 words to SPI_FIFO.
            The data should comprise:

            1)  A word with the transfer length in bytes in the top sixteen
                bits and the control register settings [7:0] in the bottom
                eight bits, i.e. TA = 1, CS, CPOL, CPHA as required.
            2)  "Transfer length" number in words of data to send.

        d)  DMA channel 2 control block should have its PER_MAP set to y and
            should be set to read "transfer length' words from SPI_FIFO.
        e)  Point each DMA channel at its CB and set its ACTIVE to 1.
        f)  On recepit of an interrupt from DMA channel 2, the transfer is
            complete.
*/

//  BCM2835 functions. --------------------------------------------------------





#endif // #ifndef MCP42X1_H
