// ****************************************************************************
// ****************************************************************************
/*
    piInfo:

    A collection of informational tools for the Raspberry Pi.

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

#define Version "Version 0.2"

//  Compilation:
//
//  Compile with gcc piInfo.c -o piInfo -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    24/10/2015  This program.
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <stdbool.h>

// ****************************************************************************
//  Pi header information. Placed here for easier updating.
// ****************************************************************************

/* Currently known versions.

        +--------+------+----------+ +---+
        |  Rev   | Year | Md | Ver | | * |
        +--------+------+----------+ +---+
        |   0002 | 2012 | B  | 1.0 | | 1 |
        |   0003 | 2012 | B  | 1.0 | | 1 |
        |   0004 | 2012 | B  | 2.0 | | 2 |
        |   0005 | 2012 | B  | 2.0 | | 2 |
        |   0006 | 2012 | B  | 2.0 | | 2 |
        |   0007 | 2013 | A  | 2.0 | | 2 |
        |   0008 | 2013 | A  | 2.0 | | 2 |
        |   0009 | 2013 | A  | 2.0 | | 2 |
        |   000d | 2012 | B  | 2.0 | | 2 |
        |   000e | 2012 | B  | 2.0 | | 2 |
        |   000f | 2012 | B  | 2.0 | | 2 |
        |   0010 | 2014 | B+ | 1.0 | | 3 |
        |   0011 | 2014 | -- | 1.0 | | - |
        |   0012 | 2014 | A+ | 1.0 | | 3 |
        |   0013 | 2015 | B+ | 1.2 | | 3 |
        | a01041 | 2015 | 2B | 1.1 | | 3 |
        | a21041 | 2015 | 2B | 1.1 | | 3 |
        +--------+------+----+-----+ +---+
*/

/* Pin layouts and GPIO numbers.

         Layout          B Rev 1.0       A Rev 2.0
                                         B Rev 2.0

         Pin  Pin        GPIO GPIO       GPIO GPIO
        +----+----+     +----+----+     +----+----+
        |  1 |  2 |     | -- | -- |     | -- | -- |
        |  3 |  4 |     |  0 | -- |     |  2 | -- |
        |  5 |  6 |     |  1 | -- |     |  3 | -- |
        |  7 |  8 |     |  4 | 14 |     |  4 | 14 |
        |  9 | 10 |     | -- | 15 |     | -- | 15 |
        | 11 | 12 |     | 17 | 18 |     | 17 | 18 |
        | 13 | 14 |     | 21 | -- |     | 27 | -- |
        | 15 | 16 |     | 22 | 23 |     | 22 | 23 |
        | 17 | 18 |     | -- | 24 |     | -- | 24 |
        | 19 | 20 |     | 10 | -- |     | 10 | -- |
        | 21 | 22 |     |  9 | 25 |     |  9 | 25 |
        | 23 | 24 |     | 11 |  8 |     | 11 |  8 |
        | 25 | 26 |     | -- |  7 |     | -- |  7 |
        +====+====+     +----+----+     +====+====+
        | 27 | 28 | }                 { | -- | -- |
        | 29 | 30 | }                 { |  5 | -- |
        | 31 | 32 | }      |A+|       { |  6 | 12 |
        | 33 | 34 | } ---- |B+|  ---- { | 13 | -- |
        | 35 | 36 | }      |2B|       { | 19 | 16 |
        | 37 | 38 | }                 { | 26 | 20 |
        | 39 | 40 | }                 { | -- | 21 |
        +----+----+                     +----+----+
*/

/*
piRevisions[0] = Known board revisions.
           [1] = Models.
           [2] = Versions.
           [3] = Layouts.
*/
#define NUMREVISIONS    16
#define NUMLAYOUTS       3
#define INDEXREVISIONS   0
#define INDEXMODELS      1
#define INDEXVERSIONS    2
#define INDEXLAYOUTS     3

