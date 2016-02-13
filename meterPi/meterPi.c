//  ===========================================================================
/*
    meterPi:

    Library to provide audio metering. Intended for use with LMS streams
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

        gcc -c -Wall -fpic meterPi.c -lm -lpthread -lrt -lasound -lncurses
        gcc -shared -o libmeterPi.so meterPi.o

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

#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

//  Local libraries -----------------------------------------------------------

#include "meterPi.h"

//  Types. --------------------------------------------------------------------

/*
    This is the structure of the shared memory object defined in Squeezelite.
    See https://github.com/ralph-irving/squeezelite/blob/master/output_vis.c.
    It looks like the samples in this buffer are fixed to 16-bit.
    Audio samples > 16-bit are right shifted so that only the higher 16-bits
    are used.
*/
static struct vis_t
{
    pthread_rwlock_t rwlock;
    uint32_t buf_size;
    uint32_t buf_index;
    bool     running;
    uint32_t rate;
    time_t   updated;
    int16_t  buffer[VIS_BUF_SIZE];
}  *vis_mmap = NULL;


static bool running = false;
static int  vis_fd = -1;
static char *mac_address = NULL;

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
//  Reopens squeezelite shared memory and maps memory block.
//  ---------------------------------------------------------------------------
static void reopen( void )
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
void vis_check( void )
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
            reopen();
            lastopen = now;
        }
        if ( !vis_mmap ) return;
    }

    pthread_rwlock_rdlock( &vis_mmap->rwlock );

    running = vis_mmap->running;

    if ( running && now - vis_mmap->updated > 5 )
    {
        pthread_rwlock_unlock(&vis_mmap->rwlock );
        reopen();
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
uint32_t vis_get_rate( void )
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
void get_dBfs( struct peak_meter_t *peak_meter )
{
	int16_t  *ptr;
	int16_t  sample;
	uint64_t sample_squared[METER_CHANNELS];
	uint16_t sample_rms[METER_CHANNELS];
    uint8_t  channel;
	size_t   i, wrap;
	int      offs;

	vis_check();

    for ( channel = 0; channel < METER_CHANNELS; channel++ )
        sample_squared[channel] = 0;

	if ( vis_get_playing() )
	{
		vis_lock();

		offs = vis_get_buffer_idx() - ( peak_meter->samples * METER_CHANNELS );
		while ( offs < 0 ) offs += vis_get_buffer_len();

		ptr = vis_get_buffer() + offs;
		wrap = vis_get_buffer_len() - offs;

		for ( i = 0; i < peak_meter->samples; i++ )
		{
            for ( channel = 0; channel < METER_CHANNELS; channel++ )
            {
			    sample = *ptr++;
                sample_squared[channel] += sample * sample;
            }
            // Check for buffer wrap and refresh if necessary.
			wrap -= 2;
			if ( wrap <= 0 )
			{
				ptr = vis_get_buffer();
				wrap = vis_get_buffer_len();
			}
		}
		vis_unlock();
	}

    for ( channel = 0; channel < METER_CHANNELS; channel++ )
    {
        sample_rms[channel] = round( sqrt( sample_squared[channel] ));
        peak_meter->dBfs[channel] = 20 * log10( (float) sample_rms[channel] /
                                                (float) peak_meter->reference );
        if ( peak_meter->dBfs[channel] < peak_meter->floor )
        {
             peak_meter->dBfs[channel] = peak_meter->floor;
        }
    }
}


//  ---------------------------------------------------------------------------
//  Calculates the indices for string representations of the peak levels.
//  ---------------------------------------------------------------------------
void get_dB_indices( struct peak_meter_t *peak_meter )
{
    uint8_t         channel;
    uint8_t         i;
    static bool     falling = false;
    static uint16_t hold_count[METER_CHANNELS] = { 0, 0 };
    static uint16_t fall_count[METER_CHANNELS] = { 0, 0 };

    for ( channel = 0; channel < METER_CHANNELS; channel++ )
    {
        for ( i = 0; i < peak_meter->num_levels; i++ )
        {
            if ( peak_meter->dBfs[channel] <= peak_meter->scale[i] )
            {
                peak_meter->bar_index[channel] = i;
                if ( i > peak_meter->dot_index[channel] )
                {
                    peak_meter->dot_index[channel] = i;
                    peak_meter->elapsed[channel] = 0;
                    falling = false;
                    hold_count[channel] = 0;
                    fall_count[channel] = 0;
                }
                else
                {
                    i = peak_meter->num_levels;
                }
            }
        }

        // Rudimentary peak hold routine until proper timing is introduced.
        if ( falling )
        {
            fall_count[channel]++;
            if ( fall_count[channel] >= peak_meter->fall_count )
            {
                fall_count[channel] = 0;
                if ( peak_meter->dot_index[channel] > 0 )
                     peak_meter->dot_index[channel]--;
            }
        }
        else
        {
            hold_count[channel]++;
            if ( hold_count[channel] >= peak_meter->hold_count )
            {
                hold_count[channel] = 0;
                falling = true;
                if ( peak_meter->dot_index[channel] > 0 )
                     peak_meter->dot_index[channel]--;
            }
        }
    }
}
