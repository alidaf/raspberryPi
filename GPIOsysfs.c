// ****************************************************************************
// ****************************************************************************
/*
    GPIOsysfs:

    Library to provide basic GPIO functions using sysfs.

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
//  Compile with gcc GPIOsysfs.c -o GPIOsysfs
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	23/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.

#define MAXSTR 64

// Pins and GPIO numbers for Raspberry Pi B (Rev 1).
#define VALID_PINS_B1    3,  5,  7,  8, 10, 11, 12, 13, 15, 16,\
                        18, 19, 21, 22, 23, 24, 26
#define VALID_GPIO_B1    0,  1,  4, 14, 15, 17, 18, 21, 22, 23,\
                        24, 10, 19, 25, 11,  8,  7
// Pins and GPIO numbers for Raspberry Pi A (Rev 2) and B (Rev 2).
#define VALID_PINS_AB2   3,  5,  7,  8, 10, 11, 12, 13, 15, 16,\
                        18, 19, 21, 22, 23, 24, 26
#define VALID_GPIO_AB2   2,  3,  4, 14, 15, 17, 18, 21, 22, 23,\
                        24, 10, 19, 25, 11,  8,  7
// Pins and GPIO numbers for Raspberry Pi A+, B+ & 2.
#define VALID_PINS_2AB   3,  5,  7,  8, 10, 11, 12, 13, 15, 16,\
                        18, 19, 21, 22, 23, 24, 26, 29, 31, 32,\
                        33, 35, 36, 37, 38, 40
#define VALID_GPIO_2AB   2,  3,  4, 14, 15, 17, 18, 21, 22, 23,\
                        24, 10, 19, 25, 11,  8,  7,  5,  6, 12,\
                        13, 19, 16, 26, 20, 21

#define IN   0
#define OUT  1

#define LOW  0
#define HIGH 1

#include <stdio.h>
#include "gpio.h"

// ****************************************************************************
//  Returns Raspberry Pi board revision.
// ****************************************************************************

int get_pi_rev( void )
{
    FILE *cpuFd;
    char line [120];
    char *c, lastChar;
    static int boardRev = -1;

    if ( boardRev != -1 )	// No point checking twice
        return boardRev;

    if (( cpuFd = fopen( "/proc/cpuinfo", "r" )) == NULL )
        fprintf( stderr, "Unable to open /proc/cpuinfo" );

    while ( fgets( line, 120, cpuFd ) != NULL )
        if ( strncmp( line, "Revision", 8 ) == 0 )
            break ;

    fclose( cpuFd ) ;

    if ( strncmp( line, "Revision", 8 ) != 0 )
        fprintf( stderr, "No revision line." ) ;

    for ( c = &line[ strlen( line ) - 1 ];
        ( *c == '\n' ) || ( *c == '\r' );
          --c ) *c = 0;

        for ( c = line; *c; ++c )
            if ( isdigit( *c ))
                break;

    if ( !isdigit( *c ))
    fprintf( stderr, "No numeric revision string." );

    lastChar = line [ strlen( line ) - 1 ] ;

    if ((lastChar == '2') || (lastChar == '3'))
        boardRev = 1;
    else
        boardRev = 2;

    return boardRev;
}




// ****************************************************************************
//  Returns Broadcom GPIO number from Raspberry Pi pin number.
// ****************************************************************************

int get_GPIO_num( int pin, int board )


{


}

int setGPIO_out(int pin)
{
    /* This is a function to export a GPIO pin - and set it's direction
       to output... */

    /* Start with a test to see if we built in debug...
       ...if we did, then show some more verbose descriptions of
       what we're doing. */
    #ifdef debug
        printf("Exporting pin %d for output\n", pin);
    #endif

    int valid_pins[] = { VALID_PINS };
    int c;
    int valid = 0;

    /* Before we do anyting else, we need to check to see if the pin number
       passed in is a valid pin or not - we'll compare it against the pins
       defined in the list of valid pins. */
    for ( c=0; c < ( sizeof( valid_pins ) / sizeof( int )); c++ )
    {
        if(pin == valid_pins[c] )
        valid = 1;
    }

    if(!valid)
    {
        fprintf( stderr, "ERROR: Invalid pin!\nPin %d is not a GPIO pin...\n",
                 pin );
        return -1;
    }

    FILE *sysfs_handle = NULL;

    /* Now we define a file handle, for the 'gpio/export' sysfs file & try to
       open it.
       Note that we open for writing, and as a binary file. */
    if (( sysfs_handle = fopen( "/sys/class/gpio/export", "w" )) == NULL )
    {
        fprintf( stderr, "ERROR: Cannot open GPIO export.\n" );
        fprintf( stderr, "Is this program running as root?)\n" );
        return 1;
    }

    /* Next we need to convert our int pin value to a string... The safest way
       is to use snprintf.
       Note that the length is n+1 as C strings are nul-terminated - so for a
       two-digit value - we need to specify 3 chars to be used. */

    char str_pin[3];
    snprintf( str_pin, ( 3 * sizeof( char )), "%d", pin );

    /* To actually export the pin - we simply write the string value of the
       pin number to the sysfs gpio/export file */
    if ( fwrite( &str_pin, sizeof( char ), 3, sysfs_handle ) !=3 )
    {
        fprintf( stderr, "ERROR: Unable to export GPIO pin %d\n", pin );
        return 2;
    }

    fclose( sysfs_handle );

    /* If we got to here, then we've been able to export the pin - so now we
       need to set the direction. */
    /* We open the direction file for the pin...*/

    char str_direction_file[MAXSTR];
    snprintf( str_direction_file, ( MAXSTR * sizeof( char )),
              "/sys/class/gpio/gpio%d/direction", pin );
    if (( sysfs_handle = fopen( str_direction_file, "w" )) == NULL )
    {
        fprintf( stderr, "ERROR: Cannot open direction file...\n" );
        return 3;
    }
    /* ...and then we'll write "out" to the direction file.*/
    if ( fwrite( "out", sizeof( char ), 4, sysfs_handle ) != 4 )
    {
        fprintf( stderr, "ERROR: Unable to write direction for GPIO%d\n", pin );
        return 4;
    }
    fclose( sysfs_handle );
    // If everything worked, we'll return 0 - an non-zero return value signifies something went wrong
    return 0;
}

