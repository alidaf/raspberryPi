// ****************************************************************************
/*
    piRotEnc:

    Rotary encoder volume control app for the Raspberry Pi.

    Copyright 2015 Darren Faulke <darren@alidaf.co.uk>
        Originally based on IQ_rot, 2015 Gordon Garrity.
            - see https://github.com/iqaudio/tools/IQ_rot.c.
        Rotary encoder state machine based on algorithm by Ben Buxton.
            - see http://www.buxtronix.net.
        ALSA routines based on amixer, 2000 Jaroslev Kysela.
            - see http://www.alsa-project.org.

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

#define piRotEncVersion "Version 0.2"

//  Compilation:
//
//  Compile with gcc piRotEnc.c alsaPi.c rotencPi.c -o piRotEnc
//          -lwiringPi -lasound -lm -lpthread
//  Also use the following flags for Raspberry Pi optimisation:
//          -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//          -ffast-math -pipe -O3

//  Authors:        D.Faulke    11/12/2015
//
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version.
//  v0.2 Rewrite main functions into libraries.
//

//  To Do:
//      Add proper muting rather than set volume to 0, and set mute when 0.
//          - look at playback switch mechanism.
//      Improve bounds checking by using arrays for each parameter.
//      Add routine to check validity of GPIOs.
//      Improve error trapping and return codes for all functions.
//      Write GPIO and interrupt routines to replace wiringPi.
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <argp.h>
#include <alsa/asoundlib.h>
//#include <alsa/mixer.h>
#include <wiringPi.h>
//#include <math.h>
#include <stdbool.h>
//#include <ctype.h>
//#include <stdlib.h>

#include "alsaPi.h"
#include "rotencPi.h"

#define NUM_BOUNDS 2

// Data structures. -----------------------------------------------------------

struct commandStruct            // Command line options.
{
    char        *card;          // ALSA card name.
    char        *mixer;         // Alsa mixer name.
    uint8_t     gpioA;          // GPIO pin for vol rotary encoder.
    uint8_t     gpioB;          // GPIO pin for vol rotary encoder.
    uint8_t     gpioC;          // GPIO pin for function button.
    uint8_t     volume;         // Initial volume (%).
    uint8_t     minimum;        // Minimum soft volume (%).
    uint8_t     maximum;        // Maximum soft volume (%).
    uint8_t     increments;     // Increments over volume range.
    float       factor;         // Volume shaping factor.
    int8_t      balance;        // Volume L/R balance.
    uint16_t    delay;          // Delay between encoder tics.
    bool        printOutput;    // Flag to print output.
    bool        printOptions;   // Flag to print options.
    bool        printRanges;    // Flag to print ranges.
}
    command =                   // Default values.
{
    .card           = "hw:0",   // 1st ALSA card.
    .mixer          = "PCM",    // Default ALSA control.
    .gpioA          = 23,       // GPIO 1 for volume encoder.
    .gpioB          = 24,       // GPIO 2 for volume encoder.
    .gpioC          = 0xFF,     // GPIO for function button - disabled.
    .volume         = 0,        // Start muted.
    .minimum        = 0,        // 0% of Maximum output level.
    .maximum        = 100,      // 100% of Maximum output level.
    .increments     = 20,       // 20 increments from 0 to 100%.
    .factor         = 1,        // Volume change rate factor.
    .balance        = 0,        // L = R.
    .delay          = 1000,     // 1ms between interrupt checks
    .printOutput    = false,    // No output printing.
    .printOptions   = false,    // No command line options printing.
    .printRanges    = false     // No range printing.
};

struct boundsStruct                     // Boundary limits for options.
{
    uint8_t volume  [NUM_BOUNDS];       // Volume.
    int8_t  balance [NUM_BOUNDS];       // Balance.
    float   factor  [NUM_BOUNDS];       // Shaping factor.
    uint8_t incs    [NUM_BOUNDS];       // Increments.
    uint16_t delay  [NUM_BOUNDS];       // Sensitivity delay.
}
    bounds =                            // Set default values.
{
    .volume     =   { 0,     100    },  // 0% to 100%.
    .balance    =   { -100,  100    },  // -100% to +100%.
    .factor     =   { 0.001, 10     },  // 0.001 to 10.
    .incs       =   { 10,    0xFF   },  // UINT8.
    .delay      =   { 1,     0xFFFF }   // UINT16.
};


//  Information functions. ----------------------------------------------------

// ----------------------------------------------------------------------------
//  Prints default or set option values.
// ----------------------------------------------------------------------------
static void printOptions( void )
{
    printf( "\n\t+-----------------+-----------------+\n" );
    printf( "\t| Option          | Value(s)        |\n" );
    printf( "\t+-----------------+-----------------+\n" );
    printf( "\t| Card name       | %-15s |\n", command.card );
    printf( "\t| Mixer name      | %-15s |\n", command.mixer );
    printf( "\t| Encoder         | GPIO%-2i", command.gpioA );
    printf( " & GPIO%-2i |\n", command.gpioB );
    printf( "\t| Function button | GPIO%-11i |\n", command.gpioC );
    printf( "\t| Volume          | %3i%% %10s |\n", command.volume, "" );
    printf( "\t| Increments      | %3i %11s |\n", command.increments, "" );
    printf( "\t| Balance         | %3i%% %10s |\n", command.balance, "" );
    printf( "\t| Minimum         | %3i%% %10s |\n", command.minimum, "" );
    printf( "\t| Maximum         | %3i%% %10s |\n", command.maximum, "" );
    printf( "\t| Factor          | %7.3f %7s |\n", command.factor, "" );
    printf( "\t| Interrupt delay | %3i %11s |\n", command.delay, "" );
    printf( "\t+-----------------+-----------------+\n\n" );
};

// ----------------------------------------------------------------------------
//  Prints option ranges.
// ----------------------------------------------------------------------------
static void printRanges( void )
{
    printf( "\nCommand line option ranges:\n\n" );
    printf( "\t+------------+--------+-------+-------+\n" );
    printf( "\t| Parameter  | Switch |  min  |  max  |\n" );
    printf( "\t+------------+--------+-------+-------+\n" );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Volume", "-v", bounds.volume[0], bounds.volume[1] );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Balance", "-b", bounds.balance[0], bounds.balance[1] );
    printf( "\t| %-10s |   %2s   | %5.3f | %5.2f |\n",
            "Factor", "-f", bounds.factor[0], bounds.factor[1] );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Increments", "-i", bounds.incs[0], bounds.incs[1] );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Response", "-d", bounds.delay[0], bounds.delay[1] );
    printf( "\t+------------+--------+-------+-------+\n\n" );
};


//  Command line option functions. --------------------------------------------

// ----------------------------------------------------------------------------
//  argp documentation.
// ----------------------------------------------------------------------------
const char *argp_program_version = piRotEncVersion;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi rotary encoder volume control.";
static const char args_doc[] = "piRotEnc <options>";

// ----------------------------------------------------------------------------
//  Command line argument definitions.
// ----------------------------------------------------------------------------
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "ALSA:" },
    { "card",      'c', "<string>",    0, "ALSA card name" },
    { "mixer",     'm', "<string>",    0, "ALSA mixer name" },
    { 0, 0, 0, 0, "GPIO:" },
    { "gpiorot",   'A', "<int>,<int>", 0, "GPIOs for rotary encoder." },
    { "gpiobut",   'B', "<int>",       0, "GPIO for function button." },
    { 0, 0, 0, 0, "Volume:" },
    { "vol",       'v', "<int>",       0, "Initial volume (%)." },
    { "bal",       'b', "<int>",       0, "Initial L/R balance (%)." },
    { "min",       'j', "<int>",       0, "Minimum volume (%)." },
    { "max",       'k', "<int>",       0, "Maximum volume (%)." },
    { "inc",       'i', "<int>",       0, "Volume increments." },
    { "fac",       'f', "<float>",     0, "Volume profile factor." },
    { 0, 0, 0, 0, "Responsiveness:" },
    { "delay",     'r', "<int>",       0, "Interrupt delay (mS)." },
    { 0, 0, 0, 0, "Debugging:" },
    { "proutput",  'P',       0,       0, "Print output while running." },
    { "proptions", 'O',       0,       0, "Print all command options." },
    { "prranges",  'R',       0,       0, "Print parameter ranges." },
    { 0 }
};

// ----------------------------------------------------------------------------
//  Command line argument parser.
// ----------------------------------------------------------------------------
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str, *token;
    const char delimiter[] = ",";

    switch ( param )
    {
        case 'c' :
            command.card = arg;
            break;
        case 'm' :
            command.mixer = arg;
            break;
        case 'A' :
            str = arg;
            token = strtok( str, delimiter );
            command.gpioA = atoi( token );
            token = strtok( NULL, delimiter );
            command.gpioB = atoi( token );
            break;
        case 'B' :
            command.gpioC = atoi( arg );
            break;
        case 'v' :
            command.volume = atoi( arg );
            break;
        case 'b' :
            command.balance = atoi( arg );
            break;
        case 'j' :
            command.minimum = atoi( arg );
            break;
        case 'k' :
            command.maximum = atoi( arg );
            break;
        case 'i' :
            command.increments = atoi( arg );
            break;
        case 'f' :
            command.factor = atof( arg );
            break;
        case 'r' :
            command.delay = atoi( arg );
            break;
        case 'P' :
            command.printOutput = true;
            break;
        case 'R' :
            command.printRanges = true;
            break;
        case 'O' :
            command.printOptions = true;
            break;
    }
    return 0;
};

// ----------------------------------------------------------------------------
//  argp parser parameter structure.
// ----------------------------------------------------------------------------
static struct argp argp = { options, parse_opt, args_doc, doc };


//  Checking functions. -------------------------------------------------------

// ----------------------------------------------------------------------------
//  Returns true if a parameter is within bounds, else false.
// ----------------------------------------------------------------------------
static bool checkIfInBounds( float value, float lower, float upper )
{
    if (( value < lower ) || ( value > upper )) return false;
    else return true;
};

// ----------------------------------------------------------------------------
//  Returns true if all options are within bounds, else false.
// ----------------------------------------------------------------------------
static bool checkParams ( void )
{
    static bool inBounds = true;
    inBounds = ( checkIfInBounds( command.volume,       // Volume soft limits.
                                  command.minimum,
                                  command.maximum ) ||
                 checkIfInBounds( command.balance,      // Balance
                                  bounds.balance[0],
                                  bounds.balance[1] ) ||
                 checkIfInBounds( command.volume,       // Starting volume.
                                  bounds.volume[0],
                                  bounds.volume[1] ) ||
                 checkIfInBounds( command.minimum,      // Minimum volume.
                                  bounds.volume[0],
                                  bounds.volume[1] ) ||
                 checkIfInBounds( command.maximum,      // Maximum volume
                                  bounds.volume[0],
                                  bounds.volume[1] ) ||
                 checkIfInBounds( command.increments,   // Increments.
                                  bounds.incs[0],
                                  bounds.incs[1] ) ||
                 checkIfInBounds( command.factor,       // Shaping factor.
                                  bounds.factor[0],
                                  bounds.factor[1] ) ||
                 checkIfInBounds( command.delay,        // Sensitivity delay.
                                  bounds.delay[0],
                                  bounds.delay[1] ));
    if ( !inBounds )
    {
        printf( "\nThere is something wrong with the set parameters.\n" );
        printf( "Use the -O -P -R flags to check values.\n\n" );
    }
    return inBounds;
};


//  Main section. -------------------------------------------------------------

int main( int argc, char *argv[] )
{
    //  Get command line arguments and check within bounds.
    argp_parse( &argp, argc, argv, 0, 0, &options );
    if ( !checkParams() ) return -1;

    //  Print out any information requested on command line.
    if ( command.printRanges ) printRanges();
    if ( command.printOptions ) printOptions();

    // Exit program for all information except program output.
    if (( command.printOptions ) || ( command.printRanges )) return 0;

    //  Set up volume data structure.
    sound.card      =   command.card;
    sound.mixer     =   command.mixer;
    sound.factor    =   command.factor;
    sound.volume    =   command.volume;
    sound.mute      =   false;
    sound.incs      =   command.increments;
    sound.print     =   command.printOutput;
    sound.min       =   command.minimum;
    sound.max       =   command.maximum;

    //  Initialise encoder and function button.
    encoderInit( command.gpioA, command.gpioB, command.gpioC );

    //  Initialise ALSA.
    soundOpen();

    //  Set initial volume.
    setVol();

    //  Check for attributes changed by interrupts.
    while ( 1 )
    {
        //  Volume.
        if (( encoder.direction != 0 ) && ( !sound.mute ))
        {
            // Volume +
            if ( encoder.direction > 0 ) incVol();
            // Volume -
            else decVol();
            encoder.direction = 0;
            setVol();
        }
        //  Button.
//        if ( button.state )
//        {
//            volume.mute = true;
//            button.state = false;
//            setVolumeMixer( volume );  // May be better to use playback switch.
//        }

        // Sensitivity delay.
        delay( command.delay );
    }

    return 0;
}
