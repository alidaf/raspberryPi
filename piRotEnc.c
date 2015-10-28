// ****************************************************************************
// ****************************************************************************
/*
    piRotEnc:

    Rotary encoder control app for the Raspberry Pi.

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
//  Compile with gcc piRotEnc.c -o piRotEnc -lwiringPi -lasound -lm
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    24/10/2015
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version - rewrite of rotencvol.c.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

// ****************************************************************************
//  Pi header information. Placed here for easier updating.
// ****************************************************************************

// Known Pi revision numbers and corresponding models and board versions.
#define REVISION  0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,\
                  0x0008, 0x0009, 0x0010, 0x0012, 0x0013, 0x000d,\
                  0x000e, 0x000f, 0xa01041, 0xa21041
#define MODEL     "B", "B", "B", "B", "B", "A",\
                  "A", "A", "B+", "A+", "B+", "B",\
                  "B", "B", "2B", "2B"
#define VERSION   1.0, 1.0, 2.0, 2.0, 2.0, 2.0,\
                  2.0, 2.0, 1.0, 1.0, 1.2, 2.0,\
                  2.0, 2.0, 1.1, 1.1
#define LAYOUT    1, 1, 2, 2, 2, 2,\
                  2, 2, 3, 3, 3, 2,\
                  2, 2, 3, 3
#define NUMLAYOUTS 3

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

// Header pin, Broadcom pin (GPIO) and wiringPi pin numbers for each board revision.
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


// To Do.
//
// Use external libraries to list mixers and find Pi version etc.
//  - listmixer, listctl, piPins.
// Add parameter options to piPins to print layout and return broadcom numbers.
// Use global struct for command line arguments.
// Void functions for any rotary encoder control, registered with wiringPi.
// Possible multithreading of each function.
// Function to open, set and close device for each encoder tic.
//  - local struct for alsa.
//  - could possibly be library?
// Replace wiringPi functions with stdio functions.
//

// Data structure for command line arguments.
struct structArgs
{
    char *card;     // ALSA card name.
    char *mixer;    // Alsa mixer name.
    int gpioA;      // GPIO pin for vol rotary encoder.
    int gpioB;      // GPIO pin for vol rotary encoder.
    int gpioC;      // GPIO pin for mute button.
    int gpioD;      // GPIO pin for bal rotary encoder.
    int gpioE;      // GPIO pin for bal rotary encoder.
    int gpioF;      // GPIO pin for bal reset button.
    int wPiPinA;    // Mapped wiringPi pin.
    int wPiPinB;    // Mapped wiringPi pin.
    int wPiPinC;    // Mapped wiringPi pin.
    int wPiPinD;    // Mapped wiringPi pin.
    int wPiPinE;    // Mapped wiringPi pin.
    int wPiPinF;    // Mapped wiringPi pin.
    int initVol;    // Initial volume (%).
    int minVol;     // Minimum soft volume (%).
    int maxVol;     // Maximum soft volume (%).
    int incVol;     // No. of increments over volume range.
    float facVol;   // Volume shaping factor.
    int balance;    // Volume L/R balance.
    int ticDelay;   // Delay between encoder tics.
    int prOutput;   // Flag to print output.
    int prRanges;   // Flag to print ranges.
    int prDefaults; // Flag to print defaults.
    int prSet;      // Flag to print set parameters.
    int prMapping;  // Flag to print GPIO mapping.
};

// Data structure for argument bounds.
struct boundsStruct
{
    int volumeLow;
    int volumeHigh;
    int balanceLow;
    int balanceHigh;
    float factorLow;
    float factorHigh;
    int incLow;
    int incHigh;
    int delayLow;
    int delayHigh;
};

// Data structure for volume definition.
struct volStruct
{
    char *card;
    char *mixer;
    float factor;
    unsigned int volume;
    int balance;
    unsigned int debug;
};


// ****************************************************************************
//  Set default values.
// ****************************************************************************

static struct structArgs cmdArgs, defaultArgs =
{
    .card = "hw:0",     // 1st ALSA card.
    .control = "PCM",   // Default ALSA control.
    .gpioA = 23,        // GPIO 23 for rotary encoder.
    .gpioB = 24,        // GPIO 24 for rotary encoder.
    .gpioC = 2,         // GPIO 0 (for mute/unmute push button).
    .wPiPinA = 4,       // WiringPi number equivalent to GPIO 23.
    .wPiPinB = 5,       // WiringPi number equivalent to GPIO 24.
    .wPiPinC = 8,       // WiringPi number equivalent to GPIO 2.
    .initVol = 0,       // Mute.
    .minVol = 0,        // 0% of Maximum output level.
    .maxVol = 100,      // 100% of Maximum output level.
    .balance = 0,       // L = R.
    .incVol = 20,       // 20 increments from 0 to 100%.
    .facVol = 1,        // Volume change rate factor.
    .ticDelay = 100,    // 100 cycles between tics.
    .prOutput = 0,      // No output printing.
    .prRanges = 0,      // No range printing.
    .prDefaults = 0,    // No defaults printing.
    .prSet = 0,         // No set parameters printing.
    .prMapping = 0      // No wiringPi map printing.
};

static struct boundsStruct paramBounds =
{
    .volumeLow = 0,     // Lower bound for volume.
    .volumeHigh = 100,  // Upper bound for volume.
    .balanceLow = -100, // Lower bound for balance.
    .balanceHigh = 100, // Upper bound for balance.
    .factorLow = 0.001, // Lower bound for volume shaping factor.
    .factorHigh = 10,   // Upper bound for volume shaping factor.
    .incLow = 1,        // Lower bound for no. of volume increments.
    .incHigh = 100,     // Upper bound for no. of volume increments.
    .delayLow = 50,     // Lower bound for delay between tics.
    .delayHigh = 1000   // Upper bound for delay between tics.
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

// ****************************************************************************
//  Returns GPIO layout by querying /proc/cpuinfo.
// ****************************************************************************

static int piInfoLayout( unsigned int prOut )
{
    int revision[] = { REVISION };
    char *model[] = { MODEL };
    float version[] = { VERSION };
    int layout[] = { LAYOUT };

    char *labels1[] = { LABELS1 };
    char *labels2[] = { LABELS2 };
    char *labels3[] = { LABELS3 };

    FILE *pFile;        // File to read in and search.
    char line[ 512 ];   // Storage for each line read in.
    char term;          // Storage for end of line character.
    static int rev;     // Revision in hex format.
    static int loop;    // Loop incrementer.
    static int index;   // Index of found revision.

    // Open file for reading.
    pFile = fopen( "/proc/cpuinfo", "r" );
    if ( pFile == NULL )
    {
        perror( "Error" );
        return( -1 );
    }

    while ( fgets( line, sizeof( line ), pFile ) != NULL )
    {
        if ( strncmp ( line, "Revision", 8 ) == 0 )
        {
            if ( !strncasecmp( "revision\t", line, 9 ))
            {
                // line should be "string" "colon" "hex string" "line break"
                sscanf( line, "%s%s%x%c", &line, &term, &rev, &term );
                {
                    if ( term == '\n' ) break;
                    rev = 0;
                }
            }
        }
    }
    fclose ( pFile ) ;

    if ( prOut )
    {
        printf( "\nKnown revisions:\n\n" );
    	printf( "\t+-------+----------+-------+---------+\n" );
    	printf( "\t| Index | Revision | Model | Version |\n" );
    	printf( "\t+-------+----------+-------+---------+\n" );
    }
    // Now compare against arrays to find info.
    index = 0;
    for ( loop = 0; loop < sizeof( revision )  / sizeof( int ); loop++ )
    {
    if ( prOut )
    {
        printf( "\t|    %2i | 0x%0.6x |   %-2s  |   %3.1f   |",
                loop,
                revision[ loop ],
                model[ loop ],
                version[ loop ]);
    }
        if ( rev == revision[ loop ] )
        {
            index = loop;
            if ( prOut ) printf( "<-This Pi\n" );
        }
        else
            if ( prOut ) printf ( "\n" );
    }

    // Print GPIO header layout.
    if (prOut )
        printf( "\t+-------+----------+-------+---------+\n\n" );
    if ( index == 0 )
    {
        if ( prOut )
            printf( "Cannot find your card version from known versions.\n\n" );
        return -1;
    }
    else
    {
        if ( prOut )
        {
            printf( "Raspberry Pi information:\n" );
            printf( "\tRevision = 0x%0.6x.\n", revision[ index ]);
            printf( "\tModel = %s (ver %3.1f).\n",
                            model[ index ],
                            version[ index ]);

            printf( "\t+--------+-----++-----+--------+\n" );
            printf( "\t|  GPIO  | pin || pin |  GPIO  |\n" );
            printf( "\t+--------+-----++-----+--------+\n" );
            switch ( layout[ index ] )
            {
                case 1 :
                    for ( loop = 0; loop < sizeof( labels1 ) /
                                ( 4 * sizeof( char )); loop = loop + 2 )
                        printf( "\t| %6s | %3i || %3i | %6s |\n",
                            labels1[ loop ], loop + 1, loop + 2, labels1[ loop + 1 ]);
                    break;
                case 2 :
                    for ( loop = 0; loop < sizeof( labels2 ) /
                                ( 4 * sizeof( char )); loop = loop + 2 )
                        printf( "\t| %6s | %3i || %3i | %6s |\n",
                             labels2[ loop ], loop + 1, loop + 2, labels2[ loop + 1 ]);
                    break;
                case 3 :
                    for ( loop = 0; loop < sizeof( labels3 ) /
                                ( 4 * sizeof( char )); loop = loop + 2 )
                        printf( "\t| %6s | %3i || %3i | %6s |\n",
                             labels3[ loop ], loop + 1, loop + 2, labels3[ loop + 1 ]);
                    break;
            }
            printf( "\t+--------+-----++-----+--------+\n\n" );
        }
    }

    return layout[ index ];
}

// ****************************************************************************
//  Returns Broadcom GPIO number from Pi header pin number.
// ****************************************************************************

static int piInfoGetGPIOHead( unsigned int piPin )
{
    unsigned int headPins1[] = { HEAD_PINS1 };
    unsigned int headPins2[] = { HEAD_PINS2 };
    unsigned int headPins3[] = { HEAD_PINS3 };
    unsigned int bcomGPIO1[] = { BCOM_GPIO1 };
    unsigned int bcomGPIO2[] = { BCOM_GPIO2 };
    unsigned int bcomGPIO3[] = { BCOM_GPIO3 };

    int layout = piInfoLayout( 0 );
    unsigned int loop;
    int retCode = -1;

    if ( layout == 1 )
    {
        for ( loop = 0; loop < sizeof( headPins1 ) / sizeof( int ); loop++ )
            if ( piPin == headPins1[ loop ]) retCode = bcomGPIO1[ loop ];
    }
    else if ( layout == 2 )
    {
        for ( loop = 0; loop < sizeof( headPins2 ) / sizeof( int ); loop++ )
            if ( piPin == headPins2[ loop ]) retCode = bcomGPIO2[ loop ];
    }
    else if ( layout == 3 )
    {
        for ( loop = 0; loop < sizeof( headPins3 ) / sizeof( int ); loop++ )
            if ( piPin == headPins3[ loop ]) retCode = bcomGPIO3[ loop ];
    }
    return retCode;
}

// ****************************************************************************
//  Returns wiringPi number from Pi header pin number.
// ****************************************************************************

static int piInfoGetWPiHead( unsigned int piPin )
{

    unsigned int headPins1[] = { HEAD_PINS1 };
    unsigned int headPins2[] = { HEAD_PINS2 };
    unsigned int headPins3[] = { HEAD_PINS3 };
    unsigned int wiPiPins1[] = { WIPI_PINS1 };
    unsigned int wiPiPins2[] = { WIPI_PINS2 };
    unsigned int wiPiPins3[] = { WIPI_PINS3 };

    int layout = piInfoLayout( 0 );
    unsigned int loop;
    int retCode = -1;

    if ( layout == 1 )
    {
        for ( loop = 0; loop < sizeof( headPins1 ) / sizeof( int ); loop++ )
            if ( piPin == headPins1[ loop ]) retCode = wiPiPins1[ loop ];
    }
    else if ( layout == 2 )
    {
        for ( loop = 0; loop < sizeof( headPins2) / sizeof( int ); loop++ )
            if ( piPin == headPins2[ loop ]) retCode = wiPiPins2[ loop ];
    }
    else if ( layout == 3 )
    {
        for ( loop = 0; loop < sizeof( headPins3) / sizeof( int ); loop++ )
            if ( piPin == headPins3[ loop ]) retCode = wiPiPins3[ loop ];
    }
    return retCode;
}

// ****************************************************************************
//  Returns wiringPi number from GPIO number.
// ****************************************************************************

static int piInfoGetWPiGPIO( unsigned int piGPIO )
{
    unsigned int bcomGPIO1[] = { BCOM_GPIO1 };
    unsigned int bcomGPIO2[] = { BCOM_GPIO2 };
    unsigned int bcomGPIO3[] = { BCOM_GPIO3 };
    unsigned int wiPiPins1[] = { WIPI_PINS1 };
    unsigned int wiPiPins2[] = { WIPI_PINS2 };
    unsigned int wiPiPins3[] = { WIPI_PINS3 };

    int layout = piInfoLayout( 0 );
    unsigned int loop;
    int retCode = -1;

    if ( layout == 1 )
    {
        for ( loop = 0; loop < sizeof( bcomGPIO1 ) / sizeof( int ); loop++ )
            if ( piGPIO == bcomGPIO1[ loop ]) retCode = wiPiPins1[ loop ];
    }
    else if ( layout == 2 )
    {
        for ( loop = 0; loop < sizeof( bcomGPIO2) / sizeof( int ); loop++ )
            if ( piGPIO == bcomGPIO2[ loop ]) retCode = wiPiPins2[ loop ];
    }
    else if ( layout == 3 )
    {
        for ( loop = 0; loop < sizeof( bcomGPIO3) / sizeof( int ); loop++ )
            if ( piGPIO == bcomGPIO3[ loop ]) retCode = wiPiPins3[ loop ];
    }
    return retCode;
}
// ****************************************************************************
//  ALSA functions.
// ****************************************************************************

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
    if ( factor == 1 ) mappedVol = volume;
    else mappedVol = lroundf((( pow( factor, (float) volume / range ) - 1 ) /
                             ( factor - 1 ) *
                               range + min ));
    return mappedVol;
};


// ****************************************************************************
//  List ALSA controls for all available cards.
// ****************************************************************************
static void listAllControls()
{
    snd_ctl_card_info_t *ctlCard;       // Simple control card info container.
    snd_ctl_elem_value_t *ctlControl;   // Simple control element value.
    snd_ctl_elem_id_t *ctlId;           // Simple control element id.
    snd_ctl_elem_info_t *ctlInfo;       // Simple control element info container.
    snd_ctl_t *ctlHandle;               // Simple control handle.
    snd_ctl_elem_type_t ctlType;        // Simple control element type.

    snd_hctl_t *hctlHandle;             // High level control handle;
    snd_hctl_elem_t *hctlElem;          // High level control element handle.

    int cardNumber = -1;
    int errNum;
    char *controlType;
    char deviceID[8];


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
        snd_card_next( &cardNumber );
        if ( cardNumber < 0 ) break;

        // Concatenate strings to get card's control interface.
        sprintf( deviceID, "hw:%i", cardNumber );

        //  Try to open card.
        snd_ctl_open( &ctlHandle, deviceID, 0 );
        snd_ctl_card_info( ctlHandle, ctlCard );


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
        snd_hctl_open( &hctlHandle, deviceID, 0 );
        snd_hctl_load( hctlHandle );


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
            snd_hctl_elem_info( hctlElem, ctlInfo );
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

    return;
}

// ****************************************************************************
//  Lists ALSA mixers for all available cards.
// ****************************************************************************
static void listAllMixers()
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
        // Find next card. Exit loop if none.
        snd_card_next( &cardNumber );
        if ( cardNumber < 0 ) break;

        sprintf( deviceID, "hw:%i", cardNumber );

        // Open card.
        snd_ctl_open( &ctlHandle, deviceID, 0 );
        snd_ctl_card_info_alloca( &ctlCard );
        snd_ctl_card_info( ctlHandle, ctlCard );


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
        snd_mixer_open( &mixerHandle, 0 );
        snd_mixer_attach( mixerHandle, deviceID );
        snd_mixer_selem_register( mixerHandle, NULL, NULL );
        snd_mixer_load( mixerHandle );
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

    return;
}

// ****************************************************************************
//  Prints info for selected mixer.
// ****************************************************************************
static void mixerInfo( char *card, char *mixer )
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

    int errNum;
    int body = 1;


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
    //  Print information for selected mixer.
    // ************************************************************************
    long volMin, volMax;
    long dBMin, dBMax;
    long voldBL, voldBR;
    int switchL, switchR;

    printf( "Mixer element name = %s.\n",
        snd_mixer_selem_get_name( mixerElem ));
    printf( "Mixer element index = %i.\n",
        snd_mixer_selem_get_index( mixerElem ));
    printf( "Mixer element ID name = %s.\n",
        snd_mixer_selem_id_get_name( mixerId ));
    printf( "Mixer element ID index = %i.\n",
        snd_mixer_selem_id_get_index( mixerId ));
    if ( snd_mixer_selem_is_active )
        printf( "Mixer is active.\n" );
    else
        printf( "Mixer is inactive.\n" );

        snd_mixer_selem_get_playback_volume_range( mixerElem,
                &volMin, &volMax );
        snd_mixer_selem_get_playback_dB_range( mixerElem, &dBMin, &dBMax );
        printf( "Minimum volume = %d (%ddB).\n", volMin, dBMin );
        printf( "Maximum volume = %d (%ddB).\n", volMax, dBMax );

    if ( snd_mixer_selem_has_playback_channel( mixerElem,
                    SND_MIXER_SCHN_FRONT_LEFT ) &&
         snd_mixer_selem_has_playback_channel( mixerElem,
                    SND_MIXER_SCHN_FRONT_RIGHT ))
    {
        printf( "Mixer has stereo channels.\n" );

        snd_mixer_selem_get_playback_dB( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, &voldBL );
        snd_mixer_selem_get_playback_dB( mixerElem,
                SND_MIXER_SCHN_FRONT_RIGHT, &voldBR );
        printf( "Playback volume = L%idB, R%idB.\n", voldBL, voldBR );

        snd_mixer_selem_get_playback_switch( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, &switchL );
        snd_mixer_selem_get_playback_switch( mixerElem,
                SND_MIXER_SCHN_FRONT_RIGHT, &switchR );
        printf( "Playback switch controls are %i (L) and %i (R).\n",
                switchL, switchR );
        printf( "\n" );
    }
    else
    {
        printf( "Mixer is mono.\n" );

        snd_mixer_selem_get_playback_dB( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, &voldBL );
        printf( "Playback volume = %idB.\n", voldBL );

        snd_mixer_selem_get_playback_switch( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, &switchL );
        printf( "Playback switch controls is %i.\n", switchL );
        printf( "\n" );

    }

    // ************************************************************************
    //  Clean up.
    // ************************************************************************
    snd_mixer_detach( mixerHandle, card );
    snd_mixer_close( mixerHandle );

    return;
}


// ****************************************************************************
//  Set volume using ALSA mixers.
// ****************************************************************************
static void setVolMixer( char *card, char *mixer,
                         unsigned int volume,
                         int balance,
                         float factor )
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

    int errNum;
    long volLinear, volMapped;
    long volMappedL, volMappedR;
    long volLeft, volRight;
    long volMin, volMax;


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


    // ************************************************************************
    //  Calculate volume and check bounds.
    // ************************************************************************
    volLinear = ((( float ) volume / 100 ) * ( volMax - volMin ) + volMin );
    volMapped = mappedVolume( volLinear, volMax - volMin, volMin, factor );

    volLeft = volRight = volLinear;
    if ( balance < 0 )
        volLeft = volLinear + balance * volLinear / 100;
    else
    if ( balance > 0 )
        volRight = volLinear - balance * volLinear / 100;

    volMappedL = mappedVolume( volLeft, volMax - volMin, volMin, factor );
    volMappedR = mappedVolume( volRight, volMax - volMin, volMin, factor );
    printf( "Volume = %d, L = %d, R = %d, M = %d.\n",
                volLinear, volMappedL, volMappedR, volMapped );

    // ************************************************************************
    //  Set volume.
    // ************************************************************************
        // If control is mono then FL will set volume.
        snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, volMappedL );
        snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_RIGHT, volMappedR );


    // ************************************************************************
    //  Clean up.
    // ************************************************************************
    snd_mixer_detach( mixerHandle, card );
    snd_mixer_close( mixerHandle );

    return;
}


// ****************************************************************************
//  GPIO interrupt functions.
// ****************************************************************************
/*
      Quadrature encoding for rotary encoder:

          :   :   :   :   :   :   :   :   :
          :   +-------+   :   +-------+   :         +---+-------+-------+
          :   |   :   |   :   |   :   |   :         | P |  +ve  |  -ve  |
      A   :   |   :   |   :   |   :   |   :         | h +---+---+---+---+
      --------+   :   +-------+   :   +-------      | a | A | B | A | B |
          :   :   :   :   :   :   :   :   :         +---+---+---+---+---+
          :   :   :   :   :   :   :   :   :         | 1 | 0 | 0 | 1 | 0 |
          +-------+   :   +-------+   :   +---      | 2 | 0 | 1 | 1 | 1 |
          |   :   |   :   |   :   |   :   |         | 3 | 1 | 1 | 0 | 1 |
      B   |   :   |   :   |   :   |   :   |         | 4 | 1 | 0 | 0 | 0 |
      ----+   :   +-------+   :   +-------+         +---+---+---+---+---+
          :   :   :   :   :   :   :   :   :
        1 : 2 : 3 : 4 : 1 : 2 : 3 : 4 : 1 : 2   <- Phase
          :   :   :   :   :   :   :   :   :
*/

