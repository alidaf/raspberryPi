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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <pigpio.h>

#include "ssd1322-spi.h"

// Data. ----------------------------------------------------------------------

struct ssd1322 *ssd1322[SSD1322_DISPLAYS];

char ssd1322_greys[16];  // Greyscale definitions.


// Hardware functions. --------------------------------------------------------

// ----------------------------------------------------------------------------
/*
    Writes a command.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_command( uint8_t id, uint8_t command )
{
    char spi[1];
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_LOW );
    gpioWrite( ssd1322[id]->gpio_dc, SSD1322_COMMAND );
    spi[0] = command;
    spiWrite( ssd1322[id]->handle, spi, 1 );
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_HIGH );
}

// ----------------------------------------------------------------------------
/*
    Writes a data byte.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_data( uint8_t id, uint8_t data )
{
    char spi[1];
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_LOW );
    gpioWrite( ssd1322[id]->gpio_dc, SSD1322_DATA );
    spi[0] = data;
    spiWrite( ssd1322[id]->handle, spi, 1 );
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_HIGH );
}

// ----------------------------------------------------------------------------
/*
    Writes a fast data byte. Write data enabled and CS# handled by calling
    function.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_fast( uint8_t id, uint8_t data )
{
    char spi[1];
    spi[0] = data;
    spiWrite( ssd1322[id]->handle, spi, 1 );
}

// ----------------------------------------------------------------------------
/*
    Writes a data stream.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_stream( uint8_t id, uint8_t *buf, unsigned count )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_WRITE + '0' );
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_LOW );
    spiWrite( ssd1322[id]->handle, (char*)buf, count );
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_HIGH );
}

// ----------------------------------------------------------------------------
/*
    Triggers a hardware reset:

    Hardware is reset when RES# pin is pulled low.
    For normal operation RES# pin should be pulled high.
    Redundant if RES# pin is wired to VCC.
*/
// ----------------------------------------------------------------------------
void ssd1322_reset( uint8_t id )
{
    gpioWrite( ssd1322[id]->gpio_reset, SSD1322_HW_RESET );
    gpioDelay( 1000 );
    gpioWrite( ssd1322[id]->gpio_reset, SSD1322_HW_RUN );
    gpioDelay( 1000 );
}

// ----------------------------------------------------------------------------
/*
    Enables grey scale lookup table.
*/
// ----------------------------------------------------------------------------
void ssd1322_enable_greys( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_ENABLE_GREYS );
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
void ssd1322_set_cols( uint8_t id, uint8_t start, uint8_t end )
{
    if (( end   < start ) ||
        ( start > SSD1322_COLS_MAX ) ||
        ( end   > SSD1322_COLS_MAX )) return;

    ssd1322_write_command( id, SSD1322_CMD_SET_COLS );
    ssd1322_write_data( id, start );
    ssd1322_write_data( id, end );
}

// ----------------------------------------------------------------------------
/*
    Resets column start & end addresses within display data RAM:

    Start = 0x00.
    End   = 0x77 (119).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_cols_reset( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_COLS );
    ssd1322_write_data( id, SSD1322_COLS_MIN );
    ssd1322_write_data( id, SSD1322_COLS_MAX );
}

// ----------------------------------------------------------------------------
/*
    Enables writing data continuously into display RAM.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_data_enable( uint8_t id )
{
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_LOW );
    ssd1322_write_command( id, SSD1322_CMD_SET_WRITE );
    // Need to call ssd1322_write_data_disable when finished fast writes.
}

// ----------------------------------------------------------------------------
/*
    Disables writing data continuously into display RAM.
*/
// ----------------------------------------------------------------------------
void ssd1322_write_data_disable( uint8_t id )
{
    gpioWrite( ssd1322[id]->gpio_cs, GPIO_HIGH );
}

// ----------------------------------------------------------------------------
/*
    Enables reading data continuously from display RAM. Not used in SPI mode.
*/
// ----------------------------------------------------------------------------
void ssd1322_read_data_enable( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_READ );
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
void ssd1322_set_rows( uint8_t id, uint8_t start, uint8_t end )
{
    if (( end   < start ) ||
        ( start > SSD1322_ROWS_MAX ) ||
        ( end   > SSD1322_ROWS_MAX )) return;

    ssd1322_write_command( id, SSD1322_CMD_SET_ROWS );
    ssd1322_write_data( id, start );
    ssd1322_write_data( id, end );
}

