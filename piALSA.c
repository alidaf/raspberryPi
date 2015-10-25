// ****************************************************************************
// ****************************************************************************
/*
    piALSA:

    ALSA library for Raspberry Pi.

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
//  Compile with gcc -c piALSA.c -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	25/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>


// ************************************************************************
//  ALSA control elements.
// ************************************************************************
snd_ctl_t *ctl;                 // Simple control handle.
snd_ctl_elem_id_t *id;          // Simple control element id.
snd_ctl_elem_value_t *control;  // Simple control element value.
snd_ctl_elem_type_t type;       // Simple control element type.
snd_ctl_elem_info_t *info;      // Simple control element info container.
snd_ctl_card_info_t *card;      // Simple control card info container.
snd_hctl_t *hctl;               // High level control handle;
snd_hctl_elem_t *elem;          // High level control element handle.


// ************************************************************************
//  ALSA mixer elements.
// ************************************************************************
snd_mixer_t *handle;            // Mixer handle.
snd_mixer_selem_id_t *sid;      // Mixer simple element identifier.
snd_mixer_elem_t *elem;         // Mixer element handle.


// ****************************************************************************
//  List ALSA controls for all available cards.
// ****************************************************************************

int listControls()
{
    int cardNumber = -1;
    int errNum;
    char *controlType;
    char deviceID[16];


    // ************************************************************************
    //  Initialise ALSA card and device types.
    // ************************************************************************
    snd_ctl_card_info_alloca( &card );
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_id_alloca( &id );
    snd_ctl_elem_info_alloca( &info );


    // ************************************************************************
    //  Start card section.
    // ************************************************************************
    // For each card.
    while (1)
    {
        // Find next card number. If < 0 then returns 1st card.
        errNum = snd_card_next( &cardNumber );
        if (( errNum < 0 ) || ( cardNumber < 0 )) break;

        // Concatenate strings to get card's control interface.
        sprintf( deviceID, "hw:%i", cardNumber );

        //  Try to open card.
        if ( snd_ctl_open( &ctl, deviceID, 0 ) < 0 )
        {
            printf( "Error opening card.\n" );
            continue;
        }

        // Fill control card info element.
        if ( snd_ctl_card_info( ctl, card ) < 0 )
        {
            printf( "Error getting card info.\n" );
            continue;
        }


        // ********************************************************************
        //  Print header block.
        // ********************************************************************
        printf( "\t+-----------------------------" );
        printf( "-----------------------------+\n" );
        printf( "\t| Card: %d - %-46s |",
                        cardNumber,
                        snd_ctl_card_info_get_name( card ));
        printf( "\n" );
        printf( "\t+--------+------------" );
        printf( "+------------------------------------+\n" );
        printf( "\t| Device | Type       " );
        printf( "| Name                               |\n" );
        printf( "\t+--------+------------" );
        printf( "+------------------------------------+\n" );


        // ********************************************************************
        //  Start control section.
        // ********************************************************************
        // Open an empty high level control.
        if ( snd_hctl_open( &hctl, deviceID, 0 ) < 0 )
            printf( "Error opening high level control.\n" );

        // Load high level control element.
        if ( snd_hctl_load( hctl ) < 0 )
            printf( "Error loading high level control.\n" );


        // ********************************************************************
        //  For each control element.
        // ********************************************************************
        for ( elem = snd_hctl_first_elem( hctl );
                    elem;
                    elem = snd_hctl_elem_next( elem ))
        {
            // Get ID of high level control element.
            snd_hctl_elem_get_id( elem, id );


            // ****************************************************************
            //  Determine control type.
            // ****************************************************************
            if ( snd_hctl_elem_info( elem, info ) < 0 )
                printf( "Can't get control information.\n" );

            type = snd_ctl_elem_info_get_type( info );

            switch ( type )
            {
                case SND_CTL_ELEM_TYPE_NONE:
                    controlType = "None";
                    break;
                case SND_CTL_ELEM_TYPE_BOOLEAN:
                    controlType = "Boolean";
                    break;
                case SND_CTL_ELEM_TYPE_INTEGER:
                    controlType = "Integer";
                    break;
                case SND_CTL_ELEM_TYPE_INTEGER64:
                    controlType = "Integer64";
                    break;
                case SND_CTL_ELEM_TYPE_ENUMERATED:
                    controlType = "Enumerated";
                    break;
                case SND_CTL_ELEM_TYPE_BYTES:
                    controlType = "Bytes";
                    break;
                case SND_CTL_ELEM_TYPE_IEC958:
                    controlType = "IEC958";
                    break;
                default:
                    controlType = "Not Found";
                    break;
            }
            printf( "\t| %-6i | %-10s | %-34s |\n",
                snd_hctl_elem_get_numid( elem ),
                controlType,
                snd_hctl_elem_get_name( elem ));
        }
        printf( "\t+--------+------------" );
        printf( "+------------------------------------+\n\n" );


        // ********************************************************************
        //  Tidy up.
        // ********************************************************************
        snd_hctl_close( hctl );
        snd_ctl_close( ctl );
    }

    return 0;
}

// ****************************************************************************
//  Lists ALSA mixers for all available cards.
// ****************************************************************************

int listMixers()
{

    int cardNumber = -1;
    int mixerNumber = -1;
    const char *cardName = "";
    const char *mixerName = "";
    char deviceID[8];
    long volMin, volMax;

    int errNum;
    int body = 1;

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
        printf( "+---+---------+---------+\n" );
        printf( "\t| Control ID%-30s ", "" );
        printf( "| * |   Min   |   Max   |\n" );
        printf( "\t+------------------------------------------" );
        printf( "+---+---------+---------+\n" );


        // ********************************************************************
        //  Set up control and mixer.
        // ********************************************************************
        // Open an empty mixer and attach a control.
        if ( snd_mixer_open( &handle, 0 ) < 0 )
        {
            printf( "Error opening mixer.\n" );
            return -1;
        }
        if ( snd_mixer_attach( handle, deviceID ) < 0 )
        {
            printf( "Error attaching control to mixer.\n" );
            return -1;
        }
        // Register the mixer simple element class and load mixer.
        if ( snd_mixer_selem_register( handle, NULL, NULL ) < 0 )
        {
            printf( "Error registering element class.\n" );
            return -1;
        }
        if ( snd_mixer_load( handle ) < 0 )
        {
            printf( "Error loading mixer.\n" );
            return -1;
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
            char *hasControl = "-";
            if ( snd_mixer_selem_has_playback_volume( elem ))
                    hasControl = "*";

            printf( "\t| %-40s | %s | %+7d | %+7d |\n",
                    snd_mixer_selem_id_get_name( sid ),
                    hasControl,
                    volMin, volMax );
        }
        printf( "\t+------------------------------------------" );
        printf( "+---+---------+---------+\n" );
        printf( "Mixers elements marked with '*' have volume control.\n\n" );
    }

    snd_mixer_close( handle );

    return 0;
}