// Encoder states.
static volatile int volDirection;
static volatile int lastVolEncoded;
static volatile int volEncoded;
static volatile int busyVol = false;
static volatile int balDirection;
static volatile int lastBalEncoded;
static volatile int balEncoded;
static volatile int busyBal = false;

// Button states.
static volatile int muteState = false;
static volatile int muteStateChanged = false;
static volatile int balanceStateSet = false;

// ****************************************************************************
//  Rotary encoder for volume.
// ****************************************************************************

static void encoderPulseVol()
{
    // Check if last interrupt has finished being processed.
    if (( busyVol ) || ( busyBal )) return;
    busyVol = true;

    // Read values at pins.
    unsigned int pinA = digitalRead( cmdArgs.wPiPinA );
    unsigned int pinB = digitalRead( cmdArgs.wPiPinB );

    // Combine A and B with last readings using bitwise operators.
    int volEncoded = ( pinA << 1 ) | pinB;
    int sum = ( lastVolEncoded << 2 ) | volEncoded;

    if ( sum == 0b0001 ||
         sum == 0b0111 ||
         sum == 0b1110 ||
         sum == 0b1000 )
         volDirection = 1;
    else
    if ( sum == 0b1011 ||
         sum == 0b1101 ||
         sum == 0b0100 ||
         sum == 0b0100 )
         volDirection = -1;

    lastVolEncoded = volEncoded;
    busyVol = false;
};


