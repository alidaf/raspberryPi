// ****************************************************************************
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

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include "alsaPi.h"

// ============================================================================
// Data structures and types.
// ============================================================================
//  Set default values.
//struct soundStruct sound =
//{
//    sound.card = "hw:0";
//    sound.mixer = "default";
//    sound.factor = 1.0;
//    sound.index = 0;
//    sound.incs = 20;
//    sound.volume = 0;
//    sound.mute = false;
//    sound.print = false;
//};

bool header = false; // Flag to print header on 1st set volume.

// ============================================================================
//  Functions.
// ============================================================================

// ----------------------------------------------------------------------------
//  Initialises hardware and returns info in soundStruct.
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
    long min, max;
    err = snd_mixer_selem_get_playback_volume_range( mixerElem, &min, &max );
    if ( err < 0 )
    {
        printf( "%s.\n", snd_strerror( err ));
        return err;
    }
    sound.min = min;
    sound.max = max;
    sound.range = max - min;

    return 0;
}

// ----------------------------------------------------------------------------
//  Calculates volume based on index. Returns value in soundStruct.
// ----------------------------------------------------------------------------
/*
    linear volume = ratio * range + min.
    mapped volume = ( factor^ratio - 1 ) / ( base - 1 ) * range + min.
    ratio = index / incs.

    factor < 1, logarithmic.
    factor = 1, linear.
    factor > 1, exponential.
*/
long calcVol( float index, float incs, float range, float min, float factor )
{
    long volume;

    if ( factor == 1 )
         volume = lroundf(( index / incs * range ) + min );
    else volume = lroundf((( pow( factor, index / incs ) - 1 ) /
                                ( factor - 1 ) * range + min ));
//    printf( "Calculated volume for index %d as %ld.\n", lroundf( index ), volume );
    return volume;
};

// ----------------------------------------------------------------------------
//  Sets soundStruct.index for a given volume.
// ----------------------------------------------------------------------------
/*
    To avoid rounding errors, volume is set based on an index value. This
    function sets the closest index value for a given volume based on the
    number of increments over the possible range.
    Should be called after soundOpen if the starting volume is not zero.
*/
void setIndex( float volume, float incs, float min, float max )
//void setIndex( int volume )
    // Get closest volume index for starting volume.
{
//    sound.index = lroundf( sound.incs - ( sound.max - volume ) *
//                    sound.incs / ( sound.max - sound.min ));
    sound.index = lroundf( incs - ( max - volume ) * incs / ( max - min ));

    // Ensure volume represents calculated index.
    sound.volume = calcVol( sound.index, sound.incs, sound.range,
                            sound.min, sound.factor );
    return;
};

// ----------------------------------------------------------------------------
//  Set volume using ALSA mixers.
// ----------------------------------------------------------------------------
int setVol( void )
{
    long linearVol; // Linear volume. Used for debugging.
    int err;

    sound.volume = calcVol( sound.index,
                            sound.incs,
                            sound.range,
                            sound.min,
                            sound.factor );

    // If control is mono then FL will set volume.
    err=snd_mixer_selem_set_playback_volume( mixerElem,
            SND_MIXER_SCHN_FRONT_LEFT, sound.volume );
    err=snd_mixer_selem_set_playback_volume( mixerElem,
            SND_MIXER_SCHN_FRONT_RIGHT, sound.volume );

    if ( sound.print ) // Print output if requested. For debugging.
    {
        linearVol = calcVol( sound.index,
                             sound.incs,
                             sound.range,
                             sound.min,
                             sound.factor );

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

    return err;
};

// ----------------------------------------------------------------------------
//  Increases volume.
// ----------------------------------------------------------------------------
void incVol( void )
{
    if ( sound.index >= sound.incs ) sound.index = sound.incs;
    else sound.index++;
    setVol();
};

// ----------------------------------------------------------------------------
//  Increases volume.
// ----------------------------------------------------------------------------
void decVol( void )
{
    if ( sound.index <= 0 ) sound.index = 0;
    else sound.index--;
    setVol();
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

