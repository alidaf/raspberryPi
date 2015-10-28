// ****************************************************************************
// ****************************************************************************
/*
    testdB:

    Program to test the dB mapping of ALSA.

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
//  Compile with gcc -c testDB.c -lasound -lm
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	25/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.
//

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <math.h>

// ************************************************************************
//  ALSA control elements.
// ************************************************************************
snd_ctl_t *ctlHandle;               // Simple control handle.
snd_ctl_elem_id_t *ctlId;           // Simple control element id.
snd_ctl_elem_value_t *ctlControl;   // Simple control element value.
snd_ctl_elem_type_t ctlType;        // Simple control element type.
snd_ctl_elem_info_t *ctlInfo;       // Simple control element info container.
snd_ctl_card_info_t *ctlCard;       // Simple control card info container.

snd_hctl_t *hctlHandle;             // High level control handle;
snd_hctl_elem_t *hctlElem;          // High level control element handle.

snd_mixer_t *mixerHandle;           // Mixer handle.
snd_mixer_selem_id_t *mixerId;      // Mixer simple element identifier.
snd_mixer_elem_t *mixerElem;        // Mixer element handle.

// ****************************************************************************
//  Return mapped volume.
// ****************************************************************************
static long mappedVolume( long volume, long range, long min, float factor )
{
    // mappedVolume = ( base^fraction - 1 ) / ( base - 1 )
    // fraction = increment / total increments.

    float base;
    long mappedVol;

    //  As factor -> 0, volume input is logarithmic.
    //  As factor -> 1, volume input is more linear.
    //  As factor >> 1, volume input is more exponential.
    mappedVol = lroundf( (( pow( factor, (float) volume / range ) - 1 ) /
                         ( factor - 1 ) *
                           range + min ));
    return mappedVol;
};

// ****************************************************************************
//  Main.
// ****************************************************************************
int main()
{

    char *card = "hw:1";
    char *mixer = "Digital";
    unsigned int volume;

    long volMin, volMax;
    long dBMin, dBMax;
    long volLin;
    long voldB;
    long volMap;
    long dBMap;

    // ************************************************************************
    //  Set up ALSA mixer.
    // ************************************************************************
    snd_mixer_open( &mixerHandle, 0 );
    snd_mixer_attach( mixerHandle, card );
    snd_mixer_load( mixerHandle );
    snd_mixer_selem_register( mixerHandle, NULL, NULL );

    snd_mixer_selem_id_alloca( &mixerId );
    snd_mixer_selem_id_set_name( mixerId, mixer );
    mixerElem = snd_mixer_find_selem( mixerHandle, mixerId );
    snd_mixer_selem_get_id( mixerElem, mixerId );


    // ************************************************************************
    //  Get some information for selected card and mixer.
    // ************************************************************************
    snd_mixer_selem_get_playback_volume_range( mixerElem, &volMin, &volMax );
    snd_mixer_selem_get_playback_dB_range( mixerElem, &dBMin, &dBMax );

    printf( "Max = %d (%ddB), Min = %d (%ddB).\n",
            volMax, dBMax / 100, volMin, dBMin / 100 );
    printf( "Range = %d.\n", volMax - volMin );

    for ( volume = 0; volume <= 10; volume++ )
    {
        volLin = ((( float ) volume / 10 ) * ( volMax - volMin ) + volMin );
        snd_mixer_selem_ask_playback_vol_dB( mixerElem, volLin, &voldB );
        volMap = mappedVolume( volLin, volMax - volMin, volMin, 0.05 );
        snd_mixer_selem_ask_playback_vol_dB( mixerElem, volMap, &dBMap );

        printf( "Linear volume = %d (%ddB), Shaped volume = %d (%ddB).\n",
            volLin, voldB / 100, volMap, dBMap / 100 );
    }

}