// ****************************************************************************
//  GPIO activity call for mute button.
// ****************************************************************************

static void buttonMute()
{
    int buttonRead = digitalRead( cmdArgs.wPiPinC );
    if ( buttonRead ) muteStateChanged = !muteStateChanged;
};


// ****************************************************************************
//  Information functions.
// ****************************************************************************

// ****************************************************************************
//  Prints informational output.
// ****************************************************************************

static void printOutput( struct volStruct *volParams, int header )
{
    float linearP; // Percentage of hard volume range (linear).
    float shapedP; // Percentage of hard volume range (shaped).

    linearP = 100 - ( volParams->hardMax -
                      volParams->linearVol ) * 100 /
                      volParams->hardRange;
    shapedP = 100 - ( volParams->hardMax -
                      volParams->shapedVol ) * 100 /
                      volParams->hardRange;

    if ( header == 1 )
    {
        printf( "\n\tHardware volume range = %ld to %ld\n",
                volParams->hardMin,
                volParams->hardMax );
        printf( "\tSoft volume range =     %ld to %ld\n\n",
                volParams->softMin,
                volParams->softMax );

        printf( "\t+-------+------------+-----+------------+-----+\n" );
        printf( "\t| Index | Left |  %%  | Right |  %%  |\n" );
        printf( "\t+-------+------------+-----+------------+-----+\n" );
        printf( "\t| %4d |", volParams->index );
    }
    else
    {
        if ( volParams->muteState ) printf( "\t| MUTE |" );
        else printf( "\t| %4d |", volParams->index );
    }
    printf( " %10d | %3d | %10d | %3d |\n",
            volParams->linearVol,
            lroundf( linearP ),
            volParams->shapedVol,
            lroundf( shapedP ));
};


