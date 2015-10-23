// ****************************************************************************
// ****************************************************************************
/*
    thx1138:

    ALSA experiments to test using PCM streams and the feasibility of creating
    a spectrum analyser and digital VU meters.

    Copyright  2015 by Darren Faulke <darren@alidaf.co.uk>

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
// ****************************************************************************
// ****************************************************************************

#define Version "Version 0.1"

//  Compilation:
//
//  Compile with gcc thx1138.c -o thx1138 -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	19/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <argp.h>

// ****************************************************************************
//  Data definitions.
// ****************************************************************************

// Data structure for PCM stream.
struct pcmStruct
{
    int channels;
    float sampleFreq;
    int sampleRate;
};

// ****************************************************************************
//  argp documentation.
// ****************************************************************************

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static char doc[] = "A short program to test using ALSA PCM streams";
static char args_doc[] = "thx1138 <options>";

// ****************************************************************************
//  Data definitions.
// ****************************************************************************

// Data structure to hold command line arguments.
struct structArgs
{
    int card;
    int control;
    char deviceID[8];
};

// ****************************************************************************
//  Command line argument definitions.
// ****************************************************************************

static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Card information:" },
    { "card", 'c', "<n>", 0, "Card ID number." },
    { "control", 'd', "<n>", 0, "Control ID number." },
    { 0 }
};

// ****************************************************************************
//  Command line argument parser.
// ****************************************************************************

static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str;
    char *token;
    const char delimiter[] = ",";
    struct structArgs *cmdArgs = state->input;

    switch( param )
    {
        case 'c' :
            cmdArgs->card = atoi( arg );
            break;
        case 'd' :
            cmdArgs->control = atoi( arg );
            break;
    }
    return 0;
};

// ****************************************************************************
//  argp parser parameter structure.
// ****************************************************************************

static struct argp argp = { options, parse_opt, args_doc, doc };


// ****************************************************************************
//  Main section.
// ****************************************************************************

int main( int argc, char *argv[] )
{
    struct structArgs cmdArgs;
	unsigned int val, val2;
	int dir;
    int errNum;
	cmdArgs.card = 0;		// Default card.
	cmdArgs.control = 1;	// Default control.


    // ************************************************************************
    //  ALSA control elements.
    // ************************************************************************
    snd_pcm_t *pcmp;
    snd_pcm_hw_params_t *params;
//	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
	snd_pcm_stream_t stream = SND_PCM_STREAM_CAPTURE;
    snd_pcm_uframes_t frames;


    // ************************************************************************
    //  Get command line parameters.
    // ************************************************************************
    argp_parse( &argp, argc, argv, 0, 0, &cmdArgs );

    printf( "Card = %i\n", cmdArgs.card );
    printf( "Control = %i\n", cmdArgs.control );
    sprintf( cmdArgs.deviceID, "hw:%i,%i", cmdArgs.card, cmdArgs.control );
	printf( "Using device %s :", cmdArgs.deviceID );

    /* Allocate a hardware parameters object. */
    if ( snd_pcm_hw_params_alloca( &params ) < 0 )
    {
    	fprintf( stderr, "Unable to allocate.\n" );
    	return -1;
   	}
    /* Open PCM device for playback. */
//    if ( snd_pcm_open( &pcmp, cmdArgs.deviceID, stream, 0 ) < 0 )
//    {
//        fprintf( stderr, "Unable to open pcm device.\n" );
//    	return -1;
//    }
    /* Fill it in with default values. */
//    if ( snd_pcm_hw_params_any( pcmp, params ) < 0
//    {
//    	fprintf( stderr, "Unable to set default values.\n" );
//    	return -1;
//    }
    /* Interleaved mode */
//    snd_pcm_hw_params_set_access( pcmp, params,
//    		SND_PCM_ACCESS_RW_INTERLEAVED );

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format( pcmp, params,
        	SND_PCM_FORMAT_S16_LE );

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels( pcmp, params, 2 );

    /* 44100 bits/second sampling rate (CD quality) */
    val = 44100;
    snd_pcm_hw_params_set_rate_near( pcmp, params, &val, &dir );

    /* Write the parameters to the driver */
    errNum = snd_pcm_hw_params( pcmp, params );
    if ( errNum < 0 )
    {
        fprintf(stderr, "unable to set hw parameters: %s\n",
            snd_strerror( errNum ));
        exit( 1 );
    }

    /* Display information about the PCM interface */

    printf( "PCM handle name = '%s'\n", snd_pcm_name( pcmp ));

    printf("PCM state = %s\n", snd_pcm_state_name( snd_pcm_state( pcmp )));

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
    snd_pcm_close( pcmp );

    return 0;
}
