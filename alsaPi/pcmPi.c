//  ===========================================================================
/*
    pcmPi:

    Library to provide PCM stream info. Intended for use with LMS streams
    via squeezelite to feed small LCD displays.
    References:
        https://github.com/ralph-irving/jivelite.

    Copyright 2016 Darren Faulke <darren@alidaf.co.uk>
    Portions copyright: 2012-2015 Adrian Smith,
                        2015-2016 Ralph Irving.

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

        gcc -c -Wall -fpic pcmPi.c -lm -lpthread -lrt -lasound -lncurses
        gcc -shared -o libpcmPi.so pcmPi.o

    For Raspberry Pi v1 optimisation use the following flags:

        -march=armv6zk -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3

    For Raspberry Pi v2 optimisation use the following flags:

        -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4
        -ffast-math -pipe -O3
*/
//  ===========================================================================
/*
    Authors:        D.Faulke            05/02/2016

    Contributors:
*/
//  ===========================================================================

//  Installed libraries -------------------------------------------------------

#include <stdbool.h>

#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <math.h>

#include <ncurses.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

//  Local libraries -----------------------------------------------------------

#include "pcmPi.h"


//  Functions. ----------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns 1st valid MAC address for shared memory access.
//  ---------------------------------------------------------------------------
/*
    There are many, much simpler examples available but the Jivelite code
    has been largely used as it is based on the Squeezelite code and therefore
    has some synergy!
*/
static char *get_mac_address()
{
    struct  ifconf ifc;
    struct  ifreq  *ifr, *ifend;
    struct  ifreq  ifreq;
    struct  ifreq  ifs[3];
    /*
        Squeezelite only searches first 4 interfaces but the Raspberry Pi
        will probably only ever have 3 interfaces at most:
            1) lo (loopback, not used),
            2) eth0 (ethernet),
            3) wlan0 (wireless adapter).
    */
    uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 };

    // Create a socket for ioctl function.
    int sd = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( sd < 0 ) return mac_address;

    // Get some interface info.
    ifc.ifc_len = sizeof( ifs );
    ifc.ifc_req = ifs;

    // Request hardware address with ioctl.
    if ( ioctl( sd, SIOCGIFCONF, &ifc ) == 0 )
    {
        // Get last interface.
        ifend = ifs + ( ifc.ifc_len / sizeof( struct ifreq ));

        // Loop through interfaces.
        for ( ifr = ifc.ifc_req; ifr < ifend; ifr++ )
        {
            if ( ifr->ifr_addr.sa_family == AF_INET )
            {
                strncpy( ifreq.ifr_name, ifr->ifr_name, sizeof( ifreq.ifr_name ));
                if ( ioctl( sd, SIOCGIFHWADDR, &ifreq ) == 0 )
                {
                    memcpy( mac, ifreq.ifr_hwaddr.sa_data, 6 );
                    // Leave on first valid address.
                    if ( mac[0]+mac[1]+mac[2] != 0 ) ifr = ifend;
                }
            }
        }
    }

    // Close socket.
    close( sd );

    char *macaddr = malloc( 18 );

    // Construct MAC address string.
    sprintf( macaddr, "%02x:%02x:%02x:%02x:%02x:%02x",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

	return macaddr;
}

//  ---------------------------------------------------------------------------
//  Reopens squeezelite if already running and maps to different memory block.
//  ---------------------------------------------------------------------------
static void _reopen( void )
/*
    Jivelite code.
*/
{
	char shm_path[40];

    // Unmap memory if it is already mapped.
	if ( vis_mmap )
	{
		munmap( vis_mmap, sizeof( struct vis_t ));
		vis_mmap = NULL;
	}

    // Close file access if it exists.
	if ( vis_fd != -1 )
	{
		close( vis_fd );
		vis_fd = -1;
	}

    // Get MAC adddress if not already determined.
	if ( !mac_address )
	{
		mac_address = get_mac_address();
	}

    /*
        The shared memory object is defined by Squeezelite and is identified
        by a name made up from the MAC address.
    */
	sprintf( shm_path, "/squeezelite-%s", mac_address ? mac_address : "" );

    // Open shared memory.
	vis_fd = shm_open( shm_path, O_RDWR, 0666 );
	if ( vis_fd > 0 )
	{
        // Map memory.
        vis_mmap = mmap( NULL, sizeof( struct vis_t ),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED, vis_fd, 0 );

        if ( vis_mmap == MAP_FAILED )
        {
            close( vis_fd );
            vis_fd = -1;
            vis_mmap = NULL;
        }
    }
}