// ****************************************************************************
//  Prints default or command line set parameters values.
// ****************************************************************************

static void printParams ( struct structArgs *printList, int defSet )
{
    if ( defSet == 0 ) printf( "\nDefault parameters:\n\n" );
    else printf ( "\nSet parameters:\n\n" );

    printf( "\tCard name = %s.\n", printList->card );
    printf( "\tMixer name = %s.\n", printList->mixer );
    printf( "\tVolume encoder attached to GPIO pins %i", printList->gpioA );
    printf( " & %i,\n", printList->gpioB );
    printf( "\tMapped to WiringPi pin numbers %i", printList->wPiPinA );
    printf( " & %i.\n", printList->wPiPinB );
    printf( "\tMute button attached to GPIO pin %i.\n", printList->gpioC );
    printf( "\tMapped to WiringPi pin number %i.\n", printList->wPiPinC );
    printf( "\tBalance encoder attached to GPIO pins %i", printList->gpioD );
    printf( " & %i,\n", printList->gpioE );
    printf( "\tMapped to WiringPi pin numbers %i", printList->wPiPinD );
    printf( " & %i.\n", printList->wPiPinE );
    printf( "\tBalance reset button attached to GPIO pin %d.\n", printList->gpioF );
    printf( "\tMapped to WiringPi pin number %i.\n", printList->wPiPinF );
    printf( "\tInitial volume = %i%%.\n", printList->initVol );
    printf( "\tInitial balance = %i%%.\n", printList->balance );
    printf( "\tMinimum volume = %i%%.\n", printList->minVol );
    printf( "\tMaximum volume = %i%%.\n", printList->maxVol );
    printf( "\tVolume increments = %i.\n", printList->incVol );
    printf( "\tVolume factor = %g.\n", printList->facVol );
    printf( "\tTic delay = %i.\n\n", printList->ticDelay );
};


