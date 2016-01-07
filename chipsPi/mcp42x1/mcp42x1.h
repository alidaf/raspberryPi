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

#define MCP42X1_VERSION 1.01

//  ===========================================================================
/*
    Authors:        D.Faulke    07/01/2016

    Contributors:

    Changelog:

        v1.00   Original version.
        v1.01   Rewrote SPI routines with direct BCM2835 register access.
*/
//  ===========================================================================

#ifndef MCP42X1_H
#define MCP42X1_H

/*
    This driver includes code to access the SPI interface directly rather than
    rely on external libraries.

    The Pi A+, B & B+ models have a Broadcom BCM2835 chip to provide the pin
    capabilities, while the 2B model has a BCM2836. In order to retain
    backward compatibility, the GPIO functions are based on the BCM2835.
*/

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
*/


//  MCP42x1 data. -------------------------------------------------------------

//  Max number of MCP42X1 chips.
#define MCP42X1_MAX 2 // SPI0 has 2 chip selects but AUX has 3.
/*
    Mote:   It is possible to handle the chip select independently of the SPI
            controller and therefore have many more but that is outside the
            scope of this driver.
*/

//  MCP42x1 register addresses.
enum mcp42x1_registers
{
     MCP42X1_REG_WIPER0 = 0x00, // Wiper for resistor network 0.
     MCP42X1_REG_WIPER1 = 0x01, // Wiper for resistor network 1.
     MCP42X1_REG_TCON   = 0x04, // Terminal control.
     MCP42X1_REG_STATUS = 0x05  // Status.
};

//  TCON register masks.
enum mcp42x1_tcon
{
    MCP42X1_TCON_R0B  = 0x01, // Resistor newtork 0, pin B.
    MCP42X1_TCON_R0W  = 0x02, // Resistor network 0, wiper.
    MCP42X1_TCON_R0A  = 0x04, // Resistor network 0, pin A.
    MCP42X1_TCON_R0HW = 0x08, // Resistor network 0, hardware configuration.
    MCP42X1_TCON_R1B  = 0x10, // Resistor network 1, pin B.
    MCP42X1_TCON_R1W  = 0x20, // Resistor network 1, wiper.
    MCP42X1_TCON_R1A  = 0x40, // Resistor network 1, pin A.
    MCP42X1_TCON_R1HW = 0x80  // Resistor network 1, hardware configuration.
};

struct mcp42x1
{
    uint8_t id; // SPI handle.
    uint8_t cs; // Chip Select.
};

struct mcp42x1 *mcp42x1[MCP42X1_MAX];


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

    There doesn't appear to be much in it yet!
*/


//  Peripheral base address (for non-deice tree versions). --------------------
static volatile uint32_t bcm2835_peri_base = 0x20000000; // Model 1.


//  Offsets for specific peripheral registers. ---------------------------------
static const uint32_t bcm2835_gpio_base = bcm2835_peri_base + 0x200000;
static const uint32_t bcm2835_spi_base  = bcm2835_peri_base + 0x204000;
static const uint32_t bcm2835_aux_base  = bcm2835_peri_base + 0x215000;


//  RPi memory specifics - see Raspberry Pi Hardware Reference. ---------------
static const uint8_t bcm2835_page_size  = 4 * 1024;
static const uint8_t bcm2835_block_size = 4 * 1024;


