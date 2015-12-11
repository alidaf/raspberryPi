// ****************************************************************************
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
// ****************************************************************************

#define piRotEncVersion "Version 0.2"

//  Compilation:
//
//  Compile with gcc piRotEnc.c -o piRotEnc -lwiringPi -lasound -lm
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        G.Garrity   30/08/2015  IQ_rot.c
//                  D.Faulke    24/10/2015  This program.
//  Contributors:
//
//  Changelog:
//
//  v0.1 Original version - rewrite of rotencvol.c.
//  v0.2 Changed rotary encoder routine to use state table.
//

//  To Do:
//      Test balance encoder and buttons.
//      Add proper muting rather than set volume to 0, and set mute when 0.
//          - look at playback switch mechanism.
//      Improve bounds checking by using arrays for each parameter.
//      Add routine to check validity of GPIOs.
//      Improve error trapping and return codes for all functions.
//      Write GPIO and interrupt routines to replace wiringPi.
//      Remove all global variables.
//

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <wiringPi.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

#include "alsaPi.h"
#include "rotencPi.h"

// ============================================================================
// Data structures
// ============================================================================

// ----------------------------------------------------------------------------
// Data structures for command line arguments and bounds checking.
// ----------------------------------------------------------------------------
struct cmdOptionsStruct
{
    char *card;                     // ALSA card name.
    char *mixer;                    // Alsa mixer name.
    unsigned char rotaryGPIOA;      // GPIO pin for vol rotary encoder.
    unsigned char rotaryGPIOB;      // GPIO pin for vol rotary encoder.
//    unsigned char volMuteGPIO;      // GPIO pin for mute button.
//    unsigned char  balResetGPIO;    // GPIO pin for bal reset button.
    unsigned char volume;           // Initial volume (%).
//    unsigned char minimum;          // Minimum soft volume (%).
//    unsigned char maximum;          // Maximum soft volume (%).
    unsigned char increments;       // No. of increments over volume range.
    float factor;                   // Volume shaping factor.
//    char balance;                   // Volume L/R balance.
    unsigned char delay;            // Delay between encoder tics.
    bool printOutput;               // Flag to print output.
    bool printOptions;              // Flag to print options.
    bool printRanges;               // Flag to print ranges.
};

// Rewrite this using arrays.
struct boundsStruct
{
    unsigned char volumeLow;
    unsigned char volumeHigh;
//    char balanceLow;
//    unsigned char balanceHigh;
    float factorLow;
    float factorHigh;
    unsigned char incLow;
    unsigned char incHigh;
    unsigned char delayLow;
    unsigned char delayHigh;
};

// ----------------------------------------------------------------------------
//  Set default values.
// ----------------------------------------------------------------------------
static struct cmdOptionsStruct cmdOptions =
{
    .card = "hw:0",         // 1st ALSA card.
    .mixer = "PCM",         // Default ALSA control.
    .rotaryGPIOA = 23,      // GPIO 1 for volume encoder.
    .rotaryGPIOB = 24,      // GPIO 2 for volume encoder.
//    .volMuteGPIO = 2,       // GPIO for mute/unmute button.
//    .balanceGPIO1 = 14,     // GPIO1 for balance encoder.
//    .balanceGPIO2 = 15,     // GPIO2 for balance encoder.
//    .balResetGPIO = 3,      // GPIO for balance reset button.
    .volume = 0,            // Start muted.
//    .minimum = 0,           // 0% of Maximum output level.
//    .maximum = 100,         // 100% of Maximum output level.
    .increments = 20,       // 20 increments from 0 to 100%.
    .factor = 1,            // Volume change rate factor.
//    .balance = 0,           // L = R.
    .delay = 1,             // 1ms between interrupt checks
    .printOutput = false,   // No output printing.
    .printOptions = false,  // No command line options printing.
    .printRanges = false    // No range printing.
};

static struct boundsStruct paramBounds =
{
    .volumeLow = 0,     // Lower bound for volume.
    .volumeHigh = 100,  // Upper bound for volume.
//    .balanceLow = -100, // Lower bound for balance.
//    .balanceHigh = 100, // Upper bound for balance.
    .factorLow = 0.001, // Lower bound for volume shaping factor.
    .factorHigh = 10,   // Upper bound for volume shaping factor.
    .incLow = 10,       // Lower bound for no. of volume increments.
    .incHigh = 100,     // Upper bound for no. of volume increments.
    .delayLow = 1,      // Lower bound for delay between tics.
    .delayHigh = 100    // Upper bound for delay between tics.
};

