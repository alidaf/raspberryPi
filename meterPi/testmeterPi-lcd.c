//  ===========================================================================
/*
    testmeterPi-lcd:

    Tests meterPi using a HD44780 based 16x2 LCD.

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
/*
    Compile with:

        gcc -c -Wall meterPi.c testmeterPi-lcd.c -o testmeterPi-lcd
               -lm -lpthread -lrt -lncurses

    For Raspberry Pi v1 optimisation use the following flags:

        -march=armv6zk -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

    For Raspberry Pi v2 optimisation use the following flags:

        -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4
        -ffast-math -pipe -O3
*/
//  ===========================================================================
/*
    Authors:        D.Faulke            12/02/2016

    Contributors:
*/
//  ===========================================================================

//  Installed libraries -------------------------------------------------------

#include <stdbool.h>

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

//  Local libraries -------------------------------------------------------

#include "meterPi.h"
#include "hd44780i2c.h"
#include "mcp23017.h"

//  Information. --------------------------------------------------------------
/*
    For testing, a HD44780 based 16x2 display was connected to an MCP23017 port
    expander for I2C control using the displayPi library.

                       GND
                        |    10k
        +-----------+   +---\/\/\--x
        | pin | Fn  |   |     |
        |-----+-----|   |     |   ,----------------------------------,
        |   1 | VSS |---'     |   | ,--------------------------------|-,
        |   2 | VDD |--> 5V   |   | | ,------------------------------|-|-,
        |   3 | Vo  |---------'   | | |                              | | |
        |   4 | RS  |-------------' | | +-----------( )-----------+  | | |
        |   5 | R/W |---------------' | |  Fn  | pin | pin |  Fn  |  | | |
        |   6 | E   |-----------------' |------+-----+-----+------|  | | |
        |   7 | DB0 |------------------>| GPB0 |  01 | 28  | GPA7 |<-' | |
        |   8 | DB1 |------------------>| GPB1 |  02 | 27  | GPA6 |<---' |
        |   9 | DB2 |------------------>| GPB2 |  03 | 26  | GPA5 |<-----'
        |  10 | DB3 |------------------>| GPB3 |  04 | 25  | GPA4 |
        |  11 | DB4 |------------------>| GPB4 |  05 | 24  | GPA3 |
        |  12 | DB5 |------------------>| GPB5 |  06 | 23  | GPA2 |
        |  13 | DB6 |------------------>| GPB6 |  07 | 22  | GPA1 |
        |  14 | DB7 |------------------>| GPB7 |  08 | 21  | GPA0 |
        |  15 | A   |--+----------------|  VDD |  09 | 20  | INTA |
        |  16 | K   |--|----+-----------|  VSS |  10 | 19  | INTB |
        +-----------+  |    |           |   NC |  11 | 18  | RST  |-----> +5V
                       |    |      ,----|  SCL |  12 | 17  | A2   |---,
                       |    |      |  ,-|  SDA |  13 | 16  | A1   |---+-> GND
                       |    |      |  | |   NC |  14 | 15  | A0   |---'
                       v    v      |  | +-------------------------+
                      +5V  GND     |  |
                                   |  |
                                   v  v
                                SCL1  SDA1

    Notes:  Vo is connected to the wiper of a 10k trim pot to adjust the
            contrast. A similar (perhaps 5k) pot could be used to adjust the
            backlight but connecting to 3.3V works OK instead. These displays
            are commonly sold with a single 10k pot.

            * The HD44780 operates slightly faster at 5V but 3.3V works fine.
            The MCP23017 expander can operate at both levels. The Pi's I2C
            pins have pull-up resistors that should protect against 5V levels
            but use a logic level shifter if there is any doubt.
*/

//  Functions. ----------------------------------------------------------------

#define METER_LEVELS 16 // 16x2 LCD.
#define METER_DELAY 1

pthread_mutex_t displayBusy;

// Meter labels.
//char lcd_meter[METER_CHANNELS][METER_LEVELS + 1] = {{ 0x00 }, { 0x01 }};
char lcd_meter[METER_CHANNELS][METER_LEVELS + 1] =
    {{ 0, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, '\0' },
     { 1, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, 5, 3, '\0' }};