// ****************************************************************************
//  Prints command line parameter ranges.
// ****************************************************************************

static void printRanges( struct boundsStruct *paramBounds )
{
    printf ( "\nCommand line parameter ranges:\n\n" );
    printf ( "\tVolume range (-i) = %d to %d.\n",
                    paramBounds->volumeLow,
                    paramBounds->volumeHigh );
    printf ( "\tBalance  range (-b) = %i%% to %i%%.\n",
                    paramBounds->balanceLow,
                    paramBounds->balanceHigh );
    printf ( "\tVolume shaping factor (-f) = %g to %g.\n",
                    paramBounds->factorLow,
                    paramBounds->factorHigh );
    printf ( "\tIncrement range (-e) = %d to %d.\n",
                    paramBounds->incLow,
                    paramBounds->incHigh );
    printf ( "\tDelay range (-d) = %d to %d.\n\n",
                    paramBounds->delayLow,
                    paramBounds->delayHigh );
};


// ****************************************************************************
//  Command line argument functions.
// ****************************************************************************

// ****************************************************************************
//  argp documentation.
// ****************************************************************************
const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi rotary encoder control.";
static const char args_doc[] = "piRotEnc <options>";


// ****************************************************************************
//  Command line argument definitions.
// ****************************************************************************
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "ALSA:" },
    { "card", 'c', "String", 0, "ALSA card name" },
    { "mixer", 'd', "String", 0, "ALSA mixer name" },
    { 0, 0, 0, 0, "GPIO:" },
    { "gpio1", 'A', "Integer", 0, "GPIO pin 1 for encoder." },
    { "gpio2", 'B', "Integer", 0, "GPIO pin 2 for encoder." },
    { "gpio3", 'C', "Integer", 0, "GPIO pin for mute." },
    { 0, 0, 0, 0, "Volume:" },
    { "init", 'i', "Integer", 0, "Initial volume (% of Maximum)." },
    { "min", 'j', "Integer", 0, "Minimum volume (% of full output)." },
    { "max", 'k', "Integer", 0, "Maximum volume (% of full output)." },
    { "inc", 'l', "Integer", 0, "Volume increments over range." },
    { "fac", 'f', "Float", 0, "Volume profile factor." },
    { "bal", 'b', "Integer", 0, "L/R balance -100% to +100%." },
    { 0, 0, 0, 0, "Responsiveness:" },
    { "delay", 't', "Integer", 0, "Delay between encoder tics (mS)." },
    { 0, 0, 0, 0, "Debugging:" },
    { "gpiomap", 'm', 0, 0, "Print GPIO/wiringPi map." },
    { "output", 'p', 0, 0, "Print program output." },
    { "default", 'q', 0, 0, "Print default parameters." },
    { "ranges", 'r', 0, 0, "Print parameter ranges." },
    { "params", 's', 0, 0, "Print set parameters." },
    { 0 }
};