// ----------------------------------------------------------------------------
/*
    Resets row start & end addresses within display data RAM:

    Start = 0.
    End   = 0x7f (127).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_rows_reset( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_ROWS );
    ssd1322_write_data( id, SSD1322_ROWS_MIN );
    ssd1322_write_data( id, SSD1322_ROWS_MAX );
}

// ----------------------------------------------------------------------------
/*
    Sets various addressing configurations.

    a[0] = 0: Increment display RAM by column (horizontal).
    a[0] = 1: Increment display RAM by row (vertical).
    a[1] = 0: Columns incremented left to right.
    a[1] = 1: Columns incremented right to left.
    a[2] = 0: Direct mapping (reset).
    a[2] = 1: The four nibbles of the RAM data bus are re-mapped.
    a[3] = 0.
    a[4] = 0: Rows incremented from top to bottom.
    a[4] = 1: Rows incremented from bottom to top.
    a[5] = 0: Sequential pin assignment of COM pins.
    a[5] = 1: Odd/even split of COM pins.
    a[7:6] = 0x00.
    b[3:0] = 0x01.
    b[4] = 0: Disable dual COM mode.
    b[4] = 1: Enable dual COM mode. A[5] must be disabled and MUX <= 63.
    b[5] = 0.

    Typical values: a = 0x14, b = 0x11.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_addr_modes( uint8_t id, uint8_t a, uint8_t b )
{
    // Can't check value of MUX without additional parameter.
    if ((( b & SSD1322_DUAL_ENABLE )  == SSD1322_DUAL_ENABLE ) &&
        (( a & SSD1322_SPLIT_ENABLE ) == SSD1322_SPLIT_ENABLE ))
        return;

    ssd1322_write_command( id, SSD1322_CMD_SET_MODES );
    ssd1322_write_data( id, a );
    ssd1322_write_data( id, b );
}

// ----------------------------------------------------------------------------
/*
    Sets display RAM start line.

    0 <= start <= 127 (0x7f).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_ram_start( uint8_t id, uint8_t start )
{
    if ( start > SSD1322_RAM_MAX ) return;

    ssd1322_write_command( id, SSD1322_CMD_SET_START );
    ssd1322_write_data( id, start );
}

// ----------------------------------------------------------------------------
/*
    Sets display RAM offset.

    0 <= start <= 127 (0x7f).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_ram_offset( uint8_t id, uint8_t offset )
{
    if ( offset > SSD1322_RAM_MAX ) return;

    ssd1322_write_command( id, SSD1322_CMD_SET_OFFSET );
    ssd1322_write_data( id, offset );
}

// ----------------------------------------------------------------------------
/*
    Sets display mode to normal (display shows contents of display RAM).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_normal( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PIX_NORM );
}

// ----------------------------------------------------------------------------
/*
    Sets all pixels on regardless of display RAM content.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_all_on( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PIX_ON );
}

// ----------------------------------------------------------------------------
/*
    Sets all pixels off regardless of display RAM content.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_all_off( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PIX_OFF );
}

// ----------------------------------------------------------------------------
/*
    Displays inverse of display RAM content.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_inverse( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PIX_INV );
}

// ----------------------------------------------------------------------------
/*
    Enables partial display mode (subset of available display).

    0 <= start <= end <= 0x7f.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_part_on( uint8_t id, uint8_t start, uint8_t end )
{
    if (( end   < start ) ||
        ( start > SSD1322_ROWS_MAX ) ||
        ( end   > SSD1322_ROWS_MAX )) return;

    ssd1322_write_command( id, SSD1322_CMD_SET_PART_ON );
    ssd1322_write_data( id, start );
    ssd1322_write_data( id, end );
}

// ----------------------------------------------------------------------------
/*
    Enables partial display mode (subset of available display).

    0 <= start <= end <= 0x7f.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_part_off( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PART_OFF );
}

// ----------------------------------------------------------------------------
/*
    Enables internal VDD regulator.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_vdd_internal( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_VDD );
    ssd1322_write_data( id, SSD1322_VDD_INT );
}

// ----------------------------------------------------------------------------
/*
    Disables internal VDD regulator (requires external regulator).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_vdd_external( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_VDD );
    ssd1322_write_data( id, SSD1322_VDD_EXT );
}

// ----------------------------------------------------------------------------
/*
    Turns display circuit on.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_on( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_DISP_ON );
}

// ----------------------------------------------------------------------------
/*
    Turns display circuit off.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_display_off( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_DISP_OFF );
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
void ssd1322_set_clk_phase( uint8_t id, uint8_t phase )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_CLK_PHASE );
    ssd1322_write_data( id, phase );
}

// ----------------------------------------------------------------------------
/*
    Sets clock divisor/frequency.

    Front clock divider  A[3:0] = 0x0-0xa, reset = 0x1.
    Oscillator frequency A[7:4] = 0x0-0xf, reset = 0xc.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_clk_freq( uint8_t id, uint8_t freq )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_CLK_FREQ );
    ssd1322_write_data( id, freq );
}

// ----------------------------------------------------------------------------
/*
    Sets display enhancement A.

    a[1:0] 0x00 = external VSL, 0x02 = internal VSL.
    a[7:2] = 0xa0.
    b[2:0] = 0x05.
    b[7:3] 0xf8 = enhanced, 0xb0 = normal.

    Typical values: a = 0xa0, b = 0xfd.

*/
// ----------------------------------------------------------------------------
void ssd1322_set_enhance_a( uint8_t id, uint8_t a, uint8_t b )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_ENHANCE_A );
    ssd1322_write_data( id, a );
    ssd1322_write_data( id, b );
}

