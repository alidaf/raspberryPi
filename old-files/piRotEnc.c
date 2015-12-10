// ****************************************************************************
// ****************************************************************************
/*
    piRotEnc:

    Rotary encoder control app for the Raspberry Pi.

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

#define Version "Version 0.2"

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

// ============================================================================
// Data structures
// ============================================================================

/*
    Note: char is the smallest integer size (usually 8 bit) and is used to
          keep the memory footprint as low as possible.
*/

// ----------------------------------------------------------------------------
// Data structures for command line arguments and bounds checking.
// ----------------------------------------------------------------------------
struct cmdOptionsStruct
{
    char *card;                     // ALSA card name.
    char *mixer;                    // Alsa mixer name.
    unsigned char volumeGPIO1;      // GPIO pin for vol rotary encoder.
    unsigned char volumeGPIO2;      // GPIO pin for vol rotary encoder.
    unsigned char volMuteGPIO;      // GPIO pin for mute button.
    unsigned char balanceGPIO1;     // GPIO pin for bal rotary encoder.
    unsigned char balanceGPIO2;     // GPIO pin for bal rotary encoder.
    unsigned char  balResetGPIO;    // GPIO pin for bal reset button.
    unsigned char volume;           // Initial volume (%).
    unsigned char minimum;          // Minimum soft volume (%).
    unsigned char maximum;          // Maximum soft volume (%).
    unsigned char increments;       // No. of increments over volume range.
    float factor;                   // Volume shaping factor.
    char balance;                   // Volume L/R balance.
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
    char balanceLow;
    unsigned char balanceHigh;
    float factorLow;
    float factorHigh;
    unsigned char incLow;
    unsigned char incHigh;
    unsigned char delayLow;
    unsigned char delayHigh;
};

// ----------------------------------------------------------------------------
// Data structure for volume.
// ----------------------------------------------------------------------------
struct volumeStruct
{
    char *card;                 // Card name.
    char *mixer;                // Mixer name.
    float factor;               // Volume mapping factor.
    unsigned char index;        // Relative index for volume level.
    unsigned char increments;   // Number of increments over soft range.
    unsigned char minimum;      // Soft minimum level.
    unsigned char maximum;      // Soft maximum level.
    char balance;               // Relative balance.
    bool mute;                  // Mute switch.
    bool print;                 // Print output switch.
};

// ----------------------------------------------------------------------------
// Data structures for rotary encoder and button functions.
// ----------------------------------------------------------------------------
struct encoderStruct
{
    unsigned char gpio1;
    unsigned char gpio2;
    unsigned char state;
    int direction;
    char position;
};

struct buttonStruct
{
    unsigned char gpio;
    unsigned char state;
};

// Define now for global access (due to limitation of wiringPi).
static struct encoderStruct encoderVolume, encoderBalance;
static struct buttonStruct buttonMute, buttonReset;


// ----------------------------------------------------------------------------
//  Set default values.
// ----------------------------------------------------------------------------
static struct cmdOptionsStruct cmdOptions =
{
    .card = "hw:0",         // 1st ALSA card.
    .mixer = "PCM",         // Default ALSA control.
    .volumeGPIO1 = 23,      // GPIO 1 for volume encoder.
    .volumeGPIO2 = 24,      // GPIO 2 for volume encoder.
    .volMuteGPIO = 2,       // GPIO for mute/unmute button.
    .balanceGPIO1 = 14,     // GPIO1 for balance encoder.
    .balanceGPIO2 = 15,     // GPIO2 for balance encoder.
    .balResetGPIO = 3,      // GPIO for balance reset button.
    .volume = 0,            // Start muted.
    .minimum = 0,           // 0% of Maximum output level.
    .maximum = 100,         // 100% of Maximum output level.
    .increments = 20,       // 20 increments from 0 to 100%.
    .factor = 1,            // Volume change rate factor.
    .balance = 0,           // L = R.
    .delay = 1,             // 1ms between interrupt checks
    .printOutput = false,   // No output printing.
    .printOptions = false,  // No command line options printing.
    .printRanges = false    // No range printing.
};

