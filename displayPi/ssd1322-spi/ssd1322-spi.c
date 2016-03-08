// ============================================================================
/*
    ssd1322-spi:

    SSD1322 OLED display driver for the Raspberry Pi (4-wire SPI version).

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
// ============================================================================

#include <pigpio.h>

#include "ssd1322-spi.h"

// Default GPIO pins. ---------------------------------------------------------
struct ssd1322 ssd1322 =
{
    .gpio_sclk = GPIO_SPI0_SCLK,
    .gpio_sdin = GPIO_SPI0_MOSI,
    .gpio_cs   = GPIO_SPI0_CE0_N,
    .gpio_dc   = GPIO_DC,
    .gpio_res  = GPIO_RESET
};

char ssd1322_command[1]; // Command byte.
char ssd1322_options[2]; // Command option bytes.
char ssd1322_greys[16];  // Greyscale definitions.

// Hardware functions. --------------------------------------------------------

// ----------------------------------------------------------------------------
/*
    Triggers a hardware reset:

    Hardware is reset when RES# pin is pulled low.
    For normal operation RES# pin should be pulled high.
    Redundant if RES# pin is wired to VCC.
*/
// ----------------------------------------------------------------------------
void ssd1322_reset( uint8_t reset_gpio )
{
    gpioWrite( reset_gpio, SSD1322_HW_RESET );
    gpioWrite( reset_gpio, SSD1322_HW_RUN );
}

// ----------------------------------------------------------------------------
/*
    Sets column start & end addresses within display data RAM:

    0 <= start[6:0] < end[6:0] <= 0x77 (119).

    If horizontal address increment mode is enabled, column address pointer is
    automatically incremented after a column read/write. Column address pointer
    is reset after reaching the end column address.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_cols( uint8_t handle, uint8_t dc_gpio, char start, char end )
{
    if (( end   < start ) ||
        ( start > SSD1322_COLS_MAX ) ||
        ( end   > SSD1322_COLS_MAX )) return;

    ssd1322_command[0] = SSD1322_CMD_SET_COLS;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    ssd1322_options[0] = start;
    ssd1322_options[1] = end;
    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 2 );
}

// ----------------------------------------------------------------------------
/*
    Resets column start & end addresses within display data RAM:

    Start = 0x00.
    End   = 0x77 (119).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_cols_reset( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_COLS;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    ssd1322_options[0] = SSD1322_COLS_MIN;
    ssd1322_options[1] = SSD1322_COLS_MAX;
    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 2 );
}

// ----------------------------------------------------------------------------
/*
    Enables writing data continuously into display RAM.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_data_enable( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_WRITE;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Enables reading data continuously from display RAM. Not used in SPI mode.
*/
// ----------------------------------------------------------------------------
void ssd1322_read_data_enable( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_READ;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets column start & end addresses within display data RAM:

    0 <= Start[6:0] < End[6:0] <= 0x7f (127).

    If vertical address increment mode is enabled, column address pointer is
    automatically incremented after a column read/write. Column address pointer
    is reset after reaching the end column address.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_rows( uint8_t handle, uint8_t dc_gpio, char start, char end )
{
    if (( end   < start ) ||
        ( start > SSD1322_ROWS_MAX ) ||
        ( end   > SSD1322_ROWS_MAX )) return;

    ssd1322_command[0] = SSD1322_CMD_SET_ROWS;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    ssd1322_options[0] = start;
    ssd1322_options[1] = end;
    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 2 );
}