//  Register offsets from BCM2835_GPIO_BASE. ----------------------------------
enum bcm2835_gpio_registers     // BCM2835 ARM Peripherals, section 6.1
{
    BCM2835_GPFSEL0     = 0x00, // GPIO Function Select 0.
    BCM2835_GPFSEL1     = 0x04, // GPIO Function Select 1.
    BCM2835_GPFSEL2     = 0x08, // GPIO Function Select 2.
    BCM2835_GPFSEL3     = 0x0c, // GPIO Function Select 3.
    BCM2835_GPFSEL4     = 0x10, // GPIO Function Select 4.
    BCM2835_GPFSEL5     = 0x14, // GPIO Function Select 5.
    //                  = 0x18, // Reserved
    BCM2835_GPSET0      = 0x1c, // GPIO pin output set 0.
    BCM2835_GPSET1      = 0x20, // GPIO pin output set 1.
    //                  = 0x24, // Reserved.
    BCM2835_GPCLR0      = 0x28, // GPIO pin output clear 0.
    BCM2835_GPCLR1      = 0x2c, // GPIO pin output clear 1.
    //                  = 0x30, // Reserved.
    BCM2835_GPLEV0      = 0x34, // GPIO pin level 0.
    BCM2835_GPLEV1      = 0x38, // GPIO pin level 1.
    //                  = 0x3c, // Reserved.
    BCM2835_GPEDS0      = 0x40, // GPIO pin event detect status 0.
    BCM2835_GPEDS1      = 0x44, // GPIO pin event detect status 1.
    //                  = 0x48, // Reserved.
    BCM2835_GPREN0      = 0x4c, // GPIO pin rising edge detect enable 0.
    BCM2835_GPREN1      = 0x50, // GPIO pin rising edge detect enable 1.
    //                  = 0x54, // Reserved.
    BCM2835_GPFEN0      = 0x58, // GPIO pin falling edge detect enable 0.
    BCM2835_GPFEN1      = 0x5c, // GPIO pin falling edge detect enable 1.
    //                  = 0x60, // Reserved.
    BCM2835_GPHEN0      = 0x64, // GPIO pin high detect enable 0.
    BCM2835_GPHEN1      = 0x68, // GPIO pin high detect enable 1.
    //                  = 0x6c, // Reserved.
    BCM2835_GPLEN0      = 0x70, // GPIO pin low detect enable 0.
    BCM2835_GPLEN0      = 0x74, // GPIO pin low detect enable 1.
    //                  = 0x78, // Reserved.
    BCM2835_GPAREN0     = 0x7c, // GPIO pin async rising edge detect 0.
    BCM2835_GPAREN0     = 0x80, // GPIO pin async rising edge detect 1.
    //                  = 0x84, // Reserved.
    BCM2835_GPAFEN0     = 0x88, // GPIO pin async falling edge detect 0.
    BCM2835_GPAFEN1     = 0x8c, // GPIO pin async falling edge detect 1.
    //                  = 0x90, // Reserved.
    BCM2835_GPPUD       = 0x94, // GPIO pin pull-up/down enable.
    BCM2835_GPPUDCLK0   = 0x98, // GPIO pin pull-up/down enable clock 0.
    BCM2835_GPPUDCLK0   = 0x9c  // GPIO pin pull-up/down enable clock 1.
    //                  = 0xa0, // Reserved.
    //                  = 0xb0  // Test.
};

enum bcm2835_aux_registers
{
    BCM2835_AUX_IRQ             = 0x00, // Auxiliary interrupt status.
    BCM2835_AUX_ENABLES         = 0x04, // Auxiliary enables.
    BCM2835_AUX_MU_IO_REG       = 0x40, // Mini UART I/O data.
    BCM2835_AUX_MU_IER_REG      = 0x44, // Mini UART interrupt enable.
    BCM2835_AUX_MU_IIR_REG      = 0x48, // Mini UART interrupt identify.
    BCM2835_AUX_MU_LCR_REG      = 0x4c, // Mini UART line control.
    BCM2835_AUX_MU_MCR_REG      = 0x50, // Mini UART MODEM control.
    BCM2835_AUX_MU_LSR_REG      = 0x54, // Mini UART line status.
    BCM2835_AUX_MU_MSR_REG      = 0x58, // Mini UART MODEM status.
    BCM2835_AUX_MU_SCRATCH      = 0x5c, // Mini UART scratch.
    BCM2835_AUX_MU_CNTL_REG     = 0x60, // Mini UART extra control.
    BCM2835_AUX_MU_STAT_REG     = 0x64, // Mini UART extra status.
    BCM2835_AUX_MU_BAUD_REG     = 0x68, // Mini UART BAUD rate.
    BCM2835_AUX_SPI0_CNTL0_REG  = 0x80, // SPI1 control register 0.
    BCM2835_AUX_SPI0_CNTL1_REG  = 0x84, // SPI1 control register 1.
    BCM2835_AUX_SPI0_STAT_REG   = 0x88, // SPI1 status.
    BCM2835_AUX_SPI0_IO_REG     = 0x90, // SPI1 data.
    BCM2835_AUX_SPI0_PEEK_REG   = 0x94, // SPI1 peek.
    BCM2835_AUX_SPI0_CNTL0_REG  = 0xc0, // SPI2 control register 0.
    BCM2835_AUX_SPI0_CNTL1_REG  = 0xc4, // SPI2 control register 1.
    BCM2835_AUX_SPI0_STAT_REG   = 0xc8, // SPI2 status.
    BCM2835_AUX_SPI0_IO_REG     = 0xd0, // SPI2 data.
    BCM2835_AUX_SPI0_PEEK_REG   = 0xd4  // SPI2 peek.
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

    Interrupt:  Too advanced for me!

    DMA:        Too advanced for me!

*/

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

#endif // #ifndef MCP42X1_H