static struct boundsStruct paramBounds =
{
    .volumeLow = 0,     // Lower bound for volume.
    .volumeHigh = 100,  // Upper bound for volume.
    .balanceLow = -100, // Lower bound for balance.
    .balanceHigh = 100, // Upper bound for balance.
    .factorLow = 0.001, // Lower bound for volume shaping factor.
    .factorHigh = 10,   // Upper bound for volume shaping factor.
    .incLow = 10,       // Lower bound for no. of volume increments.
    .incHigh = 100,     // Upper bound for no. of volume increments.
    .delayLow = 1,      // Lower bound for delay between tics.
    .delayHigh = 100    // Upper bound for delay between tics.
};


// ****************************************************************************
//  ALSA functions.
// ****************************************************************************

// ============================================================================
//  Returns mapped volume based on shaping factor.
// ============================================================================
/*
    mappedVolume = ( factor^fraction - 1 ) / ( base - 1 )
    fraction = linear volume / volume range.

    factor << 1, logarithmic.
    factor == 1, linear.
    factor >> 1, exponential.
*/
static long getMappedVolume( float index, float increments,
                             float range, float minimum, float factor )
{
    long mappedVolume;

    if ( factor == 1 )
         mappedVolume = lroundf(( index / increments * range ) + minimum );
    else mappedVolume = lroundf((( pow( factor, index / increments ) - 1 ) /
                                ( factor - 1 ) * range + minimum ));
    return mappedVolume;
};


// ============================================================================
//  Returns linear volume for ALSA.
// ============================================================================
/*
    Simple calculation but avoids typecasting.
*/
long getSoftValue( float volume, float min, float max )
{
    long softValue = ( volume * ( max - min ) / 100 + min );
    return softValue;
}


// ============================================================================
//  Set volume using ALSA mixers.
// ============================================================================
static void setVolumeMixer( struct volumeStruct volume, char body )
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


    // ------------------------------------------------------------------------
    //  Set up ALSA mixer.
    // ------------------------------------------------------------------------
    snd_mixer_open( &mixerHandle, 0 );
    snd_mixer_attach( mixerHandle, volume.card );
    snd_mixer_load( mixerHandle );
    snd_mixer_selem_register( mixerHandle, NULL, NULL );

    snd_mixer_selem_id_alloca( &mixerId );
    snd_mixer_selem_id_set_name( mixerId, volume.mixer );
    mixerElem = snd_mixer_find_selem( mixerHandle, mixerId );
    snd_mixer_selem_get_id( mixerElem, mixerId );

    // ------------------------------------------------------------------------
    //  Get some information for selected mixer.
    // ------------------------------------------------------------------------
    // Hardware volume limits.
    long hardMin, hardMax;
    snd_mixer_selem_get_playback_volume_range( mixerElem, &hardMin, &hardMax );
    long hardRange = hardMax - hardMin;

    // Soft volume limits.
    long softMin = getSoftValue( volume.minimum, hardMin, hardMax );
    long softMax = getSoftValue( volume.maximum, hardMin, hardMax );
    long softRange = softMax - softMin;


    // ------------------------------------------------------------------------
    //  Calculate volume and check bounds.
    // ------------------------------------------------------------------------
    /*
        Volume is set based on index / increments.
        Volume adjustments modify the index rather than a specific value so
        rounding errors don't accumulate.
    */

    // Calculate left and right channel indices.
    unsigned char indexLeft, indexRight;
    if ( volume.balance < 0 )
    {
        indexLeft = volume.index + volume.balance;
        indexRight = volume.index;
    }
    else
    if ( volume.balance > 0 )
    {
        indexLeft = volume.index;
        indexRight = volume.index - volume.index;
    }
    else
        indexLeft = indexRight = volume.index;

    // Calculate linear volume for left and right channels.
    long linearLeft, linearRight;
    linearLeft = getMappedVolume( indexLeft, volume.increments,
                                  softRange, softMin, 1 );
    linearRight = getMappedVolume( indexRight, volume.increments,
                                   softRange, softMin, 1 );

    // Calculate mapped volumes.
    long mappedLeft = getMappedVolume( indexLeft, volume.increments,
                                       softRange, softMin, volume.factor );
    long mappedRight = getMappedVolume( indexRight, volume.increments,
                                        softRange, softMin, volume.factor );

    // ------------------------------------------------------------------------
    //  Set volume.
    // ------------------------------------------------------------------------
        // If control is mono then FL will set volume.
        snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_LEFT, mappedLeft );
        snd_mixer_selem_set_playback_volume( mixerElem,
                SND_MIXER_SCHN_FRONT_RIGHT, mappedRight );


    // ------------------------------------------------------------------------
    //  Print output if requested.
    // ------------------------------------------------------------------------
    if ( volume.print )
    {
        if ( body == 0 )
        {
            printf( "\n" );
            printf( "\t+-----------+-----------------+-----------------+\n" );
            printf( "\t| indices   | Linear Volume   | Mapped Volume   |\n" );
            printf( "\t+-----+-----+--------+--------+--------+--------+\n" );
            printf( "\t| L   | R   | L      | R      | L      | R      |\n" );
            printf( "\t+-----+-----+--------+--------+--------+--------+\n" );
            printf( "\t| %3i | %3i | %6d | %6d | %6d | %6d |\n",
                    indexLeft, indexRight,
                    linearLeft, linearRight,
                    mappedLeft, mappedRight );
        }
        else
        {
            printf( "\t| %3i | %3i | %6d | %6d | %6d | %6d |\n",
                    indexLeft, indexRight,
                    linearLeft, linearRight,
                    mappedLeft, mappedRight );
        }
    }


    // ------------------------------------------------------------------------
    //  Clean up.
    // ------------------------------------------------------------------------
    snd_mixer_detach( mixerHandle, volume.card );
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

    State table for full step mode:
    +---------+---------+---------+---------+
    | AB = 00 | AB = 01 | AB = 10 | AB = 11 |
    +---------+---------+---------+---------+
    | START   | C/W 1   | A/C 1   | START   |
    | C/W +   | START   | C/W X   | C/W DIR |
    | C/W +   | C/W 1   | START   | START   |
    | C/W +   | C/W 1   | C/W X   | START   |
    | A/C +   | START   | A/C 1   | START   |
    | A/C +   | A/C X   | START   | A/C DIR |
    | A/C +   | A/C X   | A/C 1   | START   |
    +---------+---------+---------+---------+
