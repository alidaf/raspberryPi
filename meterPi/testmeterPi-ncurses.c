//  ===========================================================================
/*
    testmeterPi-ncurses:

    Tests meterPi using ncurses output on terminal.

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

        gcc -c -Wall meterPi.c testmeterPi-ncurses.c -o testmeterPi-ncurses
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
    Authors:        D.Faulke            13/02/2016

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

#include <ncurses.h>

//  Local libraries -------------------------------------------------------

#include "meterPi.h"


//  Functions. ----------------------------------------------------------------

#define METER_LEVELS 41
#define METER_DELAY  2270

//  ---------------------------------------------------------------------------
//  Produces string representations of the peak meters.
//  ---------------------------------------------------------------------------
/*
    This is intended for a small LCD (16x2 or similar) or terminal output.
*/
void get_peak_strings( struct peak_meter_t peak_meter,
                       char dB_string[METER_CHANNELS][METER_LEVELS + 1] )
{
    uint8_t channel;
    uint8_t i;

    for ( channel = 0; channel < METER_CHANNELS; channel++ )
    {
        for ( i = 0; i < peak_meter.num_levels; i++ )
        {
            if (( i <= peak_meter.bar_index[channel] ) ||
                ( i == peak_meter.dot_index[channel] ))
                dB_string[channel][i] = '#';
            else
                dB_string[channel][i] = ' ';
        }
        dB_string[channel][i] = '\0';
    }
}


//  ---------------------------------------------------------------------------
//  Reverses a string passed as *buffer between start and end.
//  ---------------------------------------------------------------------------
static void reverse_string( char *buffer, size_t start, size_t end )
{
    char temp;
    while ( start < end )
    {
        end--;
        temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
    }
    return;
};


//  ---------------------------------------------------------------------------
//  Main (functional test).
//  ---------------------------------------------------------------------------
int main( void )
{
    struct timeval start, end;
    uint32_t elapsed; // Elapsed time in milliseconds.
    uint8_t i;

    struct peak_meter_t peak_meter =
    {
        .int_time       = 5,
        .samples        = 2,
        .hold_time      = 500,
        .hold_count     = 3,
        .num_levels     = 41,
        .floor          = -96,
        .reference      = 32768,
        .dBfs           = { 0, 0 },
        .bar_index      = { 0, 0 },
        .dot_index      = { 0, 0 },
        .elapsed        = { 0, 0 },
        .scale          =
            { -40, -39, -38, -37, -36, -35, -34, -33, -32, -31
              -30, -29, -28, -27, -26, -25, -24, -23, -22, -21,
              -20, -19, -18, -17, -16, -15, -14, -13, -12, -11,
              -10,  -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,   0  }
    };

    // String representations for LCD display.
    char window_peak_meter[METER_CHANNELS][METER_LEVELS + 1];

    vis_check();

    // Calculate number of samples for integration time.
	peak_meter.samples = vis_get_rate() * peak_meter.int_time / 1000;
    if ( peak_meter.samples < 1 ) peak_meter.samples = 1;
    if ( peak_meter.samples > VIS_BUF_SIZE / METER_CHANNELS )
         peak_meter.samples = VIS_BUF_SIZE / METER_CHANNELS;
//    peak_meter.samples = 2; // Minimum samples for fastest response but may miss peaks.
    printf( "Samples for %dms = %d.\n", peak_meter.int_time, peak_meter.samples );

    // ncurses stuff.
    WINDOW *meter_win;
    initscr();      // Init ncurses.
    cbreak();       // Disable line buffering.
    noecho();       // No screen echo of key presses.
    meter_win = newwin( 7, 52, 10, 10 );
    box( meter_win, 0, 0 );
    wrefresh( meter_win );
    nodelay( meter_win, TRUE );
    scrollok( meter_win, TRUE );
    curs_set( 0 );  // Turn cursor off.

    mvwprintw( meter_win, 1, 2, "L" );
    mvwprintw( meter_win, 2, 2, " |....|....|....|....|....|....|....|....|" );
    mvwprintw( meter_win, 3, 2, "-40  -35  -30  -25  -20  -15  -10  -5    0 dBFS" );
    mvwprintw( meter_win, 4, 2, " |''''|''''|''''|''''|''''|''''|''''|''''|" );
    mvwprintw( meter_win, 5, 2, "R" );

    // Create foreground/background colour pairs for meters.
    start_color();
    init_pair( 1, COLOR_GREEN, COLOR_BLACK );
    init_pair( 2, COLOR_YELLOW, COLOR_BLACK );
    init_pair( 3, COLOR_RED, COLOR_BLACK );

    // Do some loops to test response time.
    gettimeofday( &start, NULL );
    for ( i = 0; i < TEST_LOOPS; i++ )
    {
        get_dBfs( &peak_meter );
        get_dB_indices( &peak_meter );
        get_peak_strings( peak_meter, window_peak_meter );

        mvwprintw( meter_win, 1, 3, "%s", window_peak_meter[0] );
        mvwprintw( meter_win, 5, 3, "%s", window_peak_meter[1] );

        mvwchgat( meter_win, 1,  3, 30, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 1, 33,  5, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 1, 38,  5, A_NORMAL, 3, NULL );
        mvwchgat( meter_win, 5,  3, 30, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 5, 33,  5, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 5, 38,  5, A_NORMAL, 3, NULL );

        // Refresh ncurses window to display.
        wrefresh( meter_win );
        usleep( METER_DELAY );

    }
    gettimeofday( &end, NULL );

    elapsed = (( end.tv_sec  - start.tv_sec  ) * 1000 +
               ( end.tv_usec - start.tv_usec ) / 1000 ) / 10;

    if ( elapsed < peak_meter.hold_time )
        peak_meter.hold_count = peak_meter.hold_time / elapsed;

//    printf( "Hold count = %d.\n", peak_meter.hold_count );

    int ch = ERR;
    while ( ch == ERR )
    {

        get_dBfs( &peak_meter );
        get_dB_indices( &peak_meter );
        get_peak_strings( peak_meter, window_peak_meter );

        // Print ncurses data.
        mvwprintw( meter_win, 1, 3, "%s", window_peak_meter[0] );
        mvwprintw( meter_win, 5, 3, "%s", window_peak_meter[1] );

        mvwchgat( meter_win, 1,  3, 30, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 1, 33,  5, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 1, 38,  5, A_NORMAL, 3, NULL );
        mvwchgat( meter_win, 5,  3, 30, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 5, 33,  5, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 5, 38,  5, A_NORMAL, 3, NULL );

        // Refresh ncurses window to display.
        wrefresh( meter_win );
        ch = wgetch( meter_win );
        usleep( METER_DELAY );
    }

    // Close ncurses.
    delwin( meter_win );
    endwin();

    return 0;
}