/*
    Needed to define entire array as strings so numeric contents need
    to be converted from strings to numbers.
*/
static char *piRevisions[4][NUMREVISIONS] =
    {{ "0002", "0003", "0004", "0005", "0006", "0007", "0008", "0009",
       "0010", "0012", "0013", "000d", "000e", "000f", "a01041", "a21041" },
     {  "B",  "B",  "B",  "B",  "B",  "A",  "A",  "A",
        "B+", "A+", "B+",  "B", "B",  "B", "2B", "2B" },
     { "1.0", "1.0", "2.0", "2.0", "2.0", "2.0", "2.0", "2.0",
       "1.0", "1.0", "1.2", "2.0", "2.0", "2.0", "1.1", "1.1" },
     { "1", "1", "2", "2", "2", "2", "2", "2",
       "3", "3", "3", "2", "2", "2", "3", "3" }};

// Header pin, Broadcom GPIO and wiringPi pin numbers for each board revision.
/*
piInfo[layout][0] = Labels.
      [layout][1] = GPIO numbers.
      [layout][2] = WiringPi numbers
*/
#define INDEXLABELS 0
#define INDEXGPIO   1

static unsigned char numPins[NUMLAYOUTS] = { 26, 26, 40 };
static char *piInfo[3][2][40] =
    {
      /* Layout 1 labels */
     {{ "+3.3V",  "+5V", "GPIO",  "+5V", "GPIO",  "GND", "GPIO", "GPIO",
          "GND", "GPIO", "GPIO", "GPIO", "GPIO",  "GND", "GPIO", "GPIO",
         "3.3V", "GPIO", "GPIO",  "GND", "GPIO", "GPIO", "GPIO", "GPIO",
          "GND", "GPIO",     "",     "",     "",     "",     "",     "",
             "",     "",     "",     "",     "",     "",     "",     "" },
      /* Layout 1 GPIO */
      { "-1", "-1",  "0", "-1",  "1", "-1",  "4", "14", "-1", "15",
        "17", "18", "21", "-1", "22", "23", "-1", "24", "10", "-1",
         "9", "25", "11",  "8", "-1",  "7", "-1", "-1", "-1", "-1",
        "-1", "-1", "-1", "-1", "-1", "-1", "-1", "-1", "-1", "-1" }},
      /* Layout 2 labels */
     {{ "+3.3V",  "+5V", "GPIO",  "+5V", "GPIO",  "GND", "GPIO", "GPIO",
          "GND", "GPIO", "GPIO", "GPIO", "GPIO",  "GND", "GPIO", "GPIO",
         "3.3V", "GPIO", "GPIO",  "GND", "GPIO", "GPIO", "GPIO", "GPIO",
          "GND", "GPIO",     "",     "",     "",     "",     "",     "",
             "",     "",     "",     "",     "",     "",     "",     "" },
      /* Layout 2 GPIO */
      { "-1", "-1",  "2", "-1",  "3", "-1",  "4", "14", "-1", "15",
        "17", "18", "27", "-1", "22", "23", "-1", "24", "10", "-1",
         "9", "25", "11",  "8", "-1",  "7", "-1", "-1", "-1", "-1" }},
      /* Layout 3 labels */
     {{ "+3.3V",  "+5V", "GPIO",  "+5V", "GPIO",  "GND", "GPIO", "GPIO",\
          "GND", "GPIO", "GPIO", "GPIO", "GPIO",  "GND", "GPIO", "GPIO",\
        "+3.3V", "GPIO", "GPIO",  "GND", "GPIO", "GPIO", "GPIO", "GPIO",\
          "GND", "GPIO",  "DNC",  "DNC", "GPIO",  "GND", "GPIO", "GPIO",\
         "GPIO",  "GND", "GPIO", "GPIO", "GPIO", "GPIO",  "GND", "GPIO" },
      /* Layout 3 GPIO */
      {   "",   "",  "2",   "",  "3",   "",  "4", "14",   "", "15",\
        "17", "18", "27",   "", "22", "23",   "", "24", "10",   "",\
         "9", "25", "11",  "8",   "",  "7",   "",   "",  "5",   "",\
         "6", "12", "13",   "", "19", "16", "26", "20",   "", "21" }}};


// Data structure for command line arguments.
struct optionsStruct
{
    bool listPins;
    bool listMixers;
    bool listControls;
    int  gpio;
    int  pin;
};