//  ---------------------------------------------------------------------------
//  Checks status of mmap and attempt to open/reopen if not updated recently.
//  ---------------------------------------------------------------------------
static void vis_check( void )
/*
    Jivelite code.
    This function maps the shared memory object if it has not already done so
    within the last 5 seconds.
    The buffer can contain 16384 samples, which at the highest sample rate of
    384kHz, is enough for around 42ms. This increases to 371ms for 44.1kHz.
    Shouldn't the shared memory object be mapped more frequently?
*/
{
    static time_t lastopen = 0;
    time_t now = time( NULL );

    if ( !vis_mmap )
    {
        if ( now - lastopen > 5 )
        {
            _reopen();
            lastopen = now;
        }
        if ( !vis_mmap ) return;
    }

    pthread_rwlock_rdlock( &vis_mmap->rwlock );

    running = vis_mmap->running;

    if ( running && now - vis_mmap->updated > 5 )
    {
        pthread_rwlock_unlock(&vis_mmap->rwlock );
        _reopen();
        lastopen = now;
    } else
    {
        pthread_rwlock_unlock( &vis_mmap->rwlock );
    }
}


//  ---------------------------------------------------------------------------
//  Locks the memory mapping thread.
//  ---------------------------------------------------------------------------
static void vis_lock( void )
{
    if ( !vis_mmap ) return;
    pthread_rwlock_rdlock( &vis_mmap->rwlock );
}

//  ---------------------------------------------------------------------------
//  Unlocks the memory mapping thread.
//  ---------------------------------------------------------------------------
static void vis_unlock( void )
{
    if ( !vis_mmap ) return;
    pthread_rwlock_unlock( &vis_mmap->rwlock );
}

//  ---------------------------------------------------------------------------
//  Returns the stream status.
//  ---------------------------------------------------------------------------
static bool vis_get_playing( void )
{
    if ( !vis_mmap ) return false;
    return running;
}

//  ---------------------------------------------------------------------------
//  Returns the stream bit rate.
//  ---------------------------------------------------------------------------
static uint32_t vis_get_rate( void )
{
    if ( !vis_mmap ) return 0;
    return vis_mmap->rate;
}

//  ---------------------------------------------------------------------------
//  Returns the shared audio buffer.
//  ---------------------------------------------------------------------------
static int16_t *vis_get_buffer( void )
{
    if ( !vis_mmap ) return NULL;
    return vis_mmap->buffer;
}

//  ---------------------------------------------------------------------------
//  Returns the length of the shared audio buffer.
//  ---------------------------------------------------------------------------
static uint32_t vis_get_buffer_len( void )
{
    if ( !vis_mmap ) return 0;
    return vis_mmap->buf_size;
}

//  ---------------------------------------------------------------------------
//  Returns the index into the shared audio buffer.
//  ---------------------------------------------------------------------------
static uint32_t vis_get_buffer_idx( void )
{
    if ( !vis_mmap ) return 0;
    return vis_mmap->buf_index;
}