struct peak_meter_t peak_meter =
{
    .int_time       = 1,
    .samples        = 2,
    .hold_time      = 500,
    .hold_count     = 3,
    .num_levels     = 16,
    .floor          = -80,
    .reference      = 32768,
    .dBfs           = { 0, 0 },
    .bar_index      = { 0, 0 },
    .dot_index      = { 0, 0 },
    .elapsed        = { 0, 0 },
    .scale          = {  -48,  -42,  -36,  -30,  -24,  -20,  -18,  -16,
                         -14,  -12,  -10,   -8,   -6,   -4,   -2,    0  }
};

//  ---------------------------------------------------------------------------
//  Produces string representations of the peak meters.
//  ---------------------------------------------------------------------------
/*
    This is intended for a small LCD (16x2 or similar).
*/
void get_peak_strings( struct peak_meter_t peak_meter,
                       char dB_string[METER_CHANNELS]
                                     [METER_LEVELS + 1] )
{
    uint8_t channel;
    uint8_t i;

    for ( channel = 0; channel < METER_CHANNELS; channel++ )
    {
        for ( i = 1; i < peak_meter.num_levels; i++ )
        {
            if (( i <= peak_meter.bar_index[channel] ) ||
                ( i == peak_meter.dot_index[channel] ))
            {
                if ( channel == 0 )
//                    dB_string[channel][i] = 0x06;
                    dB_string[channel][i] = 6;
                else
//                    dB_string[channel][i] = 0x07;
                    dB_string[channel][i] = 7;
            }
            else
            {
                if ( channel == 0 )
                {
                    if ( i % 2 == 0 )
//                        dB_string[channel][i] = 0x02;
                        dB_string[channel][i] = 2;
                    else
//                        dB_string[channel][i] = 0x04;
                        dB_string[channel][i] = 4;
                }
                else
                {
                    if ( i % 2 == 0 )
//                        dB_string[channel][i] = 0x03;
                        dB_string[channel][i] = 3;
                    else
//                        dB_string[channel][i] = 0x05;
                        dB_string[channel][i] = 5;
                }
            }
        }
        dB_string[channel][i] = '\0';
    }
}


//  ---------------------------------------------------------------------------
//  Updates Meter display.
//  ---------------------------------------------------------------------------
void *update_meter()
{
    while ( 1 )
    {
        get_dBfs( &peak_meter );
        get_dB_indices( &peak_meter );
        get_peak_strings( peak_meter, lcd_meter );

        pthread_mutex_lock( &displayBusy );
        hd44780Goto( mcp23017[0], hd44780[0], 0, 0 );
        hd44780WriteString( mcp23017[0], hd44780[0], lcd_meter[0], 16 );
        hd44780Goto( mcp23017[0], hd44780[0], 1, 0 );
        hd44780WriteString( mcp23017[0], hd44780[0], lcd_meter[1], 16 );
        pthread_mutex_unlock( &displayBusy );

        usleep( METER_DELAY );
    }
}