// ----------------------------------------------------------------------------
//  Set default values.
// ----------------------------------------------------------------------------
static struct optionsStruct cmdOptions  =
{
    .listPins = false,
    .listMixers = false,
    .listControls = false,
    .gpio = -1,
    .pin = -1
};


// ============================================================================
//  Returns index of piRevisions for matching revision from /proc/cpuinfo.
// ============================================================================
static int getRevisionIndex( void )
{

    FILE *filePtr;                  // File to read in and search.
    static char line[ 512 ];        // Storage for each line read in.
    static char token;              // Storage for character token.
    static unsigned int revision;   // Revision in hex format.

    // Open file for reading.
    filePtr = fopen( "/proc/cpuinfo", "r" );
    if ( filePtr == NULL ) return( -1 );

    while ( fgets( line, sizeof( line ), filePtr ) != NULL )
        if ( strncmp ( line, "Revision", 8 ) == 0 )
            if ( !strncasecmp( "revision\t", line, 9 ))
            {
                // line should be "string" "colon" "hex string" "line break"
                sscanf( line, "%s%s%x%c", &line, &token, &revision, &token );
                    if ( token == '\n' ) break;
            }
    fclose ( filePtr ) ;
    // Now compare against arrays to find info.
    static unsigned int foundPos = -1;
    static unsigned int i;
    for ( i = 0; i < NUMREVISIONS; i++ )
            if ( revision == strtol(
                 piRevisions[INDEXREVISIONS][i], NULL, 16 )) return i;
    return -1;
}

// ============================================================================
//  Print full header pin layout and GPIO/wiringPi mapping.
// ============================================================================
static int listPins()
{

    int index = getRevisionIndex();
    if ( index < 0 ) return -1;
    int layout = atoi( piRevisions[INDEXLAYOUTS][index]);
    if ( layout < 1 ) return layout;
    int revision = strtol( piRevisions[INDEXREVISIONS][index], NULL, 16 );

    printf( "\nKnown revisions:\n\n" );
    printf( "\t+-------+----------+-------+---------+\n" );
    printf( "\t| Index | Revision | Model | Version |\n" );
    printf( "\t+-------+----------+-------+---------+\n" );

    // Now compare against arrays to find info.
    static unsigned int foundPos = -1;
    static unsigned int i;
    static char *padding;
    for ( i = 0; i < NUMREVISIONS; i++ )
    {
        if ( strtol( piRevisions[INDEXREVISIONS][i], NULL, 16 ) < 0xfff )
            padding = "  ";
        else
            padding = "";
        printf( "\t|    %2i |   %s%0.4x |   %-2s  |   %3.1f   |",
                i,
                padding,
                strtol( piRevisions[INDEXREVISIONS][i], NULL, 16 ),
                piRevisions[INDEXMODELS][i],
                atof( piRevisions[INDEXVERSIONS][i] ));
        if ( i == index )
            printf( "<-This Pi\n" );
        else
            printf ( "\n" );
    }
    printf( "\t+-------+----------+-------+---------+\n\n" );

    // Print GPIO header layout.
    if ( foundPos < 0 )
    {
        printf( "Cannot find your card version from known versions.\n\n" );
        return -1;
    }
    else
    {
        printf( "Raspberry Pi information:\n\n" );
        printf( "\tModel = %s (ver %3.1f), board revision %0.4x.\n",
                piRevisions[INDEXMODELS][index],
                atof( piRevisions[INDEXVERSIONS][index] ),
                strtol( piRevisions[INDEXREVISIONS][index], NULL, 16 ));
        printf( "\tHeader pin layout %i:\n", layout );

        printf( "\t+------+-------++-----+-----++-------+------+\n" );
        printf( "\t| gpio | label || pin | pin || label | gpio |\n" );
        printf( "\t+------+-------++-----+-----++-------+------+\n" );

        static int gpio1, gpio2;
        for ( i = 0; i < numPins[layout-1]; i = i + 2 )
        {
            gpio1 = atoi( piInfo[layout-1][INDEXGPIO][i] );
            gpio2 = atoi( piInfo[layout-1][INDEXGPIO][i+1] );

            if ( gpio1 < 0 )
                 printf( "\t|   %2s ", "--" );
            else printf( "\t|   %2i ", gpio1 );

            printf( "| %5s || %3i | %-3i || %-5s ",
                    piInfo[layout-1][INDEXLABELS][i],
                    i + 1, i + 2,
                    piInfo[layout-1][INDEXLABELS][i+1] );

            if ( gpio2 < 0 )
                 printf( "| %-2s   |\n", "--" );
            else printf( "| %-2i   |\n", gpio2 );
        }
        printf( "\t+------+-------++-----" );
        printf( "+-----++-------+------+\n\n" );
    }
    return 0;
}


