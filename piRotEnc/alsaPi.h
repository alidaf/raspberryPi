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

#ifndef ALSAPI_H
#define ALSAPI_H

//#include <stdio.h>
//#include <string.h>
//#include <alsa/asoundlib.h>
//#include <alsa/mixer.h>
//#include <math.h>
//#include <stdbool.h>
//#include <ctype.h>
//#include <stdlib.h>


//  Data structures. ----------------------------------------------------------

struct soundStruct
{
    char *card;          // ALSA card ID.
    char *mixer;         // ALSA mixer ID.
    float factor;        // Volume mapping factor.
    unsigned char index; // Relative index for volume level.
    unsigned char incs;  // Number of increments over volume range.
    int min;             // Minimum volume (hardware dependent).
    int max;             // Maximum volume (hardware dependent).
    int range;           // Volume range (hardware dependent).
    int volume;          // Volume level.
    char balance;         // Relative balance -100(%) to +100(%).
    bool mute;           // Mute switch.
    bool print;          // Print output switch.
} sound;


//  ALSA control types. -------------------------------------------------------

snd_ctl_t *ctlHandle;             // Simple control handle.
snd_ctl_elem_id_t *ctlId;         // Simple control element id.
snd_ctl_elem_value_t *ctlControl; // Simple control element value.
snd_ctl_elem_type_t ctlType;      // Simple control element type.
snd_ctl_elem_info_t *ctlInfo;     // Simple control element info container.
snd_ctl_card_info_t *ctlCard;     // Simple control card info container.


// ALSA mixer types. ----------------------------------------------------------

snd_mixer_t *mixerHandle;         // Mixer handle.
snd_mixer_selem_id_t *mixerId;    // Mixer simple element identifier.
snd_mixer_elem_t *mixerElem;      // Mixer element handle.


//  Functions. ----------------------------------------------------------------

// ----------------------------------------------------------------------------
//  Initialises hardware and returns info in sound struct.
// ----------------------------------------------------------------------------
int soundOpen( void );

// ----------------------------------------------------------------------------
//  Calculates volume based on index. Returns value in soundStruct.
// ----------------------------------------------------------------------------
/*
    linear volume = ratio * range + min.
    mapped volume = ( factor^ratio - 1 ) / ( factor - 1 ) * range + min.
    ratio = index / incs.

    factor < 1, logarithmic.
    factor = 1, linear.
    factor > 1, exponential.
*/
long calcVol( float index, float incs, float range, float min, float factor );

// ----------------------------------------------------------------------------
//  Set volume using ALSA mixers.
// ----------------------------------------------------------------------------
int setVol( void );

// ----------------------------------------------------------------------------
//  Increases volume.
// ----------------------------------------------------------------------------
void incVol( void );

// ----------------------------------------------------------------------------
//  Increases volume.
// ----------------------------------------------------------------------------
void decVol( void );

// ----------------------------------------------------------------------------
//  Detaches and closes ALSA.
// ----------------------------------------------------------------------------
void soundClose( void );

#endif