// ----------------------------------------------------------------------------
/*
    Resets row start & end addresses within display data RAM:

    Start = 0.
    End   = 0x7f (127).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_rows_reset( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_ROWS;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    ssd1322_options[0] = SSD1322_ROWS_MIN;
    ssd1322_options[1] = SSD1322_ROWS_MAX;
    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 2 );
}

// ----------------------------------------------------------------------------
/*
    Sets various addressing configurations.

    Address increment mode A[0]
        d1[0] = 0: Increment display RAM by column (horizontal).
        d1[0] = 1: Increment display RAM by row (vertical).
    Column address remap A[1]
        d1[1] = 0: Columns incremented left to right.
        d1[1] = 1: Columns incremented right to left.
    Nibble re-map A[2]
        d1[2] = 0: Direct mapping (reset).
        d1[2] = 1: The four nibbles of the RAM data bus are re-mapped.
    COM scan direction re-map A[4]
        d1[4] = 0: Rows incremented from top to bottom.
        d1[4] = 1: Rows incremented from bottom to top.
    Odd/even split of COM pins A[5]
        d1[5] = 0: Sequential pin assignment of COM pins.
        d1[5] = 1: Odd/even split of COM pins.
    Set dual COM mode B[4]
        d2[4] = 0: Disable dual COM mode.
        d2[4] = 1: Enable dual COM mode. A[5] must be disabled and
                   MUX <= 63.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_addr_modes( uint8_t handle, uint8_t dc_gpio, char d1, char d2 )
{
    // Can't check value of MUX without additional parameter.
    if ((( d2 & SSD1322_DUAL_ENABLE )  == SSD1322_DUAL_ENABLE ) &&
        (( d1 & SSD1322_SPLIT_ENABLE ) == SSD1322_SPLIT_ENABLE ))
        return;

    ssd1322_command[0] = SSD1322_CMD_SET_MODES;
    ssd1322_options[0] = d1;
    ssd1322_options[1] = d2;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 2 );
}

// ----------------------------------------------------------------------------
/*
    Sets display RAM start line.

    0 <= start <= 127 (0x7f).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_ram_start( uint8_t handle, uint8_t dc_gpio, char start )
{
    if ( start > SSD1322_RAM_MAX ) return;

    ssd1322_command[0] = SSD1322_CMD_SET_START;
    ssd1322_options[0] = start;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets display RAM offset.

    0 <= start <= 127 (0x7f).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_ram_offset( uint8_t handle, uint8_t dc_gpio, char offset )
{
    if ( offset > SSD1322_RAM_MAX ) return;

    ssd1322_command[0] = SSD1322_CMD_SET_OFFSET;
    ssd1322_options[0] = offset;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets display mode to normal (display shows contents of display RAM).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_normal( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PIX_NORM;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets all pixels on regardless of display RAM content.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_all_on( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PIX_ON;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets all pixels off regardless of display RAM content.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_all_off( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PIX_OFF;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Displays inverse of display RAM content.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_inverse( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PIX_INV;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Enables partial display mode (subset of available display).

    0 <= start <= end <= 0x7f.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_part_on( uint8_t handle, uint8_t dc_gpio, char start, char end )
{
    if (( end   < start ) ||
        ( start > SSD1322_ROWS_MAX ) ||
        ( end   > SSD1322_ROWS_MAX )) return;

    ssd1322_command[0] = SSD1322_CMD_SET_PART_ON;
    ssd1322_options[0] = start;
    ssd1322_options[1] = end;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 2 );
}

// ----------------------------------------------------------------------------
/*
    Enables partial display mode (subset of available display).

    0 <= start <= end <= 0x7f.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_part_off( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PART_OFF;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Enables internal VDD regulator.
*/
// ----------------------------------------------------------------------------
void ssd1322_vdd_enable( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_VDD;
    ssd1322_options[0] = SSD1322_VDD_INT;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Disables internal VDD regulator (requires external regulator).
*/
// ----------------------------------------------------------------------------
void ssd1322_vdd_disable( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_VDD;
    ssd1322_options[0] = SSD1322_VDD_EXT;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Turns display circuit on.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_on( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_DISP_ON;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Turns display circuit off.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_off( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_DISP_OFF;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets length of phase 1 & 2 of segment waveform driver.

    Phase 1: phase[3:0] Set from 5 DCLKS - 31 DCLKS in steps of 2. Higher OLED
             capacitance may require longer period to discharge.
    Phase 2: phase[7:4]Set from 3 DCLKS - 15 DCLKS in steps of 1. Higher OLED
             capacitance may require longer period to charge.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_clk_phase( uint8_t handle, uint8_t dc_gpio, char phase )
{
    ssd1322_command[0] = SSD1322_CMD_SET_CLK_PHASE;
    ssd1322_options[0] = phase;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets clock divisor/frequency.

    Front clock divider  A[3:0] = 0x0-0xa, reset = 0x1.
    Oscillator frequency A[7:4] = 0x0-0xf, reset = 0xc.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_clk_freq( uint8_t handle, uint8_t dc_gpio, char freq )
{
    ssd1322_command[0] = SSD1322_CMD_SET_CLK_FREQ;
    ssd1322_options[0] = freq;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets state of GPIO pins (GPIO0 & GPIO1).

    gpio[1:0] GPIO0
    gpio[3:2] GPIO1
*/
// ----------------------------------------------------------------------------
void ssd1322_set_gpios( uint8_t handle, uint8_t dc_gpio, char gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_GPIOS;
    ssd1322_options[0] = gpio;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets second pre-charge period.

    period[3:0] 0 to 15DCLK.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_period( uint8_t handle, uint8_t dc_gpio, char period )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PERIOD;
    ssd1322_options[0] = period;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets user-defined greyscale table GS1 - GS15 (Not GS0).

    gs[7:0] 0 <= GS1 <= GS2 .. <= GS15.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_greys( uint8_t handle, uint8_t dc_gpio, char *gs[16] )
{
    uint8_t i;

    ssd1322_command[0] = SSD1322_CMD_SET_GREYS;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    for ( i = 1; i < ( SSD1322_GREYSCALES - 1 ); i++ )
    {
        spiWrite( handle, gs[i], 1 );
    }
}

// ----------------------------------------------------------------------------
/*
    Sets greyscale lookup table to default linear values.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_greys_def( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_GREYS_DEF;
    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets pre-charge voltage level for segment pins with reference to VCC.

    voltage[4:0] 0.2xVCC (0x00) to 0.6xVCC (0x3e).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_pre_volt( uint8_t handle, uint8_t dc_gpio, uint8_t volts )
{
    ssd1322_command[0] = SSD1322_CMD_SET_PRE_VOLT;
    ssd1322_options[0] = volts;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets the high voltage level of common pins, VCOMH with reference to VCC.

    voltage[3:0] 0.72xVCC (0x00) to 0.86xVCC (0x07).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_com_volt( uint8_t handle, uint8_t dc_gpio, char volts )
{
    ssd1322_command[0] = SSD1322_CMD_SET_COM_VOLT;
    ssd1322_options[0] = volts;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets the display contrast (segment output current ISEG).

    contrast[7:0]. Contrast varies linearly from 0x00 to 0xff.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_contrast( uint8_t handle, uint8_t dc_gpio, char contrast )
{
    ssd1322_command[0] = SSD1322_CMD_SET_CONTRAST;
    ssd1322_options[0] = contrast;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets the display brightness (segment output current scaling factor).

    factor[3:0] 1/16 (0x00) to 16/16 (0x0f).
    The smaller the value, the dimmer the display.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_bright( uint8_t handle, uint8_t dc_gpio, char factor )
{
    ssd1322_command[0] = SSD1322_CMD_SET_BRIGHT;
    ssd1322_options[0] = factor;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets multiplex ratio (number of common pins enabled)

    ratio[6:0] 16 (0x00) to 128 (0x7f).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_mux( uint8_t handle, uint8_t dc_gpio, char ratio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_MUX;
    ssd1322_options[0] = ratio;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Sets command lock - prevents all command except unlock.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_lock( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_LOCK;
    ssd1322_options[0] = SSD1322_COMMAND_LOCK;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}

// ----------------------------------------------------------------------------
/*
    Unsets command lock.
*/
// ----------------------------------------------------------------------------
void ssd1322_command_unlock( uint8_t handle, uint8_t dc_gpio )
{
    ssd1322_command[0] = SSD1322_CMD_SET_LOCK;
    ssd1322_options[0] = SSD1322_COMMAND_UNLOCK;

    gpioWrite( dc_gpio, SSD1322_COMMAND );
    spiWrite( handle, ssd1322_command, 1 );

    gpioWrite( dc_gpio, SSD1322_DATA );
    spiWrite( handle, ssd1322_options, 1 );
}