// ****************************************************************************
//  Command line argument parser.
// ****************************************************************************
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    struct structArgs *cmdArgs = state->input;
    switch ( param )
    {
        case 'c' :
            cmdArgs->card = arg;
            break;
        case 'd' :
            cmdArgs->control = arg;
            break;
        case 'A' :
           cmdArgs->gpioA = atoi( arg );
            break;
        case 'B' :
           cmdArgs->gpioB = atoi( arg );
            break;
        case 'C' :
            cmdArgs->gpioC = atoi( arg );
            break;
        case 'f' :
            cmdArgs->facVol = atof( arg );
            break;
        case 'b' :
            cmdArgs->balance = atoi( arg );
            break;
        case 'i' :
            cmdArgs->initVol = atoi( arg );
            break;
        case 'j' :
            cmdArgs->minVol = atoi( arg );
            break;
        case 'k' :
            cmdArgs->maxVol = atoi( arg );
            break;
        case 'l' :
            cmdArgs->incVol = atoi( arg );
            break;
        case 't' :
            cmdArgs->ticDelay = atoi( arg );
            break;
        case 'm' :
            cmdArgs->prMapping = 1;
            break;
        case 'p' :
            cmdArgs->prOutput = 1;
            break;
        case 'q' :
            cmdArgs->prDefaults = 1;
            break;
        case 'r' :
            cmdArgs->prRanges = 1;
            break;
        case 's' :
            cmdArgs->prSet = 1;
            break;
    }
    return 0;
};


