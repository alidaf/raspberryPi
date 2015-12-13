// ****************************************************************************
/*
    alsaPi:

    ALSA based sound driver for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
        ALSA routines based on amixer, 2000 Jaroslev Kysela.
            - see http://www.alsa-project.org.

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

//  Compilation:
//
//  Compile with gcc -c -fpic alsaPi.c -lasound -lm
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

#define alsaPiVersion "Version 0.1"

//  Authors:        D.Faulke    10/12/2015
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

//  To Do:
//      Add proper muting rather than set volume to 0, and set mute when 0.
//          - look at playback switch mechanism.
//      Add balance control.
//      Add soft limits.
//

//  Installed libraries -------------------------------------------------------

#include <alsa/asoundlib.h>
#include <math.h>
#include <stdbool.h>

//  Local libraries -----------------------------------------------------------

#include "alsaPi.h"


//  Local variables. ----------------------------------------------------------

static bool header = false; // Flag to print header on 1st set volume.


//  Functions. ----------------------------------------------------------------

// ----------------------------------------------------------------------------
//  Initialises hardware and returns info in sound struct.
// ----------------------------------------------------------------------------
int soundOpen( void )
{
    int err;

    //  Set up ALSA mixer.
    err = snd_mixer_open( &mixerHandle, 0 );
    if ( err < 0 )
    {
        printf( "%s.\n", snd_strerror( err ));
        return err;
    }
    err = snd_mixer_attach( mixerHandle, sound.card );
    if ( err < 0 )
    {
        printf( "%s.\n", snd_strerror( err ));
        return err;
    }
    err = snd_mixer_load( mixerHandle );
    if ( err < 0 )
    {
        printf( "%s.\n", snd_strerror( err ));
        return err;
    }
    err = snd_mixer_selem_register( mixerHandle, NULL, NULL );
    if ( err < 0 )
    {
        printf( "%s.\n", snd_strerror( err ));
        return err;
    }

    snd_mixer_selem_id_alloca( &mixerId );
    snd_mixer_selem_id_set_name( mixerId, sound.mixer );

    mixerElem = snd_mixer_find_selem( mixerHandle, mixerId );
    if ( mixerElem == NULL )
    {
        printf( "Couldn't find mixer" );
        return -1;
    }
    snd_mixer_selem_get_id( mixerElem, mixerId );

    // Get hardware volume limits.
    long minHard, maxHard;
    err = snd_mixer_selem_get_playback_volume_range( mixerElem,
                                                     &minHard, &maxHard );
    if ( err < 0 )
    {
        printf( "%s.\n", snd_strerror( err ));
        return err;
    }

    // Calculate soft limits.
    long minSoft, maxSoft;
    minSoft = sound.min / 100 * ( maxHard - minHard ) + minHard;
    maxSoft = sound.max / 100 * ( maxHard - minHard ) + minHard;

    // Set soft limits.
    sound.min = minSoft;
    sound.max = maxSoft;
    sound.range = sound.max - sound.min;

    // Set starting index and volume.
    sound.index = lroundf( (float)sound.volume / 100 * sound.incs );

    return 0;
}

// ----------------------------------------------------------------------------
//  Calculates volume based on index. Returns value in sound struct.
// ----------------------------------------------------------------------------
long calcVol( float index, float incs, float range, float min, float factor )
{
    long volume;

    if ( factor == 1 ) //  Divide by 0 if used in function.
         volume = lroundf(( index / incs * range ) + min );
    else volume = lroundf((( pow( factor, index / incs ) - 1 ) /
                                ( factor - 1 ) * range + min ));
    return volume;
};

// ----------------------------------------------------------------------------
//  Set volume using ALSA mixers.
// ----------------------------------------------------------------------------
int setVol( void )
{
    long linearVol; // Linear volume. Used for debugging.
    int err;

    // Calculate volume value from index.
    sound.volume = calcVol( sound.index, sound.incs, sound.range,
                            sound.min, sound.factor );

    // If control is mono then FRONT_LEFT will set volume.
    err=snd_mixer_selem_set_playback_volume( mixerElem,
            SND_MIXER_SCHN_FRONT_LEFT, sound.volume );
    if ( err < 0 ) return err;
    err=snd_mixer_selem_set_playback_volume( mixerElem,
            SND_MIXER_SCHN_FRONT_RIGHT, sound.volume );
    if ( err < 0 ) return err;

    if ( sound.print ) // Print output if requested. For debugging.
    {
        linearVol = calcVol( sound.index, sound.incs, sound.range,
                             sound.min, 1 );

        if ( header == false ) // Print out header block.
        {
            printf( "\n" );
            printf( "\t+-----------+-----------------+-----------------+\n" );
            printf( "\t| indices   | Linear Volume   | Mapped Volume   |\n" );
            printf( "\t+-----+-----+--------+--------+--------+--------+\n" );
            printf( "\t| L   | R   | L      | R      | L      | R      |\n" );
            printf( "\t+-----+-----+--------+--------+--------+--------+\n" );

            header = true;
        }
        if ( err < 0 )
            printf( "\t| %-45s |\n", snd_strerror( err ));
        else
            printf( "\t| %3i | %3i | %6ld | %6ld | %6d | %6d |\n",
                        sound.index, sound.index,
                        linearVol, linearVol,
                        sound.volume, sound.volume );
    }

    return 0;
};

// ----------------------------------------------------------------------------
//  Increases volume.
// ----------------------------------------------------------------------------
void incVol( void )
{
    // Ensure not at limit.
    if ( sound.index >= sound.incs ) sound.index = sound.incs;
    // Increment index.
    else sound.index++;

    // Set volume.
    setVol();

    return;
};

// ----------------------------------------------------------------------------
//  Increases volume.
// ----------------------------------------------------------------------------
void decVol( void )
{
    // Ensure not at limit.
    if ( sound.index <= 0 ) sound.index = 0;
    // Deccrement index.
    else sound.index--;

    // Set volume.
    setVol();

    return;
};

// ----------------------------------------------------------------------------
//  Detaches and closes ALSA.
// ----------------------------------------------------------------------------
void soundClose( void )
{
    snd_mixer_detach( mixerHandle, sound.card );
    snd_mixer_close( mixerHandle );

    return;
};