// ----------------------------------------------------------------------------
/*
    Sets state of GPIO pins (GPIO0 & GPIO1).

    gpio[1:0] GPIO0
    gpio[3:2] GPIO1
*/
// ----------------------------------------------------------------------------
void ssd1322_set_gpios( uint8_t id, uint8_t gpio )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_GPIOS );
    ssd1322_write_data( id, gpio );
}

// ----------------------------------------------------------------------------
/*
    Sets second pre-charge period.

    period[3:0] 0 to 15DCLK.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_period( uint8_t id, uint8_t period )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PERIOD );
    ssd1322_write_data( id, period );
}

// ----------------------------------------------------------------------------
/*
    Sets user-defined greyscale table GS1 - GS15 (Not GS0).

    gs[7:0] 0 <= GS1 <= GS2 .. <= GS15.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_greys( uint8_t id, uint8_t gs[SSD1322_GREYSCALES] )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_GREYS );
    ssd1322_write_stream( id, gs, SSD1322_GREYSCALES );
}

// ----------------------------------------------------------------------------
/*
    Sets greyscale lookup table to default linear values.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_greys_def( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_GREYS_DEF );
}

// ----------------------------------------------------------------------------
/*
    Sets pre-charge voltage level for segment pins with reference to VCC.

    voltage[4:0] 0.2xVCC (0x00) to 0.6xVCC (0x3e).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_pre_volt( uint8_t id, uint8_t volts )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_PRE_VOLT );
    ssd1322_write_data( id, volts );
}

// ----------------------------------------------------------------------------
/*
    Sets the high voltage level of common pins, VCOMH with reference to VCC.

    voltage[3:0] 0.72xVCC (0x00) to 0.86xVCC (0x07).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_com_volt( uint8_t id, uint8_t volts )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_COM_VOLT );
    ssd1322_write_data( id, volts );
}

// ----------------------------------------------------------------------------
/*
    Sets the display contrast (segment output current ISEG).

    contrast[7:0]. Contrast varies linearly from 0x00 to 0xff.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_contrast( uint8_t id, uint8_t contrast )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_CONTRAST );
    ssd1322_write_data( id, contrast );
}

// ----------------------------------------------------------------------------
/*
    Sets the display brightness (segment output current scaling factor).

    factor[3:0] 1/16 (0x00) to 16/16 (0x0f).
    The smaller the value, the dimmer the display.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_brightness( uint8_t id, uint8_t factor )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_BRIGHT );
    ssd1322_write_data( id, factor );
}

// ----------------------------------------------------------------------------
/*
    Sets multiplex ratio (number of common pins enabled)

    ratio[6:0] 16 (0x00) to 128 (0x7f).
*/
// ----------------------------------------------------------------------------
void ssd1322_set_mux( uint8_t id, uint8_t ratio )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_MUX );
    ssd1322_write_data( id, ratio );
}

// ----------------------------------------------------------------------------
/*
    Sets display enhancement B.

    a[3:0] = 0x02.
    a[5:4] 0x00 = reserved, 0x02 = normal (reset).
    a[7:6] = 0x80.
    b[7:0] = 0x20.

    typical values: a = 0x82, b = 0x20.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_enhance_b( uint8_t id, uint8_t a, uint8_t b )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_ENHANCE_B );
    ssd1322_write_data( id, a );
    ssd1322_write_data( id, b );
}

// ----------------------------------------------------------------------------
/*
    Sets command lock - prevents all command except unlock.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_lock( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_LOCK );
    ssd1322_write_data( id, SSD1322_COMMAND_LOCK );
}

// ----------------------------------------------------------------------------
/*
    Unsets command lock.
*/
// ----------------------------------------------------------------------------
void ssd1322_command_unlock( uint8_t id )
{
    ssd1322_write_command( id, SSD1322_CMD_SET_LOCK );
    ssd1322_write_data( id, SSD1322_COMMAND_UNLOCK );
}

