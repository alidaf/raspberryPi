/****************************************************************************/
/*                                                                          */
/*  ALSA experiments to test feasibility of creating a spectrum analyser     */
/*  and digital VU meters.                                                   */
/*                                                                           */
/*  Copyright  2015 by Darren Faulke <darren@alidaf.co.uk>                   */
/*                                                                           */
/*  This program is free software; you can redistribute it and/or modify     */
/*  it under the terms of the GNU General Public License as published by     */
/*  the Free Software Foundation, either version 2 of the License, or        */
/*  (at your option) any later version.                                      */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU General Public License for more details.                             */
/*                                                                           */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.     */
/*                                                                           */
/*****************************************************************************/

#define Version "Version 0.1"

/****************************************************************************/
/*                                                                          */
/*  Authors:            D.Faulke                    19/10/15                */
/*  Contributors:                                                           */
/*                                                                          */
/*  Changelog:                                                              */
/*                                                                          */
/*  v0.1 Initial version.                                                   */
/*                                                                          */
/****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Compilation:                                                             */
/*                                                                           */
/*  Uses alsa libraries.                                                     */
/*  Compile with gcc rotencvol.c -o rotencvol -lasound -lm                   */
/*  Also use the following flags for Raspberry Pi:                           */
/*      -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp          */
/*      -ffast-math -pipe -O3                                                */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

// Data structure for PCM stream.
struct pcmStruct
{
    int channels;
    float sampleFreq;
    int sampleRate;
};

/*****************************************************************************/
/*                                                                           */
/*  Main program.                                                            */
/*                                                                           */
/*****************************************************************************/

int main()
{

    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;

    unsigned int val, val2;
    int dir;

    /* Open PCM device for playback. */
    rc = snd_pcm_open( &handle, "default", SND_PCM_STREAM_PLAYBACK, 0 );
    if ( rc < 0 )
    {
        fprintf( stderr, "unable to open pcm device: %s\n",
            snd_strerror( rc ));
        exit( 1 );
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca( &params );

    /* Fill it in with default values. */
    snd_pcm_hw_params_any( handle, params );

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access( handle, params,
        SND_PCM_ACCESS_RW_INTERLEAVED );

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format( handle, params,
        SND_PCM_FORMAT_S16_LE );

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels( handle, params, 2 );

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near( handle, params, &val, &dir );

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params( handle, params );
    if ( rc < 0 )
    {
        fprintf(stderr, "unable to set hw parameters: %s\n",
            snd_strerror( rc ));
        exit( 1 );
    }

    /* Display information about the PCM interface */

    printf( "PCM handle name = '%s'\n", snd_pcm_name( handle ));

    printf("PCM state = %s\n", snd_pcm_state_name( snd_pcm_state( handle )));

    snd_pcm_hw_params_get_access( params, ( snd_pcm_access_t * ) &val );
    printf( "access type = %s\n",
        snd_pcm_access_name(( snd_pcm_access_t ) val ));

    snd_pcm_hw_params_get_format( params, ( snd_pcm_format_t * ) &val );
    printf( "format = '%s' (%s)\n",
        snd_pcm_format_name(( snd_pcm_format_t ) val ),
        snd_pcm_format_description(( snd_pcm_format_t ) val ));

    snd_pcm_hw_params_get_subformat( params,
        ( snd_pcm_subformat_t * ) &val );
    printf( "subformat = '%s' (%s)\n",
        snd_pcm_subformat_name(( snd_pcm_subformat_t ) val ),
        snd_pcm_subformat_description((
        snd_pcm_subformat_t ) val ));

    snd_pcm_hw_params_get_channels( params, &val );
    printf( "channels = %d\n", val );

    snd_pcm_hw_params_get_rate( params, &val, &dir );
    printf( "rate = %d bps\n", val );

    snd_pcm_hw_params_get_period_time( params, &val, &dir );
    printf( "period time = %d us\n", val );

    snd_pcm_hw_params_get_period_size( params, &frames, &dir );
    printf( "period size = %d frames\n", ( int ) frames );

    snd_pcm_hw_params_get_buffer_time( params, &val, &dir );
    printf( "buffer time = %d us\n", val );

    snd_pcm_hw_params_get_buffer_size( params,
        ( snd_pcm_uframes_t * ) &val );
    printf( "buffer size = %d frames\n", val );

    snd_pcm_hw_params_get_periods( params, &val, &dir );
    printf( "periods per buffer = %d frames\n", val );

    snd_pcm_hw_params_get_rate_numden( params, &val, &val2 );
    printf( "exact rate = %d/%d bps\n", val, val2 );

    val = snd_pcm_hw_params_get_sbits( params );
    printf( "significant bits = %d\n", val );

//    snd_pcm_hw_params_get_tick_time( params, &val, &dir );
//    printf( "tick time = %d us\n", val );

    val = snd_pcm_hw_params_is_batch( params );
    printf( "is batch = %d\n", val );

    val = snd_pcm_hw_params_is_block_transfer( params );
    printf( "is block transfer = %d\n", val );

    val = snd_pcm_hw_params_is_double( params );
    printf( "is double = %d\n", val );

    val = snd_pcm_hw_params_is_half_duplex( params );
    printf( "is half duplex = %d\n", val );

    val = snd_pcm_hw_params_is_joint_duplex( params );
    printf( "is joint duplex = %d\n", val );

    val = snd_pcm_hw_params_can_overrange( params );
    printf( "can overrange = %d\n", val );

    val = snd_pcm_hw_params_can_mmap_sample_resolution( params );
    printf( "can mmap = %d\n", val );

    val = snd_pcm_hw_params_can_pause( params );
    printf( "can pause = %d\n", val );

    val = snd_pcm_hw_params_can_resume( params );
    printf( "can resume = %d\n", val );

    val = snd_pcm_hw_params_can_sync_start( params );
    printf( "can sync start = %d\n", val );
    snd_pcm_close( handle );

    return 0;
}