//  ---------------------------------------------------------------------------
//  Main (functional test).
//  ---------------------------------------------------------------------------
int main( void )
{
    struct timeval start, end;
    uint32_t elapsed; // Elapsed time in milliseconds.
    uint8_t i;


    struct display_mode_t
    {
        bool data;      // Data mode (4-bit or 8-bit).
        bool lines;     // Display lines.
        bool font;      // Font.
        bool display;   // Display on/off.
        bool cursor;    // Cursor on/off.
        bool blink;     // Blink (block cursor) on/off.
        bool counter;   // Counter incrementation.
        bool shift;     // Display shift.
        bool mode;      // Cursor mode.
        bool direction; // Cursor direction.
    }
        display_mode =
    {
        .data      = 1,  // 8-bit mode.
        .lines     = 1,  // 2 display lines.
        .font      = 1,  // 5x8 font.
        .display   = 1,  // Display on.
        .cursor    = 0,  // Cursor off.
        .blink     = 0,  // Blink (block cursor) off.
        .counter   = 1,  // Increment DDRAM counter after data write
        .shift     = 0,  // Do not shift display after data write.
        .mode      = 0,  // Shift cursor.
        .direction = 0,  // Right.
    };

    struct hd44780 *hd44780this;

    int8_t err;

    // Initialise MCP23017.
    err = mcp23017Init( 0x20 );
    if ( err < 0 )
    {
        printf( "Couldn't init. Try loading i2c-dev module.\n" );
        return -1;
    }

    // Set direction of GPIOs.
    mcp23017WriteByte( mcp23017[0], IODIRA, 0x00 ); // Output.
    mcp23017WriteByte( mcp23017[0], IODIRB, 0x00 ); // Output.

    // Writes to latches are the same as writes to GPIOs.
    mcp23017WriteByte( mcp23017[0], OLATA, 0x00 ); // Clear pins.
    mcp23017WriteByte( mcp23017[0], OLATB, 0x00 ); // Clear pins.

    // Set BANK bit for 8-bit mode.
    mcp23017WriteByte( mcp23017[0], IOCONA, 0x80 );

    hd44780this = malloc( sizeof( struct hd44780 ));


    // Set up hd44780 data.
    /*
        +---------------------------------------------------------------+
        |             GPIOB             |             GPIOA             |
        |-------------------------------+-------------------------------|
        | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
        |---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---|
        |DB7|DB6|DB5|DB4|DB3|DB2|DB1|DB0|RS |R/W| E |---|---|---|---|---|
        +---------------------------------------------------------------+
    */
    hd44780this->rs    = 0x80; // HD44780 RS pin.
    hd44780this->rw    = 0x40; // HD44780 R/W pin.
    hd44780this->en    = 0x20; // HD44780 E pin.

    hd44780[0] = hd44780this;

    // Initialise display.
    hd44780Init( mcp23017[0],
                 hd44780[0],
                 display_mode.data,
                 display_mode.lines,
                 display_mode.font,
                 display_mode.display,
                 display_mode.cursor,
                 display_mode.blink,
                 display_mode.counter,
                 display_mode.shift,
                 display_mode.mode,
                 display_mode.direction );

//    hd44780WriteString( mcp23017[0], hd44780[0], "Initialised" );

    // Custom characters.
    const uint8_t meter_chars[CUSTOM_MAX][CUSTOM_SIZE] =
        {{ 0x1f, 0x17, 0x17, 0x17, 0x17, 0x17, 0x11, 0x1f },
         { 0x1f, 0x11, 0x15, 0x11, 0x13, 0x15, 0x15, 0x1f },
         { 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x1f },
         { 0x1f, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00 },
         { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x1f },
         { 0x1f, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
         { 0x1f, 0x1d, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f },
         { 0x1f, 0x1d, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f }};

    hd44780LoadCustom( mcp23017[0], hd44780[0], meter_chars );

    vis_check();

    pthread_mutex_init( &displayBusy, NULL );
    pthread_t threads[1];

    // Calculate number of samples for integration time.
	peak_meter.samples = vis_get_rate() * peak_meter.int_time / 1000;
    if ( peak_meter.samples < 1 ) peak_meter.samples = 1;
    peak_meter.samples = 2; // Minimum samples for fastest response but may miss peaks.
    printf( "Samples for %dms = %d.\n", peak_meter.int_time, peak_meter.samples );

    // Do some loops to test response time.
    gettimeofday( &start, NULL );
    for ( i = 0; i < TEST_LOOPS; i++ )
    {
        get_dBfs( &peak_meter );
        get_dB_indices( &peak_meter );
        get_peak_strings( peak_meter, lcd_meter );

        // Write to LCD.
        hd44780Goto( mcp23017[0], hd44780[0], 0, 0 );
        hd44780WriteString( mcp23017[0], hd44780[0], lcd_meter[0], 16 );
        hd44780Goto( mcp23017[0], hd44780[0], 1, 0 );
        hd44780WriteString( mcp23017[0], hd44780[0], lcd_meter[1], 16 );
        usleep( METER_DELAY );

    }
    gettimeofday( &end, NULL );

    elapsed = (( end.tv_sec  - start.tv_sec  ) * 1000 +
               ( end.tv_usec - start.tv_usec ) / 1000 ) / 10;

    if ( elapsed < peak_meter.hold_time )
        peak_meter.hold_count = peak_meter.hold_time / elapsed;
    pthread_create( &threads[0], NULL, update_meter, NULL );

    while ( 1 )
    {
    };

    pthread_mutex_destroy( &displayBusy );
    pthread_exit( NULL );

    return 0;
}