*/

const unsigned char stateTable[7][4] =
    {{ 0x0, 0x2, 0x4, 0x0 },
     { 0x3, 0x0, 0x1, 0x10 },
     { 0x3, 0x2, 0x0, 0x0 },
     { 0x3, 0x2, 0x1, 0x0 },
     { 0x6, 0x0, 0x4, 0x0 },
     { 0x6, 0x5, 0x0, 0x20 },
     { 0x6, 0x5, 0x4, 0x0 }};

/*
    Need separate functions for volume and balance due to wiringPi not being
    able to register interrupt functions that have parameters. Forces the use
    of global variables and code duplication.
*/

// ============================================================================
//  Returns encoder direction for volume.
// ============================================================================
static void encoderVolumeInterrupt()
{
    unsigned char code = ( digitalRead( encoderVolume.gpio2 ) << 1 ) |
                           digitalRead( encoderVolume.gpio1 );
    encoderVolume.state = stateTable[ encoderVolume.state & 0xf ][ code ];
    unsigned char direction = encoderVolume.state & 0x30;
    if ( direction )
        encoderVolume.direction = ( direction == 0x10 ? -1 : 1 );
    else
        encoderVolume.direction = 0;
    return;
};


// ============================================================================
//  Returns encoder direction for balance.
// ============================================================================
static void encoderBalanceInterrupt()
{
    unsigned char code = ( digitalRead( encoderBalance.gpio2 ) << 1 ) |
                           digitalRead( encoderBalance.gpio1 );
    encoderBalance.state = stateTable[ encoderBalance.state & 0xf ][ code ];
    unsigned char direction = encoderBalance.state & 0x30;
    if ( direction )
        encoderBalance.direction = ( direction == 0x10 ? -1 : 1 );
    else
        encoderBalance.direction = 0;
    return;
};


// ============================================================================
//  GPIO activity call for mute button.
// ============================================================================
static void buttonMuteInterrupt()
{
    int buttonRead = digitalRead( buttonMute.gpio );
    if ( buttonRead ) buttonMute.state = !buttonMute.state;
};