// ============================================================================
//  GPIO activity call for mute button.
// ============================================================================
//static void buttonMuteInterrupt()
//{
//    int buttonRead = digitalRead( buttonMute.gpio );
//    if ( buttonRead ) buttonMute.state = !buttonMute.state;
//};

// ****************************************************************************
//  Information functions.
// ****************************************************************************

// ============================================================================
//  Prints default or command line set parameters values.
// ============================================================================
static void printOptions( struct cmdOptionsStruct cmdOptions )
{
    printf( "\n\t+-----------------+-----------------+\n" );
    printf( "\t| Option          | Value(s)        |\n" );
    printf( "\t+-----------------+-----------------+\n" );
    printf( "\t| Card name       | %-15s |\n", cmdOptions.card );
    printf( "\t| Mixer name      | %-15s |\n", cmdOptions.mixer );
    printf( "\t| Volume encoder  | GPIO%-2i", cmdOptions.rotaryGPIOA );
    printf( " & GPIO%-2i |\n", cmdOptions.rotaryGPIOB );
//    printf( "\t| Balance encoder | GPIO%-2i", cmdOptions.balanceGPIO1 );
//    printf( " & GPIO%i |\n", cmdOptions.balanceGPIO2 );
//    printf( "\t| Mute button     | GPIO%-11i |\n", cmdOptions.volMuteGPIO );
//    printf( "\t| Reset button    | GPIO%-11i |\n", cmdOptions.balResetGPIO );
    printf( "\t| Volume          | %3i%% %10s |\n", cmdOptions.volume, "" );
    printf( "\t| Increments      | %3i %11s |\n", cmdOptions.increments, "" );
//    printf( "\t| Balance         | %3i%% %10s |\n", cmdOptions.balance, "" );
//    printf( "\t| Minimum         | %3i%% %10s |\n", cmdOptions.minimum, "" );
//    printf( "\t| Maximum         | %3i%% %10s |\n", cmdOptions.maximum, "" );
    printf( "\t| Factor          | %7.3f %7s |\n", cmdOptions.factor, "" );
    printf( "\t| Interrupt delay | %3i %11s |\n", cmdOptions.delay, "" );
    printf( "\t+-----------------+-----------------+\n\n" );
};

// ============================================================================
//  Prints command line parameter ranges.
// ============================================================================
static void printRanges( struct boundsStruct paramBounds )
{
    printf( "\nCommand line option ranges:\n\n" );
    printf( "\t+------------+--------+-------+-------+\n" );
    printf( "\t| Parameter  | Switch |  min  |  max  |\n" );
    printf( "\t+------------+--------+-------+-------+\n" );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Volume", "-v", paramBounds.volumeLow, paramBounds.volumeHigh );
//    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
//            "Balance", "-b", paramBounds.balanceLow, paramBounds.balanceHigh );
    printf( "\t| %-10s |   %2s   | %5.3f | %5.2f |\n",
            "Factor", "-f", paramBounds.factorLow, paramBounds.factorHigh );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Increments", "-i", paramBounds.incLow, paramBounds.incHigh );
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Response", "-d", paramBounds.delayLow, paramBounds.delayHigh );
    printf( "\t+------------+--------+-------+-------+\n\n" );
};

// ****************************************************************************
//  Command line option functions.
// ****************************************************************************

// ============================================================================
//  argp documentation.
// ============================================================================
const char *argp_program_version = piRotEncVersion;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi rotary encoder control.";
static const char args_doc[] = "piRotEnc <options>";

// ============================================================================
//  Command line argument definitions.
// ============================================================================
static struct argp_option options[] =
{
    { 0, 0, 0, 0, "ALSA:" },
    { "card",      'c', "<string>",    0, "ALSA card name" },
    { "mixer",     'm', "<string>",    0, "ALSA mixer name" },
    { 0, 0, 0, 0, "GPIO:" },
    { "gpiovol",   'A', "<int>,<int>", 0, "GPIOs for volume encoder." },
//    { "gpiobal",   'B', "<int>,<int>", 0, "GPIOs for balance encoder." },
//    { "gpiomut",   'C', "<int>",       0, "GPIO for mute button." },
//    { "gpiores",   'D', "<int>",       0, "GPIO for balance reset button." },
    { 0, 0, 0, 0, "Volume:" },
    { "vol",       'v', "<int>",       0, "Initial volume (%)." },
//    { "bal",       'b', "<int>",       0, "Initial L/R balance (%)." },
//    { "min",       'j', "<int>",       0, "Minimum volume (%)." },
//    { "max",       'k', "<int>",       0, "Maximum volume (%)." },
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

// ============================================================================
//  Command line argument parser.
// ============================================================================
static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str, *token;
    const char delimiter[] = ",";
    struct cmdOptionsStruct *cmdOptions = state->input;

