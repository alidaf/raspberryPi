//  ===========================================================================
/*
    pcmPi:

    Library to provide PCM stream info. Intended for use with LMS streams
    via squeezelite to feed small LCD displays.
    References:
        https://github.com/ralph-irving/jivelite.
        Code copyrighted Logitech.

    Copyright 2016 Darren Faulke <darren@alidaf.co.uk>
    Portions copyright 2010 Logitech.

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

        gcc -c -Wall -fpic pcmPi.c -lasound -lm -lpthread -lrt
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
    Authors:        D.Faulke            02/02/2016

    Contributors:
*/
//  ===========================================================================

//  Installed libraries -------------------------------------------------------

//#include <alsa/asoundlib.h>
//#include <math.h>
#include <stdbool.h>

#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>   // mmap.
#include <stdint.h>     // Standard integer types.
#include <math.h>

//  For get_mac_address function:
#include <stdlib.h>     // malloc.
#include <fcntl.h>      // open.
#include <string.h>     // string functions.
#include <sys/socket.h> // socket.
#include <sys/ioctl.h>  // ioctl.
#include <net/if.h>     // ifreq.
#include <unistd.h>     // close.

//  Local libraries -----------------------------------------------------------

#include "pcmPi.h"

//  Types. --------------------------------------------------------------------

//  Functions. ----------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Returns 1st valid MAC address for shared memory access.
//  ---------------------------------------------------------------------------
/*
    There are many, much simpler examples available but the Jivelite code
    has been largely used as it isn't clear why Jivelite has a storage array
    for 4 ifreq structs just yet.
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
//        printf( "Num interfaces = %d.\n", ifc.ifc_len / sizeof ( struct ifreq ));

        // Loop through interfaces.
        for ( ifr = ifc.ifc_req; ifr < ifend; ifr++ )
        {
            if ( ifr->ifr_addr.sa_family == AF_INET )
            {
                strncpy( ifreq.ifr_name, ifr->ifr_name, sizeof( ifreq.ifr_name ));
//                printf( "Found interface \"%s\".\n", ifr->ifr_name );
                if ( ioctl( sd, SIOCGIFHWADDR, &ifreq ) == 0 )
                {
                    memcpy( mac, ifreq.ifr_hwaddr.sa_data, 6 );
//                    printf( "MAC address = %02x:%02x:%02x:%02x:%02x:%02x.\n",
//                             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
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
    Jivelite code. Not fully understood yet.
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
        It appears that memory is mapped in a manner similar to a file system
        with a path that is determined by the MAC address of the Squeezelite
        player.
    */
	sprintf( shm_path, "/squeezelite-%s", mac_address ? mac_address : "" );
//    printf( "Memory path = %s.\n", shm_path );

    // Open shared memory.
	vis_fd = shm_open( shm_path, O_RDWR, 0666 );
