// ****************************************************************************
// ****************************************************************************
/*
    piSetVolMixer:

    Simple app to set volume using ALSA mixer controls.

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

#define Version "Version 0.3"

//  Compilation:
//
//  Compile with gcc piSetVolMixer.c -o piSetVolMixer -lasound -lm
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:     	D.Faulke	19/10/15
//  Contributors:
//
//  Changelog:
//
//  v0.1 Initial version.
//  v0.2 Added command line parameters.
//  v0.3 Added volume shaping.
//

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <argp.h>
#include <math.h>

// ****************************************************************************
//  argp documentation.
// ****************************************************************************

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static char doc[] = "A short test program to set ALSA mixer values.";
static char args_doc[] = "alsavolmixer <options>";

// ****************************************************************************
//  Data definitions.
// ****************************************************************************

// Data structure to hold command line arguments.
struct volumeStruct
{
    char *card;
    char *mixer;
    float factor;
    unsigned int left;
    unsigned int right;
};

// ****************************************************************************
//  Command line argument definitions.
// ****************************************************************************

static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Card information:" },
    { "card", 'c', "<card>", 0, "Card name or hw:<num>." },
    { "mixer", 'm', "<mixer>", 0, "Mixer name." },
    { 0, 0, 0, 0, "Mixer parameters:" },
    { "val", 'v', "<%>/<%,%>", 0, "Set mixer value(s)." },
    { "fac", 'f', "<int>", 0, "Volume shaping factor" },
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
    struct volumeStruct *volume = state->input;

    switch( param )
    {
        case 'c' :
            volume->card = arg;
            break;
        case 'm' :
            volume->mixer = arg;
            break;
        case 'f' :
            volume->factor = atof( arg );
            break;
        case 'v' :
            str = arg;
            token = strtok( str, delimiter );
            volume->left = atoi( token );
            token = strtok( NULL, delimiter );
            if ( token == NULL )
                volume->right = volume->left;
            else
                volume->right = atoi( token );
            break;
    }
    return 0;
};

// ****************************************************************************
//  argp parser parameter structure.
// ****************************************************************************

static struct argp argp = { options, parse_opt, args_doc, doc };

// ============================================================================
//  Returns mapped volume based on shaping factor.
// ============================================================================
/*
    mappedVolume = ( factor^fraction - 1 ) / ( base - 1 )
    fraction = linear volume / volume range.

    factor << 1, logarithmic.
    factor == 1, linear.
    factor >> 1, exponential.
*/
static long getVolume( float volume, float factor,
                       float minimum, float maximum )
{
    long mappedVolume;
    float range = maximum - minimum;

    if ( factor == 1 )
         mappedVolume = lroundf(( volume / 100 * range ) + minimum );
    else mappedVolume = lroundf((( pow( factor, volume / 100 ) - 1 ) /
                                ( factor - 1 ) * range + minimum ));
    return mappedVolume;
};


// ============================================================================
//  Set volume using ALSA mixers.
// ============================================================================
static void setVolume( struct volumeStruct volume )
{
    snd_ctl_t *ctlHandle;               // Simple control handle.
    snd_ctl_elem_id_t *ctlId;           // Simple control element id.
    snd_ctl_elem_value_t *ctlControl;   // Simple control element value.
    snd_ctl_elem_type_t ctlType;        // Simple control element type.
    snd_ctl_elem_info_t *ctlInfo;       // Simple control element info container.
    snd_ctl_card_info_t *ctlCard;       // Simple control card info container.

    snd_mixer_t *mixerHandle;           // Mixer handle.
    snd_mixer_selem_id_t *mixerId;      // Mixer simple element identifier.
    snd_mixer_elem_t *mixerElem;        // Mixer element handle.


    // ------------------------------------------------------------------------
    //  Set up ALSA mixer.
    // ------------------------------------------------------------------------
    snd_mixer_open( &mixerHandle, 0 );
    snd_mixer_attach( mixerHandle, volume.card );
    snd_mixer_load( mixerHandle );
    snd_mixer_selem_register( mixerHandle, NULL, NULL );

    snd_mixer_selem_id_alloca( &mixerId );
    snd_mixer_selem_id_set_name( mixerId, volume.mixer );
    mixerElem = snd_mixer_find_selem( mixerHandle, mixerId );
    snd_mixer_selem_get_id( mixerElem, mixerId );

    // ------------------------------------------------------------------------
    //  Get some information for selected mixer.
    // ------------------------------------------------------------------------
    // Hardware volume limits.
    long minimum, maximum;
    snd_mixer_selem_get_playback_volume_range( mixerElem, &minimum, &maximum );
    long range = maximum - minimum;


    // ------------------------------------------------------------------------
    //  Calculate volume and check bounds.
    // ------------------------------------------------------------------------
    // Calculate mapped volumes.
    long mappedLeft;
    long mappedRight;
    mappedLeft = getVolume( volume.left, volume.factor, minimum, maximum );
    mappedRight = getVolume( volume.right, volume.factor, minimum, maximum );


    // ------------------------------------------------------------------------
    //  Set volume.
    // ------------------------------------------------------------------------
    // If control is mono then FL will set volume.
    snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, mappedLeft );
    snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_RIGHT, mappedRight );


    // ------------------------------------------------------------------------
    //  Clean up.
    // ------------------------------------------------------------------------
    snd_mixer_detach( mixerHandle, volume.card );
    snd_mixer_close( mixerHandle );

    return;
}


// ****************************************************************************
//  Main routine.
// ****************************************************************************

void main( int argc, char *argv[] )
{

    struct volumeStruct volume =
    {
        .card = "hw:0",
        .mixer = "PCM",
        .factor = 0.01,
        .left = 0,
        .right = 0
    };


    // ************************************************************************
    //  Get command line parameters.
    // ************************************************************************
    argp_parse( &argp, argc, argv, 0, 0, &volume );

    setVolume( volume );

    return;
}