//  ---------------------------------------------------------------------------
//  Calculates peak dBfs values (L & R) of a number of stream samples.
//  ---------------------------------------------------------------------------
/*
    The dBfs values are currently stored in a global struct variable but this
    will probably change.
*/
int get_dBfs( uint16_t num_samples )
{
	int16_t  *ptr;
	int16_t  sample;
	uint64_t sample_squared[2];
	uint16_t sample_rms[2];
	size_t   i, samples_until_wrap;
	int offs;

	vis_check();

    sample_squared[0] = 0;
    sample_squared[1] = 0;

	if ( vis_get_playing() )
	{
		vis_lock();

		offs = vis_get_buffer_idx() - ( num_samples * 2 );
		while ( offs < 0 ) offs += vis_get_buffer_len();

		ptr = vis_get_buffer() + offs;
		samples_until_wrap = vis_get_buffer_len() - offs;

		for ( i = 0; i < num_samples; i++ )
		{
            // Channel 0 (L).
			sample = *ptr++;
            sample_squared[0] += sample * sample;

            // Channel 1 (R).
			sample = *ptr++;
            sample_squared[1] += sample * sample;

			samples_until_wrap -= 2;
			if ( samples_until_wrap <= 0 )
			{
				ptr = vis_get_buffer();
				samples_until_wrap = vis_get_buffer_len();
			}
		}
		vis_unlock();
	}

    sample_rms[0] = round( sqrt( sample_squared[0] ));
    sample_rms[1] = round( sqrt( sample_squared[1] ));

    peak_meter.dBfs[0] = 20 * log10( (float) sample_rms[0] /
                                     (float) peak_meter.reference );
    if ( peak_meter.dBfs[0] < peak_meter.floor )
         peak_meter.dBfs[0] = peak_meter.floor;

    peak_meter.dBfs[1] = 20 * log10( (float) sample_rms[1] /
                                     (float) peak_meter.reference );
    if ( peak_meter.dBfs[1] < peak_meter.floor )
         peak_meter.dBfs[1] = peak_meter.floor;

	return 1;
}


//  ---------------------------------------------------------------------------
//  Calculates the indices for string representations of the peak levels.
//  ---------------------------------------------------------------------------
/*
    The indices are currently stored in a global struct variable but this will
    probably change.
*/
void get_dB_indices( void )
{
    uint8_t channel;
    uint8_t i;
    static  uint8_t count[METER_CHANNELS_MAX] = { 0, 0 };

    for ( channel = 0; channel < METER_CHANNELS_MAX; channel++ )
    {
        for ( i = 0; i < peak_meter.num_levels; i++ )
        {
            if ( peak_meter.dBfs[channel] <= peak_meter.intervals[i] )
            {
                peak_meter.bar_index[channel] = i;
                if ( i > peak_meter.dot_index[channel] )
                {
                    peak_meter.dot_index[channel] = i;
                    count[channel] = 0;
                }
                break;
            }
        }
        // Rudimentary peak hold routine until proper timing is introduced.
        count[channel]++;
        if ( count[channel] >= HOLD_DELAY )
        {
            count[channel] = 0;
            if ( peak_meter.dot_index[channel] > 0 )
                 peak_meter.dot_index[channel]--;
        }
    }
}


//  ---------------------------------------------------------------------------
//  Produces string representations of the peak meters.
//  ---------------------------------------------------------------------------
/*
    This is intended for a small LCD (16x2 or similar) or terminal output.
*/
void get_peak_strings( char dB_string[METER_CHANNELS_MAX][PEAK_METER_MAX_LEVELS + 1] )
{
    uint8_t channel;
    uint8_t i;

    for ( channel = 0; channel < METER_CHANNELS_MAX; channel++ )
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
    uint8_t  i;
    uint16_t samples;
//    char *string_L, *string_R;

    // ncurses stuff.
    WINDOW *meter_win;
    initscr();      // Init ncurses.
    cbreak();       // Disable line buffering.
    noecho();       // No screen echo of key presses.
    meter_win = newwin( 7, 30, 10, 30 );
    box( meter_win, 0, 0 );
    wrefresh( meter_win );
    curs_set( 0 );  // Turn cursor off.

    vis_check();

//	samples = vis_get_rate() * peak_meter.int_time / 1000;
    samples = 2; // Minimum samples for fastest response but may miss peaks.

//    printf( "Samples for %dms = %d\n", peak_meter.int_time, samples );

    mvwprintw( meter_win, 2, 2, " |....|....|....|" );
    mvwprintw( meter_win, 3, 2, "-48  -20  -10   0 dBFS" );
    mvwprintw( meter_win, 4, 2, " |''''|''''|''''|" );

    // Create foreground/background colour pairs for meters.
    start_color();
    init_pair( 1, COLOR_GREEN, COLOR_BLACK );
    init_pair( 2, COLOR_YELLOW, COLOR_BLACK );
    init_pair( 3, COLOR_RED, COLOR_BLACK );

    while ( 1 )
    {
        get_dBfs( samples );
        get_dB_indices();
        get_peak_strings( lcd16x2_peak_meter );
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
