/*****************************************************************************/
/*                                                                           */
/*    Simple test program to list all ALSA cards and devices.                */
/*                                                                           */
/*    Copyright  2015 by Darren Faulke <darren@alidaf.co.uk>                 */
/*                                                                           */
/*    This program is free software; you can redistribute it and/or modify   */
/*    it under the terms of the GNU General Public License as published by   */
/*    the Free Software Foundation, either version 2 of the License, or      */
/*    (at your option) any later version.                                    */
/*                                                                           */
/*    This program is distributed in the hope that it will be useful,        */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          */
/*    GNU General Public License for more details.                           */
/*                                                                           */
/*    You should have received a copy of the GNU General Public License      */
/*    along with this program. If not, see <http://www.gnu.org/licenses/>.   */
/*                                                                           */
/*****************************************************************************/

#define Version "Version 0.2"

/*****************************************************************************/
/*                                                                           */
/*    Authors:            D.Faulke                    19/10/15               */
/*    Contributors:                                                          */
/*                                                                           */
/*    Changelog:                                                             */
/*                                                                           */
/*    v0.1 Initial version.                                                  */
/*    v0.2 Modified output and added control type.                           */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
//#include <alsa/mixer.h>

static struct snd_mixer_selem_regopt smixerOptions;

/*****************************************************************************/
/*                                                                           */
/* Main routine.                                                             */
/*                                                                           */
/*****************************************************************************/

void main()
{

    int cardNumber = -1;
    char *controlType;

    int errNum;
    char deviceID[16];

    snd_ctl_t *ctl;                 // Simple control handle.
    snd_ctl_elem_id_t *id;          // Simple control element id.
    snd_ctl_elem_value_t *control;  // Simple control element value.
    snd_ctl_elem_type_t type;       // Simple control element type.
    snd_ctl_elem_info_t *info;      // Simple control element info container.
    snd_ctl_card_info_t *card;      // Simple control card info container.
    snd_hctl_t *hctl;               // High level control handle;
    snd_hctl_elem_t *elem;          // High level control element handle.

    // Initialise ALSA card and device types.
    snd_ctl_card_info_alloca( &card );
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_id_alloca( &id );
    snd_ctl_elem_info_alloca( &info );

/*****************************************************************************/
/*    Start card section.                                                    */
/*****************************************************************************/

    // For each card.
    while (1)
    {
        // Find next card number. If < 0 then returns 1st card.
        errNum = snd_card_next( &cardNumber );
        if (( errNum < 0 ) || ( cardNumber < 0 )) break;

        // Concatenate strings to get card's control interface.
        sprintf( deviceID, "hw:%i", cardNumber );

        //  Try to open card.
        errNum = snd_ctl_open( &ctl, deviceID, 0 );
        if ( errNum < 0 )
        {
            printf( "Error opening card.\n" );
            continue;
        }

        // Fill control card info element.
        errNum = snd_ctl_card_info( ctl, card );
        if ( errNum < 0 )
        {
            printf( "Error getting card info.\n" );
            continue;
        }

        // Print header block.
        printf( "\t+----------------------------------------------------------+\n" );
        printf( "\t| Card: %d - %-46s |",
                        cardNumber,
                        snd_ctl_card_info_get_name( card ));
        printf( "\n" );
        printf( "\t+--------+------------+------------------------------------+\n" );
        printf( "\t| Device | Type       | Name                               |\n" );
        printf( "\t+--------+------------+------------------------------------+\n" );

/*****************************************************************************/
/*    Start control section.                                                 */
/*****************************************************************************/

//        printf( "Opened high level control.\n" );
        // Open an empty high level control.
        errNum = snd_hctl_open( &hctl, deviceID, 0 );
        if ( errNum < 0 )
        {
            printf( "Error opening high level control.\n" );
        }
//        printf( "Opened high level control.\n" );

        // Load high level control element.
        errNum = snd_hctl_load( hctl );
        if ( errNum < 0 )
        {
            printf( "Error loading high level control.\n" );
        }
//        printf( "Loaded high level control.\n" );

        // Find number of controls.
//        int numControls = snd_hctl_get_count( hctl );
//        int count;

/*****************************************************************************/
/*    For each control element.                                              */
/*****************************************************************************/

        for ( elem = snd_hctl_first_elem( hctl );
                    elem;
                    elem = snd_hctl_elem_next( elem ))

        {
            // Get ID of high level control element.
            snd_hctl_elem_get_id( elem, id );

//            printf( "Finding type.\n" );
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

        printf( "\t+--------+------------+------------------------------------+\n" );

        snd_hctl_close( hctl );
        snd_ctl_close( ctl );
    }

    return;
}