    switch ( param )
    {
        case 'c' :
            cmdOptions->card = arg;
            break;
        case 'm' :
            cmdOptions->mixer = arg;
            break;
        case 'A' :
            str = arg;
            token = strtok( str, delimiter );
            cmdOptions->rotaryGPIOA = atoi( token );
            token = strtok( NULL, delimiter );
            cmdOptions->rotaryGPIOB = atoi( token );
            break;
//        case 'B' :
//            str = arg;
//            token = strtok( str, delimiter );
//            cmdOptions->balanceGPIO1 = atoi( token );
//            token = strtok( NULL, delimiter );
//            cmdOptions->balanceGPIO2 = atoi( token );
//            break;
//        case 'C' :
//            cmdOptions->volMuteGPIO = atoi( arg );
//            break;
//        case 'D' :
//            cmdOptions->balResetGPIO = atoi( arg );
//            break;
        case 'v' :
            cmdOptions->volume = atoi( arg );
            break;
//        case 'b' :
//            cmdOptions->balance = atoi( arg );
//            break;
//        case 'j' :
//            cmdOptions->minimum = atoi( arg );
//            break;
//        case 'k' :
//            cmdOptions->maximum = atoi( arg );
//            break;
        case 'i' :
            cmdOptions->increments = atoi( arg );
            break;
        case 'f' :
            cmdOptions->factor = atof( arg );
            break;
        case 'r' :
            cmdOptions->delay = atoi( arg );
            break;
        case 'P' :
            cmdOptions->printOutput = true;
            break;
        case 'R' :
            cmdOptions->printRanges = true;
            break;
        case 'O' :
            cmdOptions->printOptions = true;
            break;
    }
    return 0;
};

// ============================================================================
//  argp parser parameter structure.
// ============================================================================
static struct argp argp = { options, parse_opt, args_doc, doc };

// ****************************************************************************
//  Checking functions.
// ****************************************************************************

// ============================================================================
//  Checks if a parameter is within bounds.
// ============================================================================
static bool checkIfInBounds( float value, float lower, float upper )
{
        if (( value < lower ) || ( value > upper )) return false;
        else return true;
};

// ============================================================================
//  Command line parameter validity checks.
// ============================================================================
static int checkParams ( struct cmdOptionsStruct cmdOptions,
                         struct boundsStruct paramBounds )
{
    static bool inBounds;
    inBounds = ( //checkIfInBounds( cmdOptions.volume,
//                                  cmdOptions.minimum,
//                                  cmdOptions.maximum ) ||
//                 checkIfInBounds( cmdOptions.balance,
//                                  paramBounds.balanceLow,
//                                  paramBounds.balanceHigh ) ||
                 checkIfInBounds( cmdOptions.volume,
                                  paramBounds.volumeLow,
                                  paramBounds.volumeHigh ) ||
//                 checkIfInBounds( cmdOptions.minimum,
//                                  paramBounds.volumeLow,
//                                  paramBounds.volumeHigh ) ||
//                 checkIfInBounds( cmdOptions.maximum,
//                                  paramBounds.volumeLow,
//                                  paramBounds.volumeHigh ) ||
                 checkIfInBounds( cmdOptions.increments,
                                  paramBounds.incLow,
                                  paramBounds.incHigh ) ||
                 checkIfInBounds( cmdOptions.factor,
                                  paramBounds.factorLow,
                                  paramBounds.factorHigh ) ||
                 checkIfInBounds( cmdOptions.delay,
                                  paramBounds.delayLow,
                                  paramBounds.delayHigh ));
    if ( !inBounds )
    {
        printf( "\nThere is something wrong with the set parameters.\n" );
        printf( "Use the -O -P -R flags to check values.\n\n" );
    }
    return inBounds;
};