int GPIO_Write( int pin, int value )
{
    /* This function will write a value (0 or 1) to the selected GPIO pin */
    /* Once again, if we built in debug mode - then show verbose output*/
    #ifdef debug
        printf( "Writing value %d, to GPIO pin %d\n", value, pin );
    #endif

    /* The first thing that we need to do is check to see if we were passed a valid value */
    if (( value !=0 ) && ( value != 1 ))
    {
        fprintf( stderr, "ERROR: Invalid value!\nValue must be 0 or 1\n" );
        return -1;
    }
    /* Now we're sure we have a god value, we'll try to open the sysfs value file for the pin
     * We'll start by building the path - using snprintf... */
    FILE *sysfs_handle = NULL;
    char str_value_file[MAXSTR];
    snprintf ( str_value_file, ( MAXSTR * sizeof( char )), "/sys/class/gpio/gpio%d/value", pin );
    if (( sysfs_handle = fopen( str_value_file, "w" )) == NULL )
    {
        fprintf( stderr, "ERROR: Cannot open value file for pin %d.\n", pin );
        fprintf( stderr, "Has the pin been exported?\n" );
        return 1;
    }
    /* If the file is good - then we write the string value "0" or "1" to it. */
    char str_val[2];
    snprintf ( str_val, ( 2 * sizeof( char )), "%d", value );
    if ( fwrite( str_val, sizeof( char ), 2, sysfs_handle ) != 2 )
    {
        fprintf( stderr, "ERROR: Cannot write value %d to GPIO pin %d\n",
                        value, pin );
        return 2;
    }
    fclose( sysfs_handle );
    return 0;
}

int unsetGPIO( int pin )
{
    /* This function "turns off" the specified pin - and unexports it. */
     /* If we built in debug - then produce verbose output */
    #ifdef debug
        printf( "Unxporting pin %d\n", pin );
    #endif

    /* We start by building the string for the sysfs value file for the pin - to set it to 0*/
    FILE *sysfs_handle = NULL;
    char str_pin[3];
    char str_value_file[MAXSTR];

    snprintf ( str_pin, ( 3 * sizeof( char )), "%d", pin );
    snprintf ( str_value_file, ( MAXSTR * sizeof( char )),
                    "/sys/class/gpio/gpio%d/value", pin );
    if (( sysfs_handle = fopen( str_value_file, "w" )) == NULL )
    {
        fprintf( stderr, "ERROR: Cannot open value file for pin %d...\n", pin );
        return 1;
    }
    if ( fwrite( "0", sizeof( char ), 2, sysfs_handle ) != 2 )
    {
        fprintf( stderr, "ERROR: Cannot write to GPIO pin %d\n", pin );
        return 2;
    }
        fclose( sysfs_handle );

    /* Once we've done that - the last step is to open the gpio/unexport file - and write the pin
       number to it - to unexport the pin. */
    if (( sysfs_handle = fopen( "/sys/class/gpio/unexport", "w" )) == NULL )
    {
        fprintf( stderr, "ERROR: Cannot open GPIO unexport...\n" );
        return 1;
    }
    if (fwrite( &str_pin, sizeof( char ), 3, sysfs_handle ) !=3 )
    {
        fprintf( stderr, "ERROR: Unable to unexport GPIO pin %d\n", pin );
        return 2;
    }

    fclose( sysfs_handle );
    return 0;
}
