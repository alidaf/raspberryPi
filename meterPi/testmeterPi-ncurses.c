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

#include <ncurses.h>

//  Local libraries -------------------------------------------------------

#include "meterPi.h"


//  Functions. ----------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Produces string representations of the peak meters.
//  ---------------------------------------------------------------------------
/*
    This is intended for a small LCD (16x2 or similar) or terminal output.
*/
void get_peak_strings( struct peak_meter_t peak_meter,
                       char dB_string[METER_CHANNELS]
                                     [PEAK_METER_LEVELS_MAX + 1] )
{
    uint8_t channel;
    uint8_t i;

    for ( channel = 0; channel < METER_CHANNELS; channel++ )
    {
        for ( i = 1; i < peak_meter.num_levels; i++ )
        {
            if (( i <= peak_meter.bar_index[channel] ) ||
                ( i == peak_meter.dot_index[channel] ))
                dB_string[channel][i] = '|';
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
        .num_levels     = 16,
        .floor          = -80,
        .reference      = 32768,
        .dBfs           = { 0, 0 },
        .bar_index      = { 0, 0 },
        .dot_index      = { 0, 0 },
        .elapsed        = { 0, 0 },
        .scale          =
        // dBfs scale intervals - need to make this user controlled.
    //        {    -80,    -60,    -50,    -40,    -30,    -20,    -18,    -16,
    //             -14,    -12,    -10,     -8,     -6,     -4,     -2,      0  },
            {    -48,    -42,    -36,    -30,    -24,    -20,    -18,    -16,
                 -14,    -12,    -10,     -8,     -6,     -4,     -2,      0  }
    };


    // String representations for LCD display.
    char lcd16x2_peak_meter[METER_CHANNELS][PEAK_METER_LEVELS_MAX + 1] = { "L" , "R" };

    vis_check();

    // Calculate number of samples for integration time.
	peak_meter.samples = vis_get_rate() * peak_meter.int_time / 1000;
    if ( peak_meter.samples < 1 ) peak_meter.samples = 1;
//    samples = 2; // Minimum samples for fastest response but may miss peaks.
    printf( "Samples for %dms = %d.\n", peak_meter.int_time, peak_meter.samples );

    // ncurses stuff.
    WINDOW *meter_win;
    initscr();      // Init ncurses.
    cbreak();       // Disable line buffering.
    noecho();       // No screen echo of key presses.
    meter_win = newwin( 7, 30, 10, 30 );
    box( meter_win, 0, 0 );
    wrefresh( meter_win );
    curs_set( 0 );  // Turn cursor off.

    mvwprintw( meter_win, 2, 2, " |....|....|....|" );
    mvwprintw( meter_win, 3, 2, "-48  -20  -10   0 dBFS" );
    mvwprintw( meter_win, 4, 2, " |''''|''''|''''|" );

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
        get_peak_strings( peak_meter, lcd16x2_peak_meter );

        mvwprintw( meter_win, 1, 3, "%s", lcd16x2_peak_meter[0] );
        mvwprintw( meter_win, 5, 3, "%s", lcd16x2_peak_meter[1] );

        mvwchgat( meter_win, 1,  4, 9, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 1, 13, 3, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 1, 16, 3, A_NORMAL, 3, NULL );
        mvwchgat( meter_win, 5,  4, 9, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 5, 13, 3, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 5, 16, 3, A_NORMAL, 3, NULL );

        // Refresh ncurses window to display.
        wrefresh( meter_win );
        usleep( 100000 );

    }
    gettimeofday( &end, NULL );

    elapsed = (( end.tv_sec  - start.tv_sec  ) * 1000 +
               ( end.tv_usec - start.tv_usec ) / 1000 ) / 10;

    if ( elapsed < peak_meter.hold_time )
        peak_meter.hold_count = peak_meter.hold_time / elapsed;

    while ( 1 )
    {
        get_dBfs( &peak_meter );
        get_dB_indices( &peak_meter );
        get_peak_strings( peak_meter, lcd16x2_peak_meter );

//        reverse_string( lcd16x2_peak_meter[0], 0, strlen( lcd16x2_peak_meter[0] ));

//        printf( "\r%04d (0x%05x,0x%05x) %04d (0x%05x,0x%05x) %s:%s",
//            peak_meter.dBfs[0],
//            peak_meter.leds_bar[peak_meter.dBfs_index[0]],
//            peak_meter.leds_dot[peak_meter.dBfs_index[1]],
//            peak_meter.dBfs[1],
//            peak_meter.leds_bar[peak_meter.dBfs_index[2]],
//            peak_meter.leds_dot[peak_meter.dBfs_index[3]],
//            lcd16x2_peak_meter[0],
//            lcd16x2_peak_meter[1] );

//        fflush( stdout ); // Flush buffer for line overwrite.

        // Print ncurses data.
        mvwprintw( meter_win, 1, 3, "%s", lcd16x2_peak_meter[0] );
        mvwprintw( meter_win, 5, 3, "%s", lcd16x2_peak_meter[1] );

        mvwchgat( meter_win, 1,  4, 9, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 1, 13, 3, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 1, 16, 3, A_NORMAL, 3, NULL );
        mvwchgat( meter_win, 5,  4, 9, A_NORMAL, 1, NULL );
        mvwchgat( meter_win, 5, 13, 3, A_NORMAL, 2, NULL );
        mvwchgat( meter_win, 5, 16, 3, A_NORMAL, 3, NULL );

        // Refresh ncurses window to display.
        wrefresh( meter_win );

        // Reversal has moved the L to the wrong end. Put it back!
//        lcd16x2_peak_meter[0][0] = 'L';

        /*
            A delay to adjust the LCD or screen for eye sensitivity.
            This delay should only really be applied to the screen updating
            otherwise dynamics could be missed.
        */
        usleep( 100000 );
    }

    // Close ncurses.
    delwin( meter_win );
    endwin();

    return 0;
}
