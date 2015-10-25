// ****************************************************************************
// ****************************************************************************
/*
    piALSA:

    ALSA library for Raspberry Pi.
    provides the following functions:
        void listControls();
            // List ALSA controls for all available cards.
        void listmixers();
            // List ALSA mixers for all available cards.
        int setVolControl( int card, int control,
                           unsigned int volLeft,
                           unsigned int volRight );
            // Set volume using ALSA control elements.
        int setVolMixer( int card, int control,
                         unsigned int volLeft,
                         unsigned int volRight );
            // Set volume using ALSA mixer elements.

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
//    v0.1 Initial version - merged mutliple utility apps.

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>


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
    snd_ctl_card_info_alloca( &ctlCard );
    snd_ctl_elem_value_alloca( &ctlControl );
    snd_ctl_elem_id_alloca( &ctlId );
    snd_ctl_elem_info_alloca( &ctlInfo );


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
        if ( snd_ctl_open( &ctlHandle, deviceID, 0 ) < 0 )
        {
            printf( "Error opening card.\n" );
            continue;
        }

        // Fill control card info element.
        if ( snd_ctl_card_info( ctlHandle, ctlCard ) < 0 )
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
                        snd_ctl_card_info_get_name( ctlCard ));
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
        if ( snd_hctl_open( &hctlHandle, deviceID, 0 ) < 0 )
            printf( "Error opening high level control.\n" );

        // Load high level control element.
        if ( snd_hctl_load( hctlHandle ) < 0 )
            printf( "Error loading high level control.\n" );


        // ********************************************************************
        //  For each control element.
        // ********************************************************************
        for ( hctlElem = snd_hctl_first_elem( hctlHandle );
                    hctlElem;
                    hctlElem = snd_hctl_elem_next( hctlElem ))
        {
            // Get ID of high level control element.
            snd_hctl_elem_get_id( hctlElem, ctlId );


            // ****************************************************************
            //  Determine control type.
            // ****************************************************************
            if ( snd_hctl_elem_info( hctlElem, ctlInfo ) < 0 )
                printf( "Can't get control information.\n" );

            ctlType = snd_ctl_elem_info_get_type( ctlInfo );

            switch ( ctlType )
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
                snd_hctl_elem_get_numid( hctlElem ),
                controlType,
                snd_hctl_elem_get_name( hctlElem ));
        }
        printf( "\t+--------+------------" );
        printf( "+------------------------------------+\n\n" );


        // ********************************************************************
        //  Tidy up.
        // ********************************************************************
        snd_hctl_close( hctlHandle );
        snd_ctl_close( ctlHandle );
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
    char deviceID[8];
    char channelStr[3];
    unsigned int channels;
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
        errNum = snd_ctl_open( &ctlHandle, deviceID, 0 );
        if (errNum < 0 ) continue;

        // Allocate memory for card info.
        snd_ctl_card_info_alloca( &ctlCard );
        errNum = snd_ctl_card_info( ctlHandle, ctlCard );
        if (errNum < 0 ) continue;


        // ********************************************************************
        //  Print header block and some card specific info.
        // ********************************************************************
        printf( "Card: %s.\n", snd_ctl_card_info_get_name( ctlCard ));
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
        if ( snd_mixer_open( &mixerHandle, 0 ) < 0 )
        {
            printf( "Error opening mixer.\n" );
            return -1;
        }
        if ( snd_mixer_attach( mixerHandle, deviceID ) < 0 )
        {
            printf( "Error attaching control to mixer.\n" );
            return -1;
        }
        // Register the mixer simple element class and load mixer.
        if ( snd_mixer_selem_register( mixerHandle, NULL, NULL ) < 0 )
        {
            printf( "Error registering element class.\n" );
            return -1;
        }
        if ( snd_mixer_load( mixerHandle ) < 0 )
        {
            printf( "Error loading mixer.\n" );
            return -1;
        }
        snd_mixer_selem_id_alloca( &mixerId );


        // ********************************************************************
        //  For each mixer element.
        // ********************************************************************
        for ( mixerElem = snd_mixer_first_elem( mixerHandle );
              mixerElem;
              mixerElem = snd_mixer_elem_next( mixerElem ))
        {
            // Get ID of mixer element.
            snd_mixer_selem_get_id( mixerElem, mixerId );

            // Get range of values for control.
            errNum = snd_mixer_selem_get_playback_volume_range(
                            mixerElem, &volMin, &volMax );

            // Check whether element has volume control.
            char *hasVolume = "-";
            char *hasSwitch = "-";
            if ( snd_mixer_selem_has_playback_volume( mixerElem ))
                    hasVolume = "*";
            if ( snd_mixer_selem_has_playback_switch( mixerElem ))
                    hasSwitch = "*";

            // Check which channels the mixer has.
            channels = 0;
            if ( snd_mixer_selem_has_playback_channel( mixerElem,
                        SND_MIXER_SCHN_MONO ) &&
                 snd_mixer_selem_has_playback_channel( mixerElem,
                        SND_MIXER_SCHN_FRONT_LEFT ) &&
                !snd_mixer_selem_has_playback_channel( mixerElem,
                        SND_MIXER_SCHN_FRONT_RIGHT ))
                sprintf( channelStr, " 1 " );
            else
            {

                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_FRONT_LEFT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_FRONT_CENTER ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_FRONT_RIGHT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_SIDE_LEFT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_SIDE_RIGHT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_REAR_LEFT ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_REAR_CENTER ))
                    channels++;
                if ( snd_mixer_selem_has_playback_channel( mixerElem,
                            SND_MIXER_SCHN_REAR_RIGHT ))
                    channels++;

                sprintf( channelStr, "%i.%i",
                         channels,
                         snd_mixer_selem_has_playback_channel( mixerElem,
                             SND_MIXER_SCHN_WOOFER ));
            }

            printf( "\t| %-40s | %s | %s |%s|%+7d|%+7d|\n",
                    snd_mixer_selem_id_get_name( mixerId ),
                    hasVolume,
                    hasSwitch,
                    channelStr,
                    volMin, volMax );
        }
        printf( "\t+------------------------------------------" );
        printf( "+---+---+---+-------+-------+\n\n" );

    }
    snd_mixer_close( mixerHandle );

    return 0;
}

// ****************************************************************************
//  Set volume using ALSA controls.
// ****************************************************************************

int setVolControl( int card, int control,
                   unsigned int volLeft,
                   unsigned int volRight )
{
    char deviceID[8];
    int errNum;
    long volMin, volMax, volStep;

    // ************************************************************************
    //  Set up ALSA control.
    // ************************************************************************
    sprintf( deviceID, "hw:%i", card );

    if ( snd_ctl_open( &ctlHandle, deviceID, 1 ) < 0 )
        return -1;

    // Initialise a simple control element id structure.
    snd_ctl_elem_id_alloca( &ctlId );
    snd_ctl_elem_id_set_numid( ctlId, control );

    // Initialise info element.
    snd_ctl_elem_info_alloca( &ctlInfo );
    snd_ctl_elem_info_set_numid( ctlInfo, control );

    // Is the control valid?
    if ( snd_ctl_elem_info( ctlHandle, ctlInfo ) < 0 )
    {
        snd_ctl_close( ctlHandle );
        return -1;
    }

    // Find type of control.
    // either:
    // SND_CTL_ELEM_TYPE_INTEGER,
    // SND_CTL_ELEM_TYPE_INTEGER64,
    // SND_CTL_ELEM_TYPE_ENUMERATED, etc.
    // Only interested in INTEGER.
    ctlType = snd_ctl_elem_info_get_type( ctlInfo );
    if ( ctlType != SND_CTL_ELEM_TYPE_INTEGER )
    {
        snd_ctl_close( ctlHandle );
        return -1;
    }

    // ************************************************************************
    //  Get some information for selected control.
    // ************************************************************************
    volMin = snd_ctl_elem_info_get_min( ctlInfo );
    volMax = snd_ctl_elem_info_get_max( ctlInfo );
    volStep = snd_ctl_elem_info_get_step( ctlInfo );

    // Initialise the control element value container.
    snd_ctl_elem_value_alloca( &ctlControl );
    snd_ctl_elem_value_set_id( ctlControl, ctlId );


    // ************************************************************************
    //  Set values for selected control.
    // ************************************************************************
    snd_ctl_elem_value_set_integer( ctlControl, 0, volLeft );
    if ( snd_ctl_elem_write( ctlHandle, ctlControl ) < 0 )
    {
        snd_ctl_close( ctlHandle );
        return -1;
    }
    snd_ctl_elem_value_set_integer( ctlControl, 1, volRight );
    if ( snd_ctl_elem_write( ctlHandle, ctlControl ) < 0 )
    {
        snd_ctl_close( ctlHandle );
        return -1;
    }

    // ************************************************************************
    //  Clean up.
    // ************************************************************************
    snd_ctl_close( ctlHandle );

    return 0;
}

// ****************************************************************************
//  Set volume using ALSA mixers.
// ****************************************************************************

int setVolMixer( char *card, char *mixer,
                 int mixerIndex,
                 unsigned int volLeft,
                 unsigned int volRight )
{

    int errNum;
    long volMin, volMax;
    unsigned int channels = 0;


    // ************************************************************************
    //  Set up ALSA mixer.
    // ************************************************************************
    if ( snd_mixer_open( &mixerHandle, 0 ) < 0 )
        return -1;
    if ( snd_mixer_attach( mixerHandle, card ) < 0 )
        return -1;
    if ( snd_mixer_load( mixerHandle ) < 0 )
        return -1;


    // ************************************************************************
    //  Set up mixer simple element register.
    // ************************************************************************
    snd_mixer_selem_id_alloca( &mixerId );
    snd_mixer_selem_id_set_name( mixerId, mixer );
    if ( snd_mixer_selem_register( mixerHandle, NULL, NULL ) < 0 )
        return -1;

    mixerElem = snd_mixer_find_selem( mixerHandle, mixerId );
    if ( mixerElem == NULL )
        return -1;


    // ************************************************************************
    //  Get some information for selected card and mixer.
    // ************************************************************************
    snd_mixer_selem_get_playback_volume_range(
                    mixerElem, &volMin, &volMax );

    if ( snd_mixer_selem_is_playback_mono )
        channels++;
    else
    {
        if ( snd_mixer_selem_has_playback_channel( mixerElem,
                    SND_MIXER_SCHN_FRONT_LEFT ))
            channels++;
        if ( snd_mixer_selem_has_playback_channel( mixerElem,
                    SND_MIXER_SCHN_FRONT_RIGHT ))
            channels++;
    }

    // ************************************************************************
    //  Set values for selected card and mixer.
    // ************************************************************************
    if ( channels == 1 )
    {
        if ( snd_mixer_selem_set_playback_volume_all( mixerElem,
                volLeft ) < 0)
            printf( "Unable to set volume.\n" );
        else
            printf( "Set volume to %d.\n", volLeft );
    }
    else
    if ( channels == 2 )
    {
        if ( snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, volLeft ) < 0 )
            printf( "Unable to set L volume.\n" );
        else
            printf( "Set L volume to %d.\n", volLeft );
        if ( snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_RIGHT, volRight ) < 0 )
            printf( "Unable to set R volume.\n" );
        else
            printf( "Set R volume to %d.\n", volRight );
    }
    else
        printf( "No channels to set.\n" );

    // ************************************************************************
    //  Clean up.
    // ************************************************************************
    snd_mixer_detach( mixerHandle, card );
    snd_mixer_close( mixerHandle );

    return 0;
}