// ----------------------------------------------------------------------------
/*
    Clears display RAM.
*/
// ----------------------------------------------------------------------------
void ssd1322_clear_ram( uint8_t id )
{
    uint8_t col, row;
    ssd1322_set_cols_reset( id );
    ssd1322_set_rows_reset( id );
    ssd1322_write_data_enable( id );
    for ( row = SSD1322_ROWS_MIN; row < SSD1322_ROWS_MAX + 1; row++ )
    {
        for ( col = SSD1322_COLS_MIN; col < SSD1322_COLS_MAX + 1; col++ )
        {
            spiWrite( id, 0x00, 1 );
        }
    }
    ssd1322_write_data_disable( id );
}

// ----------------------------------------------------------------------------
/*
    Sets default display properties.
*/
// ----------------------------------------------------------------------------
void ssd1322_set_default( uint8_t id )
{
    // Typical init sequence.
    ssd1322_command_unlock( id );               // Unlock commands.
    ssd1322_set_display_off( id );              // Display off.
    ssd1322_set_clk_freq( id, 0xf3 );           // Clock frequency.
    ssd1322_set_mux( id, 0x3f );                // Multiplex ratio.
    ssd1322_set_ram_offset( id, 0x00 );         // RAM offset.
    ssd1322_set_ram_start( id, 0x00 );          // RAM start.
    ssd1322_set_addr_modes( id, 0x14, 0x11 );   // Address modes.
    ssd1322_set_vdd_internal( id );             // Internal VDD.

    ssd1322_set_enhance_a( id, 0xa0, 0xfd );    // Enhancement A.

    ssd1322_set_contrast( id, 0xff );           // Contrast.
    ssd1322_set_brightness( id, 0x0f );         // Brightness.
    ssd1322_set_clk_phase( id, 0xf0 );          // Clock phase length.

    ssd1322_set_enhance_b( id, 0x82, 0x20 );    // Enhancement B.

    ssd1322_set_pre_volt( id, 0x0d );           // Pre-charge voltage.
    ssd1322_set_period( id, 0x08 );             // Pre-charge period.
    ssd1322_set_com_volt( id, 0x00 );           // VCOMH.

    ssd1322_set_display_on( id );               // Display on.
    ssd1322_set_display_all_on( id );           // All pixels on.
    ssd1322_clear_ram( id );                    // Flush RAM.

    ssd1322_set_display_normal( id );           // Normal display.
}

// ----------------------------------------------------------------------------
/*
    Initialises display.
*/
// ----------------------------------------------------------------------------
int8_t ssd1322_init( uint8_t sclk,
                     uint8_t sdin,
                     uint8_t cs,
                     uint8_t dc,
                     uint8_t reset )
{

    struct ssd1322 *ssd1322_this; // Display instance.
    static bool init = false;     // 1st Call to init.

    int8_t  id = -1;    // Display handle.
    uint8_t spi;        // SPI handle.
    uint8_t display;    // Counter.

    // 1st call - clear ssd1322 structs.
    if ( !init)
    {
        for ( display = 0; display < SSD1322_DISPLAYS; display++ )
        ssd1322[display] = NULL;
        gpioInitialise();
    }

    // Get next available display ID.
    for ( display = 0; display < SSD1322_DISPLAYS; display++ )
    {
        if ( ssd1322[display] == NULL )
        {
            id = display;
            display = SSD1322_DISPLAYS;
            spi = spiOpen( 0, SSD1322_SPI_BAUD, SSD1322_SPI_FLAGS );
        }
    }

    if ( id < 0 ) return id;

    // Allocate memory for display struct.
    ssd1322_this = malloc( sizeof( struct ssd1322 ));

    // Return if no memory.
    if ( ssd1322_this == NULL ) return -2;

    // Create instance for display being initialised.
    ssd1322_this->handle     = spi;
    ssd1322_this->gpio_sclk  = sclk;
    ssd1322_this->gpio_sdin  = sdin;
    ssd1322_this->gpio_cs    = cs;
    ssd1322_this->gpio_dc    = dc;
    ssd1322_this->gpio_reset = reset;
    ssd1322[id] = ssd1322_this;

    init = true;

    ssd1322_set_default( id );

    return id;
}
