/*******************************************************************************

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

******************************************************************************/

#define Version "Version 0.1"

/*
//  Authors:    D.Faulke    19/10/15
//
//  Changelog:
//
//  v0.1 Initial version.
*/

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

// Can't be sure future revisions will be hex so store as strings.
#define REVISION    0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,\
                    0x0008, 0x0009, 0x0010, 0x0012, 0x0013, 0x000d,\
                    0x000e, 0x000f, 0xa01041, 0xa21041
#define MODEL       "B", "B", "B", "B", "B", "A",\
                    "A", "A", "B+", "A+", "B+", "B",\
                    "B", "B", "2B", "2B"
#define VERSION     1.0, 1.0, 2.0, 2.0, 2.0, 2.0,\
                    2.0, 2.0, 1.0, 1.0, 1.2, 2.0,\
                    2.0, 2.0, 1.1, 1.1
#define LAYOUT      1, 1, 2, 2, 2, 2,\
                    2, 2, 3, 3, 3, 2,\
                    2, 2, 3, 3

//*****************************************************************************
//  Main.
//*****************************************************************************

int main ( void )
{
    int revision[] = {REVISION};
    char *model[] = {MODEL};
    float version[] = {VERSION};
    int layout[] = {LAYOUT};

    FILE *pFile;        // File to read in and search.
    char line[512];     // Storage for each line read in.
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
    printf( "Revision = 0x%0.6x.\n", rev );
    fclose ( pFile ) ;
    printf( "\t+-------+----------+-------+---------+--------+\n" );
    printf( "\t| Index | Revision | Model | Version | Layout |\n" );
    printf( "\t+-------+----------+-------+---------+--------+\n" );
    // Now compare against arrays.
    index = 0;
    for ( loop = 0; loop < sizeof( revision )  / sizeof( int ); loop++ )
    {
        printf( "\t|    %2i | 0x%0.6x |   %-2s  |   %3.1f   |    %1i   |",
                loop,
                revision[ loop ],
                model[ loop ],
                version[ loop ],
                layout[ loop ]);
        if ( rev == revision[ loop ] )
        {
            index = loop;
            printf( "<-\n" );
        }
        else printf ( "\n" );
    }
    printf( "\t+-------+----------+-------+---------+--------+\n\n" );
    printf( "Revision = 0x%0.6x.\n", revision[ index ]);
    printf( "Model = %s.\n", model[ index ]);
    printf( "Version = %3.1f.\n", version[ index ]);
    printf( "GPIO layout = %1i.\n\n", layout[ index ]);

    return 0;
}