// ============================================================================
//  Main section.
// ============================================================================
int main( int argc, char *argv[] )
{
    // ------------------------------------------------------------------------
    //  Set defaults.
    // ------------------------------------------------------------------------
//    printf( "Setting defaults.............................." );
//    sound.card = "hw:0";
//    sound.mixer = "default";
//    sound.factor = 1.0;
//    sound.index = 0;
//    sound.incs = 20;
//    sound.volume = 0;
//    sound.mute = false;
//    sound.print = false;
//    printf( "done.\n" );
    // ------------------------------------------------------------------------
    //  Get command line arguments and check within bounds.
    // ------------------------------------------------------------------------
    printf( "Getting command line options.................." );
    argp_parse( &argp, argc, argv, 0, 0, &cmdOptions );
    if ( !checkParams( cmdOptions, paramBounds )) return -1;
    printf( "done.\n" );

    // ------------------------------------------------------------------------
    //  Print out any information requested on command line.
    // ------------------------------------------------------------------------
    if ( cmdOptions.printRanges ) printRanges( paramBounds );
    if ( cmdOptions.printOptions ) printOptions( cmdOptions );

    // exit program for all information except program output.
    if (( cmdOptions.printOptions ) || ( cmdOptions.printRanges )) return 0;

    // ------------------------------------------------------------------------
    //  Set up volume data structure.
    // ------------------------------------------------------------------------
    printf( "Setting up data structures...................." );
    sound.card = cmdOptions.card;
    sound.mixer = cmdOptions.mixer;
    sound.factor = cmdOptions.factor;
    sound.volume = cmdOptions.volume;
    sound.mute = false;
    sound.incs = cmdOptions.increments;
    sound.print = cmdOptions.printOutput;

//    sound.minimum = cmdOptions.minimum;
//    sound.maximum = cmdOptions.maximum;
    printf( "done.\n" );

    // ------------------------------------------------------------------------
    //  Set up encoder and button data structures.
    // ------------------------------------------------------------------------
    printf( "Initialising encoder.........................." );
    encoderInit( cmdOptions.rotaryGPIOA, cmdOptions.rotaryGPIOB );
    printf( "done.\n" );

//    buttonMute.gpio = cmdOptions.volMuteGPIO;
//    buttonMute.state = 0;

//    buttonReset.gpio = cmdOptions.balResetGPIO;
//    buttonReset.state = 0;

    // Set button pin modes.
//    pinMode( buttonMute.gpio, INPUT );
//    pullUpDnControl( buttonMute.gpio, PUD_UP );

//    pinMode( buttonReset.gpio, INPUT );
//    pullUpDnControl( buttonReset.gpio, PUD_UP );

    // ------------------------------------------------------------------------
    //  Set initial volume.
    // ------------------------------------------------------------------------
    printf( "Setting up ALSA..............................." );
    soundOpen();
    printf( "done.\n" );
    // Get closest volume index for starting volume.
    printf( "Setting index for initial volume.............." );
    setIndex( sound.volume, sound.incs, sound.min, sound.max );
    printf( "done.\n" );

    printf( "Parameters are:\n" );
    printf( "\tCard = %s.\n", sound.card );
    printf( "\tMixer = %s.\n", sound.mixer );
    printf( "\tFactor = %f.\n", sound.factor );
    printf( "\tIndex = %d.\n", sound.index );
    printf( "\tVolume = %d.\n", sound.volume );
    printf( "\tMin Volume = %d.\n", sound.min );
    printf( "\tMax Volume = %d.\n", sound.max );
    printf( "\tIncrements = %d.\n", sound.incs );

    // ------------------------------------------------------------------------
    //  Register interrupt functions.
    // ------------------------------------------------------------------------
    printf( "Setting up interrupt functions................" );
    wiringPiISR( encoder.gpioA, INT_EDGE_BOTH, &encoderDirection );
    wiringPiISR( encoder.gpioB, INT_EDGE_BOTH, &encoderDirection );
    printf( "done.\n" );
//    wiringPiISR( buttonMute.gpio, INT_EDGE_BOTH, &buttonMuteInterrupt );
//    wiringPiISR( buttonReset.gpio, INT_EDGE_BOTH, &buttonResetInterrupt );

    printf( "Setting initial volume and starting main routine.\n" );
    setVol();

    // ------------------------------------------------------------------------
    //  Check for attributes changed by interrupts.
    // ------------------------------------------------------------------------
    while ( 1 )
    {
        // --------------------------------------------------------------------
        //  Volume.
        // --------------------------------------------------------------------
        if (( encoder.direction != 0 ) && ( !sound.mute ))
        {
            // Volume +
            if ( encoder.direction > 0 ) incVol();
            // Volume -
            else decVol();
            encoder.direction = 0;
            setVol();
        }
        // --------------------------------------------------------------------
        //  Mute.
        // --------------------------------------------------------------------
//        if ( buttonMute.state )
//        {
//            volume.mute = true;
//            buttonMute.state = false;
//            setVolumeMixer( volume, 1 );  // May be better to use playback switch.
//        }
        // --------------------------------------------------------------------
        //  Balance reset.
        // --------------------------------------------------------------------
//        if ( buttonReset.state )
//        {
//            volume.balance = 0;
//            buttonReset.state = false;
//            setVolumeMixer( volume, 1 );
//        }

        // Responsiveness - small delay to allow interrupts to finish.
        // Keep as small as possible.
        delay( cmdOptions.delay );
    }

    return 0;
}