// ============================================================================
//  Returns Broadcom GPIO number for corresponding header pin number.
// ============================================================================
static int getGPIO( unsigned int pin )
{
    int layout = atoi( piRevisions[INDEXLAYOUTS][getRevisionIndex()] );
    if ( layout < 1 ) return -1;
    if (( pin < 1 ) || ( pin > numPins[layout-1] )) return -1;

    return atoi( piInfo[layout-1][INDEXGPIO][pin-1] );
}


// ============================================================================
//  Returns header pin number for corresponding GPIO.
// ============================================================================
static int getPin( unsigned int gpio )
{
    int layout = atoi( piRevisions[INDEXLAYOUTS][getRevisionIndex()] );
    unsigned int i;

    if ( layout < 1 ) return -1;

    for ( i = 0; i < numPins[layout-1]; i++ )
        if ( gpio == atoi( piInfo[layout-1][INDEXGPIO][i] ))
              return i + 1;

    return -1;
}


// ****************************************************************************
//  ALSA functions.
// ****************************************************************************

// ============================================================================
//  Lists ALSA mixers for all available cards.
// ============================================================================
static void listALSAmixers( void )
{
    snd_ctl_t *ctlHandle;               // Simple control handle.
    snd_ctl_elem_id_t *ctlId;           // Simple control element id.
    snd_ctl_elem_value_t *ctlControl;   // Simple control element value.
    snd_ctl_elem_type_t ctlType;        // Simple control element type.
    snd_ctl_elem_info_t *ctlInfo;       // Simple control element info container.
    snd_ctl_card_info_t *ctlCard;       // Simple control card info container.

    snd_mixer_t *mixerHandle;           // Mixer handle.
    snd_mixer_selem_id_t *mixerId;      // Mixer simple element identifier.
    snd_mixer_elem_t *mixerElem;        // Mixer element handle.

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
        // Find next card. Exit i if none.
        snd_card_next( &cardNumber );
        if ( cardNumber < 0 ) break;

        sprintf( deviceID, "hw:%i", cardNumber );

        // Open card.
        snd_ctl_open( &ctlHandle, deviceID, 0 );
        snd_ctl_card_info_alloca( &ctlCard );
        snd_ctl_card_info( ctlHandle, ctlCard );


        // --------------------------------------------------------------------
        //  Print header block and some card specific info.
        // --------------------------------------------------------------------
        printf( "Card: %s.\n", snd_ctl_card_info_get_name( ctlCard ));
        printf( "\t+------------------------------------------" );
        printf( "+---+---+---+-------+-------+\n" );
        printf( "\t| Control ID%-30s ", "" );
        printf( "|Vol|0/1|Chn|  Min  |  Max  |\n" );
        printf( "\t+------------------------------------------" );
        printf( "+---+---+---+-------+-------+\n" );


        // --------------------------------------------------------------------
        //  Set up control and mixer.
        // --------------------------------------------------------------------
        snd_mixer_open( &mixerHandle, 0 );
        snd_mixer_attach( mixerHandle, deviceID );
        snd_mixer_selem_register( mixerHandle, NULL, NULL );
        snd_mixer_load( mixerHandle );
        snd_mixer_selem_id_alloca( &mixerId );


        // --------------------------------------------------------------------
        //  For each mixer element.
        // --------------------------------------------------------------------
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

    return;
}