//    printf( "Open shared memory result = %d.\n", vis_fd );
	if ( vis_fd > 0 )
	{
        // Map memory.
//        printf( "Mapping memory.\n" );
        vis_mmap = mmap( NULL, sizeof( struct vis_t ),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED, vis_fd, 0 );

        if ( vis_mmap == MAP_FAILED )
        {
//            printf( "Failed to map memory.\n" );
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

static void vis_lock( void )
{
    if ( !vis_mmap ) return;
    pthread_rwlock_rdlock( &vis_mmap->rwlock );
}

static void vis_unlock( void )
{
    if ( !vis_mmap ) return;
    pthread_rwlock_unlock( &vis_mmap->rwlock );
}

static bool vis_get_playing( void )
{
    if ( !vis_mmap ) return false;
    return running;
}

static uint32_t vis_get_rate( void )
{
    if ( !vis_mmap ) return 0;
    return vis_mmap->rate;
}

static int16_t *vis_get_buffer( void )
{
    if ( !vis_mmap ) return NULL;
    return vis_mmap->buffer;
}

static uint32_t vis_get_buffer_len( void )
{
    if ( !vis_mmap ) return 0;
    return vis_mmap->buf_size;
}

static uint32_t vis_get_buffer_idx( void )
{
    if ( !vis_mmap ) return 0;
    return vis_mmap->buf_index;
}

//  ---------------------------------------------------------------------------
//  Returns mean square values (L & R) of a number of samples.
//  ---------------------------------------------------------------------------
int get_pcm_data()
{
	long long sample_accumulator[2];
	int16_t *ptr;
	int16_t sample;
	int32_t sample_sq;
	size_t i, num_samples, samples_until_wrap;
	int offs;

    uint16_t maxL, maxR;
    double  dBL, dBR;
    int8_t dBFSL, dBFSR;
    uint16_t fullscale = 32768;

    maxL = maxR = 0;
    dBL = dBR = 0;
    dBFSL = dBFSR = 0;

    /*
        Standard VU meters have a 300ms response time.
        Peak Program Meters have a 5ms integration time.
        Need to check stream info somehow. For now assume stream is
        16-bit @ 44.1kHz, i.e.
        VU samples  = 0.3 x 44100 = 13230.
        PPM samples = 0.005 x 44100 = 221.
    */
	num_samples = 2;

	sample_accumulator[0] = 0;
	sample_accumulator[1] = 0;

	vis_check();

	if ( vis_get_playing() )
	{
		vis_lock();

		offs = vis_get_buffer_idx() - ( num_samples * 2 );
//        printf( "Offset = %d.\n", offs );
		while ( offs < 0 ) offs += vis_get_buffer_len();

		ptr = vis_get_buffer() + offs;
		samples_until_wrap = vis_get_buffer_len() - offs;
//        printf( "Samples until wrap = %d.\n", samples_until_wrap );

        /*
            This approach is a simple averaging of the buffer over time, which
            is essentially integrating the signal to give the signal energy.
        */
		for ( i = 0; i < num_samples; i++ )
		{
//			sample = ( *ptr++ ) >> 8;
			sample = abs( *ptr++ );
//			sample_sq = sample * sample;
//			sample_accumulator[0] += sample_sq;
			sample_accumulator[0] += sample;
//            printf( "L sample: %06d, ", sample );
            if ( sample > maxL ) maxL = sample;
            dBL = 20 * log10( (float) maxL / (float) fullscale );

            if ( dBL < -88 ) dBFSL = -88; else dBFSL = round( dBL );

//			sample = ( *ptr++ ) >> 8;
			sample = abs( *ptr++ );
//			sample_sq = sample * sample;
//			sample_accumulator[1] += sample_sq;
			sample_accumulator[1] += sample;
//            printf( "R sample: %06d\n", sample );
            if ( sample > maxR ) maxR = sample;
            dBR = 20 * log10( (float) maxR / (float) fullscale );

            if ( dBR < -88 ) dBFSR = -88; else dBFSR = round( dBL );

			samples_until_wrap -= 2;
			if ( samples_until_wrap <= 0 )
			{
				ptr = vis_get_buffer();
				samples_until_wrap = vis_get_buffer_len();
			}
		}
		vis_unlock();
	}

    // Calculate the mean of the accumulated square values.
	sample_accumulator[0] /= num_samples;
	sample_accumulator[1] /= num_samples;
    char *string_L, *string_R;

    if ( dBFSL >  -2 ) string_L = "################"; else
    if ( dBFSL >  -4 ) string_L = " ###############"; else
    if ( dBFSL >  -6 ) string_L = "  ##############"; else
    if ( dBFSL >  -8 ) string_L = "   #############"; else
    if ( dBFSL > -10 ) string_L = "    ############"; else
    if ( dBFSL > -12 ) string_L = "     ###########"; else
    if ( dBFSL > -14 ) string_L = "      ##########"; else
    if ( dBFSL > -16 ) string_L = "       #########"; else
    if ( dBFSL > -18 ) string_L = "        ########"; else
    if ( dBFSL > -20 ) string_L = "         #######"; else
    if ( dBFSL > -22 ) string_L = "          ######"; else
    if ( dBFSL > -24 ) string_L = "           #####"; else
    if ( dBFSL > -26 ) string_L = "            ####"; else
    if ( dBFSL > -52 ) string_L = "             ###"; else
    if ( dBFSL > -76 ) string_L = "              ##"; else
                       string_L = "               #";

    if ( dBFSR >  -2 ) string_R = "################"; else
    if ( dBFSR >  -4 ) string_R = "############### "; else
    if ( dBFSR >  -6 ) string_R = "##############  "; else
    if ( dBFSR >  -8 ) string_R = "#############   "; else
    if ( dBFSR > -10 ) string_R = "############    "; else
    if ( dBFSR > -12 ) string_R = "###########     "; else
    if ( dBFSR > -14 ) string_R = "##########      "; else
    if ( dBFSR > -16 ) string_R = "#########       "; else
    if ( dBFSR > -18 ) string_R = "########        "; else
    if ( dBFSR > -20 ) string_R = "#######         "; else
    if ( dBFSR > -22 ) string_R = "######          "; else
    if ( dBFSR > -24 ) string_R = "#####           "; else
    if ( dBFSR > -26 ) string_R = "####            "; else
    if ( dBFSR > -52 ) string_R = "###             "; else
    if ( dBFSR > -76 ) string_R = "##              "; else
                       string_R = "#               ";

    printf( "\r%s :L|R: %s",
                string_L,
                string_R );
    fflush( stdout );

	return 1;
}

int main()
{
    uint8_t i;

    for ( ; ; )
    {
        get_pcm_data();
        usleep( 50000 );
    }

    return 0;

}
