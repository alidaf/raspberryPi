// ****************************************************************************
// ****************************************************************************
/*
    piPinLayout:

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

#define Version "Version 0.2"

//  Compilation:
//
//  Compile with gcc piPinLayout.c -o piPinLayout
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:    D.Faulke    23/10/15
//
//  Changelog:
//
//  v0.1 Initial version.
//  v0.2 Added printout of GPIO header pins.

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

// GPIO pin layouts - preformatted to make the printout routine easier.
#define PINS1  " +3.3V", "+5V   ", " GPIO0", "+5V   ", " GPIO1", "GND   ",\
               " GPIO4", "GPIO14",    "GND", "GPIO15", "GPIO17", "GPIO18",\
               "GPIO21", "GND   ", "GPIO22", "GPIO23", "  3.3V", "GPIO24",\
               "GPIO10", "GND   ", " GPIO9", "GPIO25", "GPIO11", "GPIO8 ",\
               "   GND", "GPIO7 "
#define PINS2  " +3.3V", "+5V   ", " GPIO2", "+5V   ", " GPIO3", "GND   ",\
               " GPIO4", "GPIO14", "   GND", "GPIO15", "GPIO17", "GPIO18",\
               "GPIO21", "GND   ", "GPIO22", "GPIO23",   "3.3V", "GPIO24",\
               "GPIO10", "GND   ", " GPIO9", "GPIO25", "GPIO11", "GPIO8 ",\
               "   GND", "GPIO7 "
#define PINS3  " +3.3V", "+5V   ", " GPIO2", "+5V   ", " GPIO3", "GND   ",\
               " GPIO4", "GPIO14", "   GND", "GPIO15", "GPIO17", "GPIO18",\
               "GPIO21", "GND   ", "GPIO22", "GPIO23", " +3.3V", "GPIO24",\
               "GPIO10", "GND   ", " GPIO9", "GPIO25", "GPIO11", "GPIO8 ",\
               "   GND", "GPIO7 ", "   DNC", "DNC   ", " GPIO5", "GND   ",\
               " GPIO6", "GPIO12", "GPIO13", "GND   ", "GPIO19", "GPIO16",\
               "GPIO26", "GPIO20", "   GND", "GPIO21"

// ****************************************************************************
//  Main.
// ****************************************************************************

int main ( void )
{
    int revision[] = { REVISION };
    char *model[] = { MODEL };
    float version[] = { VERSION };
    int layout[] = { LAYOUT };

    char *pins1[] = { PINS1 };
    char *pins2[] = { PINS2 };
    char *pins3[] = { PINS3 };

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
    printf( "\nKnown revisions:\n\n", rev );
    fclose ( pFile ) ;
    printf( "\t+-------+----------+-------+---------+\n" );
    printf( "\t| Index | Revision | Model | Version |\n" );
    printf( "\t+-------+----------+-------+---------+\n" );

    // Now compare against arrays to find info.
    index = 0;
    for ( loop = 0; loop < sizeof( revision )  / sizeof( int ); loop++ )
    {
        printf( "\t|    %2i | 0x%0.6x |   %-2s  |   %3.1f   |",
                loop,
                revision[ loop ],
                model[ loop ],
                version[ loop ]);
        if ( rev == revision[ loop ] )
        {
            index = loop;
            printf( "<-This Pi\n" );
        }
        else printf ( "\n" );
    }

    // Print GPIO header layout.
    printf( "\t+-------+----------+-------+---------+\n\n" );
    if ( index == 0 )
        printf( "Cannot find your card version from known versions.\n\n" );
    else
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
                for ( loop = 0; loop < sizeof( pins1 ) /
                            ( 4 * sizeof( char )); loop = loop + 2 )
                    printf( "\t| %6s | %3i || %3i | %6s |\n",
                         pins1[ loop ], loop + 1, loop + 2, pins1[ loop + 1 ]);
                break;
            case 2 :
                for ( loop = 0; loop < sizeof( pins2 ) /
                            ( 4 * sizeof( char )); loop = loop + 2 )
                    printf( "\t| %6s | %3i || %3i | %6s |\n",
                         pins2[ loop ], loop + 1, loop + 2, pins2[ loop + 1 ]);
                break;
            case 3 :
                for ( loop = 0; loop < sizeof( pins3 ) /
                            ( 4 * sizeof( char )); loop = loop + 2 )
                    printf( "\t| %6s | %3i || %3i | %6s |\n",
                         pins3[ loop ], loop + 1, loop + 2, pins3[ loop + 1 ]);
                break;
        }
        printf( "\t+--------+-----++-----+--------+\n\n" );
    }

    return 0;
}
