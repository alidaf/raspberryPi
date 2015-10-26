// ****************************************************************************
// ****************************************************************************
/*
    piPins:

    Test program to return the Raspberry Pi GPIO pin layout based on the
    model revision.

    Copyright Darren Faulke 2015.

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

#define Version "Version 0.3"

//  Compilation:
//
//  Compile with gcc -c piInfo.c
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:    D.Faulke    23/10/15
//
//  Changelog:
//
//  v0.1 Initial version.
//  v0.2 Added printout of GPIO header pins.
//  v0.3 Added pin mapping functions.
//

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
    Program will return the '*' column value.
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

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

// Known revision numbers and corresponding models and board versions.
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

// ****************************************************************************
//  Returns GPIO layout by querying /proc/cpuinfo.
// ****************************************************************************

int piInfoLayout( unsigned int prOut )
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

int piInfoGetGPIOHead( unsigned int piPin )
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

int piInfoGetWPiHead( unsigned int piPin )
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

int piInfoGetWPiGPIO( unsigned int piGPIO )
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
