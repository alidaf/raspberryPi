// ****************************************************************************
// ****************************************************************************
/*
    piInfo:

    A collection of tools to print information for the Raspberry Pi.

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
//  Compile with gcc piInfo.c -o piInfo -lasound -lm
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
#include <math.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

// ----------------------------------------------------------------------------
//  Pi header information. Placed here for easier updating.
// ----------------------------------------------------------------------------

// Known Pi revision numbers, corresponding models and board versions.
#define REVISIONS   0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,\
                    0x0008, 0x0009, 0x0010, 0x0012, 0x0013, 0x000d,\
                    0x000e, 0x000f, 0xa01041, 0xa21041
#define MODELS      "B", "B", "B", "B", "B", "A",\
                    "A", "A", "B+", "A+", "B+", "B",\
                    "B", "B", "2B", "2B"
#define VERSIONS    1.0, 1.0, 2.0, 2.0, 2.0, 2.0,\
                    2.0, 2.0, 1.0, 1.0, 1.2, 2.0,\
                    2.0, 2.0, 1.1, 1.1
#define LAYOUTS     1, 1, 2, 2, 2, 2,\
                    2, 2, 3, 3, 3, 2,\
                    2, 2, 3, 3
#define NUMLAYOUTS  3

// GPIO pin layouts - preformatted to make the printout routine easier.
#define LABELS1  " +3.3V", "+5V   ", " GPIO0", "+5V   ", " GPIO1", "GND   ",\
                 " GPIO4", "GPIO14",    "GND", "GPIO15", "GPIO17", "GPIO18",\
                 "GPIO21", "GND   ", "GPIO22", "GPIO23", "  3.3V", "GPIO24",\
                 "GPIO10", "GND   ", " GPIO9", "GPIO25", "GPIO11", "GPIO8 ",\
                 "   GND", "GPIO7 "
#define LABELS2  " +3.3V", "+5V   ", " GPIO2", "+5V   ", " GPIO3", "GND   ",\
                 " GPIO4", "GPIO14", "   GND", "GPIO15", "GPIO17", "GPIO18",\
                 "GPIO21", "GND   ", "GPIO22", "GPIO23",   "3.3V", "GPIO24",\
                 "GPIO10", "GND   ", " GPIO9", "GPIO25", "GPIO11", "GPIO8 ",\
                 "   GND", "GPIO7 "
#define LABELS3  " +3.3V", "+5V   ", " GPIO2", "+5V   ", " GPIO3", "GND   ",\
                 " GPIO4", "GPIO14", "   GND", "GPIO15", "GPIO17", "GPIO18",\
                 "GPIO21", "GND   ", "GPIO22", "GPIO23", " +3.3V", "GPIO24",\
                 "GPIO10", "GND   ", " GPIO9", "GPIO25", "GPIO11", "GPIO8 ",\
                 "   GND", "GPIO7 ", "   DNC", "DNC   ", " GPIO5", "GND   ",\
                 " GPIO6", "GPIO12", "GPIO13", "GND   ", "GPIO19", "GPIO16",\
                 "GPIO26", "GPIO20", "   GND", "GPIO21"

// Header pin, Broadcom GPIO and wiringPi pin numbers for each board revision.
#define HEAD_PINS1  3,  5,  7,  8, 10, 11, 12, 13, 15, 16, 18,\
        		   19, 21, 22, 23, 24, 26
#define BCOM_GPIO1  0,  1,  4, 14, 15, 17, 18, 21, 22, 23, 24,\
		           10,  9, 25, 11,  8,  7
#define WIPI_PINS1  8,  9,  7, 15, 16,  0,  1,  2,  3,  4,  5,\
        		   12, 13,  6, 14, 10, 11

#define HEAD_PINS2  3,  5,  7,  8, 10, 11, 12, 13, 15, 16, 18,\
		           19, 21, 22, 23, 24, 26
#define BCOM_GPIO2  2,  3,  4, 14, 15, 17, 18, 27, 22, 23, 24,\
	        	   10,  9, 25, 11,  8,  7
#define WIPI_PINS2  8,  9,  7, 15, 16,  0,  1,  2,  3,  4,  5,\
		           12, 13,  6, 14, 10, 11

#define HEAD_PINS3  3,  5,  7,  8, 10, 11, 12, 13, 15, 16, 18,\
                   19, 21, 22, 23, 24, 26, 29, 31, 32, 33, 35,\
                   36, 37, 38, 40
#define BCOM_GPIO3  2,  3,  4, 14, 15, 17, 18, 27, 22, 23, 24,\
                   10,  9, 25, 11,  8,  7,  5,  6, 12, 13, 19,\
		           16, 26, 20, 21
#define WIPI_PINS3  8,  9,  7, 15, 16,  0,  1,  2,  3,  4,  5,\
		           12, 13,  6, 14, 10, 11, 21, 22, 26, 23, 24,\
		           27, 25, 28, 29


// Data structure for command line arguments.
struct optionsStruct
{
    bool printGPIO;         // Flag to print GPIO information.
    bool printALSAmixers;   // Flag to print ALSA mixers.
    bool printALSAcontrols; // Flag to print ALSA control.
};


// ----------------------------------------------------------------------------
//  Set default values.
// ----------------------------------------------------------------------------
static struct optionsStruct cmdOptions  =
{
    .printGPIO = false,
    .printALSAmixers = false,
    .printALSAcontrols = false,
};

// ****************************************************************************
//  Pi information functions.
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
    Function will return the '*' column value.
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

// ============================================================================
//  Returns GPIO layout by querying /proc/cpuinfo.
// ============================================================================
static int gpioLayout( bool print )
{
    static unsigned int revisions[] = { REVISIONS };
    static char         *model[]    = { MODELS };
    static float        version[]   = { VERSIONS };
    static unsigned int layout[]    = { LAYOUTS };

    static char *labels1[] = { LABELS1 };
    static char *labels2[] = { LABELS2 };
    static char *labels3[] = { LABELS3 };

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
                    revision = 0;
            }
    fclose ( filePtr ) ;

    if ( print )
    {
        printf( "\nKnown revisions:\n\n" );
        printf( "\t+-------+----------+-------+---------+\n" );
        printf( "\t| Index | Revision | Model | Version |\n" );
        printf( "\t+-------+----------+-------+---------+\n" );
    }
    // Now compare against arrays to find info.
    static unsigned int index = 0;
    static unsigned int i;
    for ( i = 0; i < sizeof( revisions )  / sizeof( int ); i++ )
    {
        if ( print )
            printf( "\t|    %2i | 0x%0.6x |   %-2s  |   %3.1f   |",
                i,
                revisions[ i ],
                model[ i ],
                version[ i ]);
        if ( revision == revisions[ i ] )
        {
            index = i;
            if ( print ) printf( "<-This Pi\n" );
        }
        else
            if ( print ) printf ( "\n" );
    }

    // Print GPIO header layout.
    if ( print )
        printf( "\t+-------+----------+-------+---------+\n\n" );
    if ( index == 0 )
    {
        printf( "Cannot find your card version from known versions.\n\n" );
        return -1;
    }
    else
    {
        if ( print )
        {
            printf( "Raspberry Pi information:\n" );
            printf( "\tRevision = 0x%0.6x.\n", revisions[ index ]);
            printf( "\tModel = %s (ver %3.1f).\n", model[ index ],
                                                   version[ index ]);

            printf( "\t+--------+-----++-----+--------+\n" );
            printf( "\t|  GPIO  | pin || pin |  GPIO  |\n" );
            printf( "\t+--------+-----++-----+--------+\n" );
            switch ( layout[ index ] )
            {
                case 1 :
                    for ( i = 0; i < sizeof( labels1 ) /
                                ( 4 * sizeof( char )); i = i + 2 )
                        printf( "\t| %6s | %3i || %3i | %6s |\n",
                            labels1[ i ], i + 1, i + 2, labels1[ i + 1 ]);
                    break;
                case 2 :
                    for ( i = 0; i < sizeof( labels2 ) /
                                ( 4 * sizeof( char )); i = i + 2 )
                        printf( "\t| %6s | %3i || %3i | %6s |\n",
                             labels2[ i ], i + 1, i + 2, labels2[ i + 1 ]);
                    break;
                case 3 :
                    for ( i = 0; i < sizeof( labels3 ) /
                                ( 4 * sizeof( char )); i = i + 2 )
                        printf( "\t| %6s | %3i || %3i | %6s |\n",
                             labels3[ i ], i + 1, i + 2, labels3[ i + 1 ]);
                    break;
            }
            printf( "\t+--------+-----++-----+--------+\n\n" );
        }
    }
    return layout[ index ];
}

// ============================================================================
//  Returns Broadcom GPIO number for corresponding header pin number.
// ============================================================================
static int getGPIOfromHeaderPin( unsigned int headerPin )
{
    unsigned int headerPins1[] = { HEAD_PINS1 };
    unsigned int headerPins2[] = { HEAD_PINS2 };
    unsigned int headerPins3[] = { HEAD_PINS3 };
    unsigned int bcomGPIO1[]   = { BCOM_GPIO1 };
    unsigned int bcomGPIO2[]   = { BCOM_GPIO2 };
    unsigned int bcomGPIO3[]   = { BCOM_GPIO3 };

    int layout = gpioLayout( 0 );
    unsigned int i;
    int returnCode = -1;

    if ( layout == 1 )
    {
        for ( i = 0; i < sizeof( headerPins1 ) / sizeof( int ); i++ )
            if ( headerPin == headerPins1[ i ]) returnCode = bcomGPIO1[ i ];
    }
    else if ( layout == 2 )
    {
        for ( i = 0; i < sizeof( headerPins2 ) / sizeof( int ); i++ )
            if ( headerPin == headerPins2[ i ]) returnCode = bcomGPIO2[ i ];
    }
    else if ( layout == 3 )
    {
        for ( i = 0; i < sizeof( headerPins3 ) / sizeof( int ); i++ )
            if ( headerPin == headerPins3[ i ]) returnCode = bcomGPIO3[ i ];
    }
    return returnCode;
}

// ============================================================================
//  Returns wiringPi number from Pi header pin number.
// ============================================================================
static int getWiringPifromHeader( unsigned int headerPin )
{

    unsigned int headerPins1[]   = { HEAD_PINS1 };
    unsigned int headerPins2[]   = { HEAD_PINS2 };
    unsigned int headerPins3[]   = { HEAD_PINS3 };
    unsigned int wiringPiPins1[] = { WIPI_PINS1 };
    unsigned int wiringPiPins2[] = { WIPI_PINS2 };
    unsigned int wiringPiPins3[] = { WIPI_PINS3 };

    int layout = gpioLayout( 0 );
    unsigned int i;
    int returnCode = -1;

    if ( layout == 1 )
    {
        for ( i = 0; i < sizeof( headerPins1 ) / sizeof( int ); i++ )
            if ( headerPin == headerPins1[ i ]) returnCode = wiringPiPins1[ i ];
    }
    else if ( layout == 2 )
    {
        for ( i = 0; i < sizeof( headerPins2) / sizeof( int ); i++ )
            if ( headerPin == headerPins2[ i ]) returnCode = wiringPiPins2[ i ];
    }
    else if ( layout == 3 )
    {
        for ( i = 0; i < sizeof( headerPins3) / sizeof( int ); i++ )
            if ( headerPin == headerPins3[ i ]) returnCode = wiringPiPins3[ i ];
    }
    return returnCode;
}

// ============================================================================
//  Returns wiringPi number from GPIO number.
// ============================================================================
static int getWiringPiFromGPIO( unsigned int gpio )
{
    unsigned int bcomGPIO1[]     = { BCOM_GPIO1 };
    unsigned int bcomGPIO2[]     = { BCOM_GPIO2 };
    unsigned int bcomGPIO3[]     = { BCOM_GPIO3 };
    unsigned int wiringPiPins1[] = { WIPI_PINS1 };
    unsigned int wiringPiPins2[] = { WIPI_PINS2 };
    unsigned int wiringPiPins3[] = { WIPI_PINS3 };

    int layout = gpioLayout( 0 );
    unsigned int i;
    int returnCode = -1;

    // Change this to use switch.

    if ( layout == 1 )
    {
        for ( i = 0; i < sizeof( bcomGPIO1 ) / sizeof( int ); i++ )
            if ( gpio == bcomGPIO1[ i ]) returnCode = wiringPiPins1[ i ];
    }
    else if ( layout == 2 )
    {
        for ( i = 0; i < sizeof( bcomGPIO2) / sizeof( int ); i++ )
            if ( gpio == bcomGPIO2[ i ]) returnCode = wiringPiPins2[ i ];
    }
    else if ( layout == 3 )
    {
        for ( i = 0; i < sizeof( bcomGPIO3) / sizeof( int ); i++ )
            if ( gpio == bcomGPIO3[ i ]) returnCode = wiringPiPins3[ i ];
    }
    return returnCode;
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


        // --------------------------------------------------------------------
            //  Determine control type.
        // --------------------------------------------------------------------
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
    { 0, 0, 0, 0, "GPIO:" },
    { "listgpio",     'p', 0, 0, "Print GPIO/wiringPi map." },
    { 0, 0, 0, 0, "ALSA:" },
    { "listmixers",   'm', 0, 0, "print ALSA mixers info." },
    { "listcontrols", 'c', 0, 0, "print ALSA controls info." },
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
            cmdOptions->printGPIO = true;
            break;
        case 'm' :
            cmdOptions->printALSAmixers = true;
            break;
        case 'c' :
            cmdOptions->printALSAcontrols = true;
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
    if ( cmdOptions.printGPIO ) gpioLayout( 1 );
    if ( cmdOptions.printALSAmixers ) listALSAmixers();
    if ( cmdOptions.printALSAcontrols ) listALSAcontrols();

    return 0;
}
