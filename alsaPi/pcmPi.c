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

        gcc -c -Wall -fpic pcmPi.c -lm -lpthread -lrt -lasound
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
    Authors:        D.Faulke            04/02/2016

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
    Squeezelite only searches first 4 interfaces but Pi will probably only ever
    have 3 interfaces at most:
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
        by a name made from the MAC address.
        see output_vis.c of Squeezelite source code.
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
//  Returns peak dBfs values (L & R) of a number of stream samples.
//  ---------------------------------------------------------------------------
/*
    According to IEC 60268-18 (1995), peak level meters should have the
    following characteristics:

        Delay time < 150ms.
        Integration time < 5ms.
        Return time < 1.7s +/- 0.3s.
        Hold time = 1.0s +/- 0.5s.
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
//  Returns a binary string representations of the peak levels.
//  ---------------------------------------------------------------------------
void get_dB_indices( void )
{
    uint8_t i;
    static  uint8_t count[2] = { 0, 0 };

    // Channel 0.
    for ( i = 0; i < PEAK_METER_INTERVALS; i++ )
    {
        if ( peak_meter.dBfs[0] <= peak_meter.intervals[i] )
        {
            peak_meter.dBfs_index[0] = i;
            if ( i > peak_meter.dBfs_index[2] )
            {
                peak_meter.dBfs_index[2] = i;
                count[0] = 0;
            }
            break;
        }
    }
    // Channel 1.
    for ( i = 0; i < PEAK_METER_INTERVALS; i++ )
    {
        if ( peak_meter.dBfs[1] <= peak_meter.intervals[i] )
        {
            peak_meter.dBfs_index[1] = i;
            if ( i > peak_meter.dBfs_index[3] )
            {
                peak_meter.dBfs_index[3] = i;
                count[1] = 0;
            }
            break;
        }
    }

    count[0]++;
    count[1]++;

    if ( count[0] >= 5 )
    {
        count[0] = 0;
        if ( peak_meter.dBfs_index[2] > 0 ) peak_meter.dBfs_index[2]--;
    }

    if ( count[1] >= 5 )
    {
        count[1] = 0;
        if ( peak_meter.dBfs_index[3] > 0 ) peak_meter.dBfs_index[3]--;
    }

}


//  ---------------------------------------------------------------------------
//  Returns a string representation of the peak meter for a single channel.
//  ---------------------------------------------------------------------------
/*
    This is intended for a small LCD (16x2 or similar) or terminal output.
*/
void get_peak_strings( uint8_t index[4],
                       char    dB_string[2][PEAK_METER_INTERVALS + 1] )
{
    uint8_t i;
    uint16_t muxed[2];

    // Combine bar and dot codes.
    muxed[0] = ( peak_meter.leds_bar[index[0]] |
                 peak_meter.leds_dot[index[2]] );
    muxed[1] = ( peak_meter.leds_bar[index[1]] |
                 peak_meter.leds_dot[index[3]] );
//    printf( "Bar = 0x%04x, 0x%04x\n", peak_meter.leds_bar[index[0]],
//                                      peak_meter.leds_bar[index[1]] );
//    printf( "Dot = 0x%04x, 0x%04x\n", peak_meter.leds_dot[index[2]],
//                                      peak_meter.leds_dot[index[3]] );
//    printf( "Mux = 0x%04x, 0x%04x\n", muxed[0], muxed[1] );
    /*
        Ignore first bit since that slot will have channel id ( L or R ).
        Need to work backwards and shift right otherwise size of var increases.
    */
    for ( i = 1; i < PEAK_METER_INTERVALS; i++ )
    {
        if (( muxed[0] & 0x1 ) == 0x1 )
            dB_string[0][PEAK_METER_INTERVALS - i] = '#';
        else
            dB_string[0][PEAK_METER_INTERVALS - i] = ' ';

        if (( muxed[1] & 0x1 ) == 0x1 )
            dB_string[1][PEAK_METER_INTERVALS - i] = '#';
        else
            dB_string[1][PEAK_METER_INTERVALS - i] = ' ';

        muxed[0] >>= 1;
        muxed[1] >>= 1;
    }

    dB_string[0][i] = '\0';
    dB_string[1][i] = '\0';
}

//  ---------------------------------------------------------------------------
//  Main (functional test).
//  ---------------------------------------------------------------------------
int main( void )
{
    uint8_t  i;
    uint16_t samples;
//    char *string_L, *string_R;

    vis_check();

	samples = vis_get_rate() * peak_meter.int_time / 1000;
    samples = 2;

    printf( "Samples for %dms = %d\n", peak_meter.int_time, samples );

    while ( 1 )
    {
        get_dBfs( samples );
        get_dB_indices();
        get_peak_strings( peak_meter.dBfs_index, lcd16x2_peak_meter );

        // Test string function.
//        get_dB_string( peak_meter.leds_bar[0], lcd16x2_peak_meter[0] );
//        get_dB_string( peak_meter.leds_bar[1], lcd16x2_peak_meter[1] );

        printf( "\r%04d (0x%05x,0x%05x) %04d (0x%05x,0x%05x) %s  %s",
            peak_meter.dBfs[0],
            peak_meter.leds_bar[peak_meter.dBfs_index[0]],
            peak_meter.leds_dot[peak_meter.dBfs_index[1]],
            peak_meter.dBfs[1],
            peak_meter.leds_bar[peak_meter.dBfs_index[2]],
            peak_meter.leds_dot[peak_meter.dBfs_index[3]],
            lcd16x2_peak_meter[0],
            lcd16x2_peak_meter[1] );

        fflush( stdout );
        usleep( 50000 );
    }

    return 0;
}
