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

#define Version "Version 0.2"

//  Compilation:
//
//  Compile with gcc piSetVolMixer.c -o piSetVolMixer -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	19/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.
//    v0.2 Added command line parameters.
//

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <argp.h>

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
struct structArgs
{
    char *card;
    char *mixer;
    int value1;
    int value2;
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
            cmdArgs->card = arg;
            break;
        case 'm' :
            cmdArgs->mixer = arg;
            break;
        case 'v' :
            str = arg;
            token = strtok( str, delimiter );
            cmdArgs->value1 = atoi( token );
            token = strtok( NULL, delimiter );
            if ( token == NULL )
                cmdArgs->value2 = cmdArgs->value1;
            else
                cmdArgs->value2 = atoi( token );
            break;
    }
    return 0;
};

// ****************************************************************************
//  argp parser parameter structure.
// ****************************************************************************

static struct argp argp = { options, parse_opt, args_doc, doc };

// ****************************************************************************
//  Main routine.
// ****************************************************************************

void main( int argc, char *argv[] )
{

    struct structArgs cmdArgs =
    {
        .card = "",
        .mixer = "",
        .value1 = 0,
        .value2 = 0
    };

    int errNum;
    long minVal, maxVal;
    unsigned int channels = 0;


    // ************************************************************************
    //  ALSA mixer elements.
    // ************************************************************************
    snd_mixer_t *handle;        // Mixer handle.
    snd_mixer_selem_id_t *id;   // Mixer simple element identifier.
    snd_mixer_elem_t *elem;     // Mixer element handle.


    // ************************************************************************
    //  Get command line parameters.
    // ************************************************************************
    argp_parse( &argp, argc, argv, 0, 0, &cmdArgs );

    printf( "Card = %s\n", cmdArgs.card );
    printf( "mixer = %s\n", cmdArgs.mixer );
    printf( "Value 1 = %i\n", cmdArgs.value1 );
    printf( "Value 2 = %i\n", cmdArgs.value2 );


    // ************************************************************************
    //  Set up ALSA mixer.
    // ************************************************************************
    if ( snd_mixer_open( &handle, 0 ) < 0 )
    {
        printf( "Error opening mixer.\n" );
        return;
    }
    if ( snd_mixer_attach( handle, cmdArgs.card ) < 0 )
    {
        printf( "Error attaching mixer.\n" );
        return;
    }
    if ( snd_mixer_load( handle ) < 0 )
    {
        printf( "Error loading mixer elements.\n" );
        return;
    }


    // ************************************************************************
    //  Set up mixer simple element register.
    // ************************************************************************
    snd_mixer_selem_id_alloca( &id );
    snd_mixer_selem_id_set_name( id, cmdArgs.mixer );
    if ( snd_mixer_selem_register( handle, NULL, NULL ) < 0 )
    {
        printf( "Error registering element.\n" );
        return;
    }

    elem = snd_mixer_find_selem( handle, id );
    if ( elem == NULL )
    {
        printf( "Couldn't find mixer control with that name.\n" );
        return;
    }


    // ************************************************************************
    //  Get some information for selected card and mixer.
    // ************************************************************************
    snd_mixer_selem_get_playback_volume_range(
                    elem, &minVal, &maxVal );

    printf( "Mixer information:\n" );

    printf( "Min = %d, Max = %d\n", minVal, maxVal );

    if ( snd_mixer_selem_has_playback_channel( elem,
                    SND_MIXER_SCHN_FRONT_LEFT ))
        channels++;
    if ( snd_mixer_selem_has_playback_channel( elem,
                    SND_MIXER_SCHN_FRONT_RIGHT ))
        channels++;



    // ************************************************************************
    //  Set values for selected card and mixer.
    // ************************************************************************
    if ( channels == 1 )
    {
        printf( "Mixer control is mono.\n" );
        if ( snd_mixer_selem_set_playback_volume_all( elem,
                cmdArgs.value1 ) < 0)
            printf( "Unable to set volume.\n" );
        else
            printf( "Set volume to %d.\n", cmdArgs.value1 );
    }
    else
    if ( channels == 2 )
    {
        printf( "Mixer has stereo control.\n" );
        if ( snd_mixer_selem_set_playback_volume( elem,
                SND_MIXER_SCHN_FRONT_LEFT, cmdArgs.value1 ) < 0 )
            printf( "Unable to set L volume.\n" );
        else
            printf( "Set L volume to %d.\n", cmdArgs.value1 );
        if ( snd_mixer_selem_set_playback_volume( elem,
                SND_MIXER_SCHN_FRONT_RIGHT, cmdArgs.value2 ) < 0 )
            printf( "Unable to set R volume.\n" );
        else
            printf( "Set R volume to %d.\n", cmdArgs.value2 );
    }
    else
        printf( "No channels to set.\n" );

    // ************************************************************************
    //  Clean up.
    // ************************************************************************
    snd_mixer_detach( handle, cmdArgs.card );
    snd_mixer_close( handle );

    return;
}