// ****************************************************************************
//  argp parser parameter structure.
// ****************************************************************************
static struct argp argp = { options, parse_opt, args_doc, doc };


// ****************************************************************************
//  Checking functions.
// ****************************************************************************

// ****************************************************************************
//  Checks if a parameter is within bounds.
// ****************************************************************************
static int checkIfInBounds( float value, float lower, float upper )
{
        if (( value < lower ) || ( value > upper )) return 1;
        else return 0;
};

// ****************************************************************************
//  Command line parameter validity checks.
// ****************************************************************************
static int checkParams ( struct structArgs *cmdArgs,
                         struct boundsStruct *paramBounds )
{
    static int error = 0;
    static int pinA, pinB;

    pinA = piInfoGetWPiGPIO( cmdArgs->gpioA );
    pinB = piInfoGetWPiGPIO( cmdArgs->gpioB );

    if (( pinA == -1 ) || ( pinB == -1 )) error = 1;
    else
    {
        cmdArgs->wPiPinA = pinA;
        cmdArgs->wPiPinB = pinB;
    }
    error = ( error ||
              checkIfInBounds( cmdArgs->initVol,
                               cmdArgs->minVol,
                               cmdArgs->maxVol ) ||
              checkIfInBounds( cmdArgs->balance,
                               paramBounds->balanceLow,
                               paramBounds->balanceHigh ) ||
              checkIfInBounds( cmdArgs->initVol,
                               paramBounds->volumeLow,
                               paramBounds->volumeHigh ) ||
              checkIfInBounds( cmdArgs->minVol,
                               paramBounds->volumeLow,
                               paramBounds->volumeHigh ) ||
              checkIfInBounds( cmdArgs->maxVol,
                               paramBounds->volumeLow,
                               paramBounds->volumeHigh ) ||
              checkIfInBounds( cmdArgs->incVol,
                               paramBounds->incLow,
                               paramBounds->incHigh ) ||
              checkIfInBounds( cmdArgs->facVol,
                               paramBounds->factorLow,
                               paramBounds->factorHigh ) ||
              checkIfInBounds( cmdArgs->ticDelay,
                               paramBounds->delayLow,
                               paramBounds->delayHigh ));
    if ( error )
    {
        printf( "\nThere is something wrong with the set parameters.\n" );
        printf( "Use the -m -p -q -r -s flags to check values.\n\n" );
    }
    return error;
};


