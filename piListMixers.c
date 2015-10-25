// ****************************************************************************
// ****************************************************************************
/*
    listmixer:

    Simple test program to list all ALSA cards and mixers.

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

#define Version "Version 0.4"

//  Compilation:
//
//  Compile with gcc listmixer.c -o listmixer -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:          D.Faulke    19/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.
//    v0.2 Modified output and added control type.
//    v0.3 Rewrite from scratch.
//    v0.4 Added number of channels to output.
//

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

// ***************************************************************************/
//  Main routine.
// ***************************************************************************/

void main()
{

    int cardNumber = -1;
    int mixerNumber = -1;
    char deviceID[8];
    char channelStr[3];
    unsigned int channels;
    long volMin, volMax;

    int errNum;
    int body = 1;

    // ************************************************************************
    //  ALSA mixer elements.
    // ************************************************************************
    snd_mixer_t *handle;            // Mixer handle.
    snd_mixer_selem_id_t *sid;      // Mixer simple element identifier.
    snd_mixer_elem_t *elem;         // Mixer element handle.


    //  We are cycling through the mixers without knowing the CTL names so
    //  we need some control elements.
    //  Using CTL or HCTL  elements should be avoided for 'safe' ALSA!
    snd_ctl_t *ctl;                 // Simple control handle.
    snd_ctl_elem_id_t *id;          // Control element identifier.


    //  ALSA info elements for retrieving certain information. Should also
    //  be avoided for 'safe' ALSA.
    snd_ctl_card_info_t *card;      //  Control card info container.

    printf( "\nKey:" );
    printf( "\tVol = Volume Control.\n" );
    printf( "\t0/1 = Playback switch.\n" );
    printf( "\tChn = Number of channels.\n\n" );

    while (1)
    {
        // Find next card. Exit loop if none.
        errNum = snd_card_next( &cardNumber );
        if (( errNum < 0 ) || ( cardNumber < 0 )) break;

        sprintf( deviceID, "hw:%i", cardNumber );

        // Open card.
        errNum = snd_ctl_open( &ctl, deviceID, 0 );
        if (errNum < 0 ) continue;

        // Allocate memory for card info.
        snd_ctl_card_info_alloca( &card );
        errNum = snd_ctl_card_info( ctl, card );
        if (errNum < 0 ) continue;


        // ********************************************************************
        //  Print header block and some card specific info.
        // ********************************************************************
        printf( "Card: %s.\n", snd_ctl_card_info_get_name( card ));
        printf( "\t+------------------------------------------" );
        printf( "+---+---+---+-------+-------+\n" );
        printf( "\t| Control ID%-30s ", "" );
        printf( "|Vol|0/1|Chn|  Min  |  Max  |\n" );
        printf( "\t+------------------------------------------" );
        printf( "+---+---+---+-------+-------+\n" );


        // ********************************************************************
        //  Set up control and mixer.
        // ********************************************************************
        // Open an empty mixer and attach a control.
        if ( snd_mixer_open( &handle, 0 ) < 0 )
        {
            printf( "Error opening mixer.\n" );
            return;
        }
        if ( snd_mixer_attach( handle, deviceID ) < 0 )
        {
            printf( "Error attaching control to mixer.\n" );
            return;
        }
        // Register the mixer simple element class and load mixer.
        if ( snd_mixer_selem_register( handle, NULL, NULL ) < 0 )
        {
            printf( "Error registering element class.\n" );
            return;
        }
        if ( snd_mixer_load( handle ) < 0 )
        {
            printf( "Error loading mixer.\n" );
            return;
        }
        snd_mixer_selem_id_alloca( &sid );


        // ********************************************************************
        //  For each mixer element.
        // ********************************************************************
        for ( elem = snd_mixer_first_elem( handle );
              elem;
              elem = snd_mixer_elem_next( elem ))
        {
            // Get ID of mixer element.
            snd_mixer_selem_get_id( elem, sid );

            // Get range of values for control.
            errNum = snd_mixer_selem_get_playback_volume_range(
                            elem, &volMin, &volMax );

            // Check whether element has volume control.
            char *hasVolume = "-";
            char *hasSwitch = "-";
            if ( snd_mixer_selem_has_playback_volume( elem ))
                    hasVolume = "*";
            if ( snd_mixer_selem_has_playback_switch( elem ))
                    hasSwitch = "*";

            // Check which channels the mixer has.
            channels = 0;
            if ( snd_mixer_selem_has_playback_channel( elem,
                        SND_MIXER_SCHN_MONO ) &&
                 snd_mixer_selem_has_playback_channel( elem,
                        SND_MIXER_SCHN_FRONT_LEFT ) &&
                !snd_mixer_selem_has_playback_channel( elem,
                        SND_MIXER_SCHN_FRONT_RIGHT ))
                sprintf( channelStr, " 1 " );
            else
            {

                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_FRONT_LEFT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_FRONT_CENTER ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_FRONT_RIGHT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_SIDE_LEFT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_SIDE_RIGHT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_REAR_LEFT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_REAR_CENTER ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( elem,
                            SND_MIXER_SCHN_REAR_RIGHT ))
                    channels++;

                sprintf( channelStr, "%i.%i",
                         channels,
                         snd_mixer_selem_has_playback_channel( elem,
                             SND_MIXER_SCHN_WOOFER ));
            }

            printf( "\t| %-40s | %s | %s |%s|%+7d|%+7d|%i\n",
                    snd_mixer_selem_id_get_name( sid ),
                    hasVolume,
                    hasSwitch,
                    channelStr,
                    volMin, volMax,
                    snd_mixer_selem_get_index( elem ));
        }
        printf( "\t+------------------------------------------" );
        printf( "+---+---+---+-------+-------+\n\n" );
    }

    snd_mixer_close( handle );

    return;
}