// ============================================================================
//  Lists ALSA controls for all available cards.
// ============================================================================
int listALSAcontrols( void )
{
    static int cardNumber = -1;
    static int errNum;
    static char *controlType;
    static char deviceID[16];


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
        if (( errNum < 0 ) || ( cardNumber < 0 )) return -1;

        // Concatenate strings to get card's control interface.
        sprintf( deviceID, "hw:%i", cardNumber );

        //  Try to open card.
        if ( snd_ctl_open( &ctl, deviceID, 0 ) < 0 )
        {
            printf( "Error opening card.\n" );
            return -1;
        }

        // Fill control card info element.
        if ( snd_ctl_card_info( ctl, card ) < 0 )
        {
            printf( "Error getting card info.\n" );
            return -1;
        }

        // --------------------------------------------------------------------
        //  Additional information that may be useful.
        // --------------------------------------------------------------------
//        char *hctlName = snd_ctl_ascii_elem_id_get( id );
//        printf( "HCTL name = %s.\n", hctlName );
//        int cardNum = snd_ctl_card_info_get_card( card );
//        printf( "CTL card number = %i.\n", cardNum );
//        const char *ctlInfo = snd_ctl_card_info_get_id( card );
//        printf( "CTL info = %s.\n", ctlInfo );


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
        {
            printf( "Error opening high level control.\n" );
            return -1;
        }
        // Load high level control element.
        if ( snd_hctl_load( hctl ) < 0 )
        {
            printf( "Error loading high level control.\n" );
            return -1;
        }


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


// ****************************************************************************
//  Command line option functions.
// ****************************************************************************

// ============================================================================
//  argp documentation.
// ============================================================================
const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi information.";
static const char args_doc[] = "piInfo <options>";


// ============================================================================
//  Command line argument definitions.
// ============================================================================
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Tables:" },
    { "listpins", 'p', 0, 0, "Print full map of pin functions." },
    { 0, 0, 0, 0, "Conversion:" },
    { "getgpio",  'g', "int", 0, "Return GPIO from header pin." },
    { "getpin",   'h', "int", 0, "Return header pin from GPIO." },
    { 0, 0, 0, 0, "ALSA:" },
    { "listmixers",   'm', 0, 0, "print ALSA mixer info." },
    { "listcontrols", 'c', 0, 0, "print ALSA control info." },
    { 0 }
};


// ============================================================================
//  Command line argument parser.
// ============================================================================
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    struct optionsStruct *cmdOptions = state->input;
    switch ( param )
    {
        case 'p' :
            cmdOptions->listPins = true;
            break;
        case 'm' :
            cmdOptions->listMixers = true;
            break;
        case 'c' :
            cmdOptions->listControls = true;
            break;
        case 'g' :
            cmdOptions->pin = atoi( arg );
            break;
        case 'h' :
            cmdOptions->gpio = atoi( arg );
            break;
    }
    return 0;
};


// ============================================================================
//  argp parser parameter structure.
// ============================================================================
static struct argp argp = { options, parse_opt, args_doc, doc };


// ============================================================================
//  Main section.
// ============================================================================
int main( int argc, char *argv[] )
{

    // ------------------------------------------------------------------------
    //  Get command line arguments and check within bounds.
    // ------------------------------------------------------------------------
    argp_parse( &argp, argc, argv, 0, 0, &cmdOptions );


    // ------------------------------------------------------------------------
    //  Print out any information requested on command line.
    // ------------------------------------------------------------------------
    if ( cmdOptions.listPins ) listPins();
    if ( cmdOptions.listMixers ) listALSAmixers();
    if ( cmdOptions.listControls ) listALSAcontrols();

    if ( cmdOptions.pin > 0 )
    {
        int gpio = getGPIO( cmdOptions.pin );
        if ( gpio < 0 ) printf( "\nNo GPIO for that pin.\n\n" );
        else printf( "\nHeader pin %i = GPIO%i.\n\n", cmdOptions.pin, gpio);
    }

    if ( cmdOptions.gpio >= 0 )
    {
        int pin = getPin( cmdOptions.gpio );
        if ( pin < 0 ) printf( "\nGPIO doesn't exist for this board.\n\n" );
        else printf( "\nGPIO%i = header pin %i.\n\n", cmdOptions.gpio, pin );
    }

    return 0;
}