// ****************************************************************************
//  Main section.
// ****************************************************************************
int main( int argc, char *argv[] )
{
    static int volPos = 0;  // Starting encoder position.
    static int balPos = 0;  // Starting encoder position.
    static int error = 0;   // Error trapping flag.
    static struct volStruct volParams; // Data structure for volume.
    static unsigned int volumeIndex;
    static unsigned int volumeIncrement;
    cmdArgs = defaultArgs;  // Set command line arguments to default.


    // ************************************************************************
    //  Get command line arguments and check within bounds.
    // ************************************************************************
    argp_parse( &argp, argc, argv, 0, 0, &cmdArgs );
    error = checkParams( &cmdArgs, &paramBounds );
    if ( error ) return 0;


    // ************************************************************************
    //  Print out any information requested on command line.
    // ************************************************************************
    if ( cmdArgs.prMapping ) piInfoLayout( 1 );
    if ( cmdArgs.prRanges ) printRanges( &paramBounds );
    if ( cmdArgs.prDefaults ) printParams( &defaultArgs, 0 );
    if ( cmdArgs.prSet ) printParams ( &cmdArgs, 1 );

    // exit program for all information except program output.
    if (( cmdArgs.prMapping ) ||
        ( cmdArgs.prDefaults ) ||
        ( cmdArgs.prSet ) ||
        ( cmdArgs.prRanges )) return 0;


    // ************************************************************************
    //  Initialise wiringPi.
    // ************************************************************************
    wiringPiSetup ();

    // Set encoder pin mode.
    pinMode( cmdArgs.wPiPinA, INPUT );
    pullUpDnControl( cmdArgs.wPiPinA, PUD_UP );
    pinMode( cmdArgs.wPiPinB, INPUT );
    pullUpDnControl( cmdArgs.wPiPinB, PUD_UP );

    // Set mute button pin mode.
    pinMode( cmdArgs.wPiPinC, INPUT );
    pullUpDnControl( cmdArgs.wPiPinC, PUD_UP );

    // Initialise encoder direction.
    volDirection = 0;


    // ************************************************************************
    //  Set up volume data structure.
    // ************************************************************************
    volParams.card = cmdArgs.card;
    volParams.mixer = cmdArgs.mixer;
    volParams.factor = cmdArgs.facVol;
    volParams.balance = 0;
    volParams.mute = 0;
    volParams.debug = cmdArgs.prOutput;

    // Get closest volume index for starting volume.
    volumeIndex = ( cmdArgs->incVol -
                  ( cmdArgs->maxVol - cmdArgs->initVol ) *
                    cmdArgs->incVol /
                  ( cmdArgs->maxVol - cmdArgs->minVol ));
    // Calculate actual starting volume.
    volParams.volume = index * volumeIncrement;


    // ************************************************************************
    //  Set initial volume.
    // ************************************************************************
    setVolMixer( volParams.card,
                 volParams.control,
                 volParams.volume,
                 volParams.balance,
                 volParams.factor );


    // ************************************************************************
    //  Register interrupt functions.
    // ************************************************************************
    wiringPiISR( cmdArgs.wPiPinA, INT_EDGE_BOTH, &encoderPulseVol );
    wiringPiISR( cmdArgs.wPiPinB, INT_EDGE_BOTH, &encoderPulseVol );
    wiringPiISR( cmdArgs.wPiPinC, INT_EDGE_BOTH, &buttonMute );


    // ************************************************************************
    //  Wait for GPIO activity.
    // ************************************************************************
    while ( 1 )
    {
        if (( volDirection != 0 ) && ( !volParams.muteState ))
        {
            // Volume +.
            if ( volDirection > 0 )
            {
                volParams.volume = volParams.volume + 100 / volParams.increments;
                if ( volParams.volume > volParams.min )
                     volParams.volume = volParams.min;
            }
            // Volume -.
            else
            if ( volDirection < 0 )
            {
                volParams.index--;
                if ( volParams.index < 0 ) volParams.index = 0;
            }
            volDirection = 0;


            // ****************************************************************
            //  Calculate and set volume.
            // ****************************************************************
            setVolMixer( volParams.card,
                         volParams.control,
                         volParams.volume,
                         volParams.balance,
                         volParams.factor );

        }

        // ********************************************************************
        //  Check and set mute state.
        // ********************************************************************
        {
        }

        // Tic delay in mS. Adjust according to encoder.
        delay( cmdArgs.ticDelay );
    }
    // Close sockets.
    snd_ctl_close( ctl );

    return 0;
}