// ============================================================================
//  GPIO activity call for balance reset button.
// ============================================================================
static void buttonResetInterrupt()
{
    int buttonRead = digitalRead( buttonReset.gpio );
    if ( buttonRead ) buttonReset.state = !buttonReset.state;
};


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
    printf( "\t| Volume encoder  | GPIO%-2i", cmdOptions.volumeGPIO1 );
    printf( " & GPIO%-2i |\n", cmdOptions.volumeGPIO2 );
    printf( "\t| Balance encoder | GPIO%-2i", cmdOptions.balanceGPIO1 );
    printf( " & GPIO%i |\n", cmdOptions.balanceGPIO2 );
    printf( "\t| Mute button     | GPIO%-11i |\n", cmdOptions.volMuteGPIO );
    printf( "\t| Reset button    | GPIO%-11i |\n", cmdOptions.balResetGPIO );
    printf( "\t| Volume          | %3i%% %10s |\n", cmdOptions.volume, "" );
    printf( "\t| Increments      | %3i %11s |\n", cmdOptions.increments, "" );
    printf( "\t| Balance         | %3i%% %10s |\n", cmdOptions.balance, "" );
    printf( "\t| Minimum         | %3i%% %10s |\n", cmdOptions.minimum, "" );
    printf( "\t| Maximum         | %3i%% %10s |\n", cmdOptions.maximum, "" );
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
    printf( "\t| %-10s |   %2s   |  %3i  |  %3i  |\n",
            "Balance", "-b", paramBounds.balanceLow, paramBounds.balanceHigh );
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
const char *argp_program_version = Version;
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
    { "gpiobal",   'B', "<int>,<int>", 0, "GPIOs for balance encoder." },
    { "gpiomut",   'C', "<int>",       0, "GPIO for mute button." },
    { "gpiores",   'D', "<int>",       0, "GPIO for balance reset button." },
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
            cmdOptions->volumeGPIO1 = atoi( token );
            token = strtok( NULL, delimiter );
            cmdOptions->volumeGPIO2 = atoi( token );
            break;
        case 'B' :
            str = arg;
            token = strtok( str, delimiter );
            cmdOptions->balanceGPIO1 = atoi( token );
            token = strtok( NULL, delimiter );
            cmdOptions->balanceGPIO2 = atoi( token );
            break;
        case 'C' :
            cmdOptions->volMuteGPIO = atoi( arg );
            break;
        case 'D' :
            cmdOptions->balResetGPIO = atoi( arg );
            break;
        case 'v' :
            cmdOptions->volume = atoi( arg );
            break;
        case 'b' :
            cmdOptions->balance = atoi( arg );
            break;
        case 'j' :
            cmdOptions->minimum = atoi( arg );
            break;
        case 'k' :
            cmdOptions->maximum = atoi( arg );
            break;
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
    inBounds = ( checkIfInBounds( cmdOptions.volume,
                                  cmdOptions.minimum,
                                  cmdOptions.maximum ) ||
                 checkIfInBounds( cmdOptions.balance,
                                  paramBounds.balanceLow,
                                  paramBounds.balanceHigh ) ||
                 checkIfInBounds( cmdOptions.volume,
                                  paramBounds.volumeLow,
                                  paramBounds.volumeHigh ) ||
                 checkIfInBounds( cmdOptions.minimum,
                                  paramBounds.volumeLow,
                                  paramBounds.volumeHigh ) ||
                 checkIfInBounds( cmdOptions.maximum,
                                  paramBounds.volumeLow,
                                  paramBounds.volumeHigh ) ||
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
char main( int argc, char *argv[] )
{
    static int error = 0; // Error trapping flag.

    // ------------------------------------------------------------------------
    //  Get command line arguments and check within bounds.
    // ------------------------------------------------------------------------
    argp_parse( &argp, argc, argv, 0, 0, &cmdOptions );
    if ( !checkParams( cmdOptions, paramBounds )) return -1;


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
    static struct volumeStruct volume;
    volume.card = cmdOptions.card;
    volume.mixer = cmdOptions.mixer;
    volume.factor = cmdOptions.factor;
    volume.mute = false;
    volume.increments = cmdOptions.increments;
    volume.print = cmdOptions.printOutput;
    volume.minimum = cmdOptions.minimum;
    volume.maximum = cmdOptions.maximum;

    // Get closest volume index for starting volume.
    volume.index = ( cmdOptions.increments -
                   ( cmdOptions.maximum - cmdOptions.volume ) *
                     cmdOptions.increments /
                   ( cmdOptions.maximum - cmdOptions.minimum ));

    // Get closest balance index for starting balance.
    volume.balance = ( cmdOptions.balance * cmdOptions.increments ) / 100;

    // ------------------------------------------------------------------------
    //  Set up encoder and button data structures.
    // ------------------------------------------------------------------------
    encoderVolume.gpio1 = cmdOptions.volumeGPIO1;
    encoderVolume.gpio2 = cmdOptions.volumeGPIO2;
    encoderVolume.state = 0;
    encoderVolume.direction = 0;

    encoderBalance.gpio1 = cmdOptions.balanceGPIO1;
    encoderBalance.gpio2 = cmdOptions.balanceGPIO2;
    encoderBalance.state = 0;
    encoderBalance.direction = 0;

    buttonMute.gpio = cmdOptions.volMuteGPIO;
    buttonMute.state = 0;

    buttonReset.gpio = cmdOptions.balResetGPIO;
    buttonReset.state = 0;


    // ------------------------------------------------------------------------
    //  Initialise wiringPi.
    // ------------------------------------------------------------------------
    wiringPiSetupGpio(); // Use GPIO numbers rather than wiringPi numbers.

    // Set encoder pin modes.
    pinMode( encoderVolume.gpio1, INPUT );
    pinMode( encoderVolume.gpio2, INPUT );
    pullUpDnControl( encoderVolume.gpio1, PUD_UP );
    pullUpDnControl( encoderVolume.gpio2, PUD_UP );

    pinMode( encoderBalance.gpio1, INPUT );
    pinMode( encoderBalance.gpio2, INPUT );
    pullUpDnControl( encoderBalance.gpio1, PUD_UP );
    pullUpDnControl( encoderBalance.gpio2, PUD_UP );

    // Set button pin modes.
    pinMode( buttonMute.gpio, INPUT );
    pullUpDnControl( buttonMute.gpio, PUD_UP );

    pinMode( buttonReset.gpio, INPUT );
    pullUpDnControl( buttonReset.gpio, PUD_UP );


    // ------------------------------------------------------------------------
    //  Set initial volume.
    // ------------------------------------------------------------------------
    setVolumeMixer( volume, 0 );


    // ------------------------------------------------------------------------
    //  Register interrupt functions.
    // ------------------------------------------------------------------------
    wiringPiISR( encoderVolume.gpio1, INT_EDGE_BOTH, &encoderVolumeInterrupt );
    wiringPiISR( encoderVolume.gpio2, INT_EDGE_BOTH, &encoderVolumeInterrupt );
    wiringPiISR( encoderBalance.gpio1, INT_EDGE_BOTH, &encoderBalanceInterrupt );
    wiringPiISR( encoderBalance.gpio2, INT_EDGE_BOTH, &encoderBalanceInterrupt );
    wiringPiISR( buttonMute.gpio, INT_EDGE_BOTH, &buttonMuteInterrupt );
    wiringPiISR( buttonReset.gpio, INT_EDGE_BOTH, &buttonResetInterrupt );


    // ------------------------------------------------------------------------
    //  Check for attributes changed by interrupts.
    // ------------------------------------------------------------------------
    while ( 1 )
    {
        // --------------------------------------------------------------------
        //  Volume.
        // --------------------------------------------------------------------
        if (( encoderVolume.direction != 0 ) && ( !volume.mute ))
        {
            // Volume +
            if (( encoderVolume.direction > 0 ) &&
                ( volume.index < volume.increments )) volume.index++;
            else
            // Volume -
            if (( encoderVolume.direction < 0 ) &&
                ( volume.index > 0 )) volume.index--;

            encoderVolume.direction = 0;
            setVolumeMixer( volume, 1 );
        }
        // --------------------------------------------------------------------
        //  Balance.
        // --------------------------------------------------------------------
        if (( encoderBalance.direction != 0 ) && ( !volume.mute ))
        {
            // Balance +
            if (( encoderBalance.direction > 0 ) &&
                ( volume.balance < volume.increments )) volume.balance++;
            else
            // Balance -
            if (( encoderBalance.direction < 0 ) &&
                ( volume.balance > -volume.increments )) volume.balance--;

            encoderBalance.direction = 0;
            setVolumeMixer( volume, 1 );

        }
        // --------------------------------------------------------------------
        //  Mute.
        // --------------------------------------------------------------------
        if ( buttonMute.state )
        {
            volume.mute = true;
            buttonMute.state = false;
            setVolumeMixer( volume, 1 );  // May be better to use playback switch.
        }
        // --------------------------------------------------------------------
        //  Balance reset.
        // --------------------------------------------------------------------
        if ( buttonReset.state )
        {
            volume.balance = 0;
            buttonReset.state = false;
            setVolumeMixer( volume, 1 );
        }

        // Responsiveness - small delay to allow interrupts to finish.
        // Keep as small as possible.
        delay( cmdOptions.delay );
    }

    return 0;
}
