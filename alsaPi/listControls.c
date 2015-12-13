// ****************************************************************************
// ****************************************************************************
/*
    listControls:

    Lists ALSA control elements for all available cards.

    Copyright 2015 by Darren Faulke <darren@alidaf.co.uk>

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
//  Compile with gcc listControls.c -Wall -o listControls -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	19/10/15
//    Contributors:
//
//    Changelog:
//
//    Initial version.
//

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

// ============================================================================
//  Main routine.
// ============================================================================

int main()
{
    int cardNumber = -1;
    int errNum;
    char *controlType;
    char deviceID[16];

    // ------------------------------------------------------------------------
    //  ALSA control elements.
    // ------------------------------------------------------------------------
    snd_ctl_t *ctl;                 // Simple control handle.
    snd_ctl_elem_id_t *id;          // Simple control element id.
    snd_ctl_elem_value_t *control;  // Simple control element value.
    snd_ctl_elem_type_t type;       // Simple control element type.
    snd_ctl_elem_info_t *info;      // Simple control element info container.
    snd_ctl_card_info_t *card;      // Simple control card info container.

    snd_hctl_t *hctl;               // High level control handle;
    snd_hctl_elem_t *elem;          // High level control element handle.

    // ------------------------------------------------------------------------
    //  Initialise ALSA card and device types.
    // ------------------------------------------------------------------------
    snd_ctl_card_info_alloca( &card );
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_id_alloca( &id );
    snd_ctl_elem_info_alloca( &info );

    // ------------------------------------------------------------------------
    //  Start card section.
    // ------------------------------------------------------------------------
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

        // --------------------------------------------------------------------
        //  Print header block.
        // --------------------------------------------------------------------
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

        // --------------------------------------------------------------------
        //  Start control section.
        // --------------------------------------------------------------------
        // Open an empty high level control.
        if ( snd_hctl_open( &hctl, deviceID, 0 ) < 0 )
            printf( "Error opening high level control.\n" );

        // Load high level control element.
        if ( snd_hctl_load( hctl ) < 0 )
            printf( "Error loading high level control.\n" );

        // --------------------------------------------------------------------
        //  For each control element.
        // --------------------------------------------------------------------
        for ( elem = snd_hctl_first_elem( hctl );
                    elem;
                    elem = snd_hctl_elem_next( elem ))
        {
            // Get ID of high level control element.
            snd_hctl_elem_get_id( elem, id );

            // ----------------------------------------------------------------
            //  Determine control type.
            // ----------------------------------------------------------------
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

        // --------------------------------------------------------------------
        //  Tidy up.
        // --------------------------------------------------------------------
        snd_hctl_close( hctl );
        snd_ctl_close( ctl );
    }

    return 0;
}
