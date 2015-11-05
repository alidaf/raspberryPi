// ****************************************************************************
// ****************************************************************************
/*
    piRotEnc:

    Rotary encoder control app for the Raspberry Pi.

    Copyright 2015 by Darren Faulke <darren@alidaf.co.uk>
    Portions based on IQ_rot.c, copyright 2015 Gordon Garrity.

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
    char balanceHigh;
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
    char direction;
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
    .volumeGPIO1 = 14,      // GPIO 1 for volume encoder.
    .volumeGPIO2 = 15,      // GPIO 2 for volume encoder.
    .volMuteGPIO = 2,       // GPIO for mute/unmute button.
    .balanceGPIO1 = 23,     // GPIO1 for balance encoder.
    .balanceGPIO2 = 24,     // GPIO2 for balance encoder.
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
    .incLow = 1,        // Lower bound for no. of volume increments.
    .incHigh = 100,     // Upper bound for no. of volume increments.
    .delayLow = 1,      // Lower bound for delay between tics.
    .delayHigh = 100    // Upper bound for delay between tics.
};


// ****************************************************************************
//  ALSA functions.
// ****************************************************************************

// ============================================================================
//  Returns mapped volume from linear volume based on shaping factor.
// ============================================================================
/*
    mappedVolume = ( base^fraction - 1 ) / ( base - 1 )
    fraction = linear volume / volume range.

    factor << 1, logarithmic.
    factor == 1, linear.
    factor >> 1, exponential.
*/
static long mapVolume( long linear, long range, long min, float factor )
{
    float base;
    long mapped;

    if ( factor == 1 ) mapped = linear;
    else mapped = lroundf((( pow( factor, (float) linear / range ) - 1 ) /
                             ( factor - 1 ) * range + min ));
    return mapped;
};


// ============================================================================
//  Set volume using ALSA mixers.
// ============================================================================
static void setVolumeMixer( struct volumeStruct volume )
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
    long softMin = ( volume.minimum * ( hardMax - hardMin )) / 100;
    long softMax = ( volume.maximum * ( hardMax - hardMin )) / 100;
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
    long linearLeft = (( indexLeft * softRange ) /
                          volume.increments ) + softMin;
    long linearRight = (( indexRight * softRange ) /
                          volume.increments ) + softMin;

    // Calculate mapped volumes.
    long mappedLeft = mapVolume( linearLeft, softRange,
                                 softMin, volume.factor );
    long mappedRight = mapVolume( linearRight, softRange,
                                 softMin, volume.factor );

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
        printf( " %10d | %10d | %10d | %10d |\n",
                linearLeft, linearRight,
                mappedLeft, mappedRight );
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
static void printParams ( struct cmdOptionsStruct cmdOptions )
{
    printf( "\nSet options:\n\n" );
    printf( "\tCard name = %s.\n", cmdOptions.card );
    printf( "\tMixer name = %s.\n", cmdOptions.mixer );
    printf( "\tVolume encoder attached to GPIOs %i", cmdOptions.volumeGPIO1 );
    printf( " & %i,\n", cmdOptions.volumeGPIO2 );
    printf( "\tMute button attached to GPIO %i.\n", cmdOptions.volMuteGPIO );
    printf( "\tBalance encoder attached to GPIOs %i", cmdOptions.balanceGPIO1 );
    printf( " & %i,\n", cmdOptions.balanceGPIO2 );
    printf( "\tBalance reset attached to GPIO %d.\n", cmdOptions.balResetGPIO );
    printf( "\tInitial volume = %i%%.\n", cmdOptions.volume );
    printf( "\tVolume increments = %i.\n", cmdOptions.increments );
    printf( "\tInitial balance = %i%%.\n", cmdOptions.balance );
    printf( "\tMinimum volume = %i%%.\n", cmdOptions.minimum );
    printf( "\tMaximum volume = %i%%.\n", cmdOptions.maximum );
    printf( "\tVolume factor = %g.\n", cmdOptions.factor );
    printf( "\tTic delay = %i.\n\n", cmdOptions.delay );
};


// ============================================================================
//  Prints command line parameter ranges.
// ============================================================================
static void printRanges( struct boundsStruct paramBounds )
{
    printf ( "\nCommand line parameter ranges:\n\n" );
    printf ( "\tVolume range (-i) = %d to %d.\n",
                    paramBounds.volumeLow,
                    paramBounds.volumeHigh );
    printf ( "\tBalance  range (-b) = %i%% to %i%%.\n",
                    paramBounds.balanceLow,
                    paramBounds.balanceHigh );
    printf ( "\tVolume shaping factor (-f) = %g to %g.\n",
                    paramBounds.factorLow,
                    paramBounds.factorHigh );
    printf ( "\tIncrement range (-e) = %d to %d.\n",
                    paramBounds.incLow,
                    paramBounds.incHigh );
    printf ( "\tDelay range (-d) = %d to %d.\n\n",
                    paramBounds.delayLow,
                    paramBounds.delayHigh );
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
    { "card", 'c', "<string>", 0, "ALSA card name" },
    { "mixer", 'm', "<string>", 0, "ALSA mixer name" },
    { 0, 0, 0, 0, "GPIO:" },
    { "gpiovol", 'A', "<int>,<int>", 0, "GPIO pins for volume encoder." },
    { "gpiobal", 'B', "<int>,<int>", 0, "GPIO pins for balance encoder." },
    { "gpiomut", 'C', "<int>", 0, "GPIO pin for mute button." },
    { "gpiores", 'D', "<int>", 0, "GPIO pin for balance reset button." },
    { 0, 0, 0, 0, "Volume:" },
    { "vol", 'v', "<int>", 0, "Initial volume (%)." },
    { "bal", 'b', "<int>", 0, "Initial L/R balance -100% to +100%." },
    { "min", 'j', "<int>", 0, "Minimum volume (% of full output)." },
    { "max", 'k', "<int>", 0, "Maximum volume (% of full output)." },
    { "inc", 'i', "<int>", 0, "Volume increments over range." },
    { "fac", 'f', "<float>", 0, "Volume profile factor." },
    { 0, 0, 0, 0, "Responsiveness:" },
    { "delay", 'r', "<int>", 0, "Interrupt delay (mS)." },
    { 0, 0, 0, 0, "Debugging:" },
    { "printoutput", 'P', 0, 0, "Print program output." },
    { "printdefault", 'Q', 0, 0, "Print default parameters." },
    { "printranges", 'R', 0, 0, "Print parameter ranges." },
    { "printset", 'S', 0, 0, "Print set parameters." },
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
static int checkIfInBounds( float value, float lower, float upper )
{
        if (( value < lower ) || ( value > upper )) return 1;
        else return 0;
};

// ============================================================================
//  Command line parameter validity checks.
// ============================================================================
static int checkParams ( struct cmdOptionsStruct *cmdOptions,
                         struct boundsStruct *paramBounds )
{
    static char error = 0;
    error = ( checkIfInBounds( cmdOptions->volume,
                               cmdOptions->minimum,
                               cmdOptions->maximum ) ||
              checkIfInBounds( cmdOptions->balance,
                               paramBounds->balanceLow,
                               paramBounds->balanceHigh ) ||
              checkIfInBounds( cmdOptions->volume,
                               paramBounds->volumeLow,
                               paramBounds->volumeHigh ) ||
              checkIfInBounds( cmdOptions->minimum,
                               paramBounds->volumeLow,
                               paramBounds->volumeHigh ) ||
              checkIfInBounds( cmdOptions->maximum,
                               paramBounds->volumeLow,
                               paramBounds->volumeHigh ) ||
              checkIfInBounds( cmdOptions->increments,
                               paramBounds->incLow,
                               paramBounds->incHigh ) ||
              checkIfInBounds( cmdOptions->factor,
                               paramBounds->factorLow,
                               paramBounds->factorHigh ) ||
              checkIfInBounds( cmdOptions->delay,
                               paramBounds->delayLow,
                               paramBounds->delayHigh ));
    if ( error )
    {
        printf( "\nThere is something wrong with the set parameters.\n" );
        printf( "Use the -m -p -q -r -s flags to check values.\n\n" );
    }
    return error;
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
    if ( checkParams( &cmdOptions, &paramBounds ) != 0 ) return -1;


    // ------------------------------------------------------------------------
    //  Print out any information requested on command line.
    // ------------------------------------------------------------------------
    if ( cmdOptions.printRanges ) printRanges( paramBounds );
    if ( cmdOptions.printOptions ) printParams( cmdOptions );

    // exit program for all information except program output.
    if (( cmdOptions.printOptions ) || ( cmdOptions.printRanges )) return 0;


    // ------------------------------------------------------------------------
    //  Set up volume data structure.
    // ------------------------------------------------------------------------
    static struct volumeStruct volume;
    volume.card = cmdOptions.card,
    volume.mixer = cmdOptions.mixer,
    volume.factor = cmdOptions.factor,
    volume.mute = false,
    volume.print = cmdOptions.printOutput,

        // Get closest volume index for starting volume.
    volume.index = ( cmdOptions.increments -
                   ( cmdOptions.maximum - cmdOptions.volume ) *
                     cmdOptions.increments /
                   ( cmdOptions.maximum - cmdOptions.minimum )),
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
    setVolumeMixer( volume );


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
            setVolumeMixer( volume );
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
            setVolumeMixer( volume );

        }
        // --------------------------------------------------------------------
        //  Mute.
        // --------------------------------------------------------------------
        if ( buttonMute.state )
        {
            volume.mute = true;
            buttonMute.state = false;
            setVolumeMixer( volume );  // May be better to use playback switch.
        }
        // --------------------------------------------------------------------
        //  Balance reset.
        // --------------------------------------------------------------------
        if ( buttonReset.state )
        {
            volume.balance = 0;
            buttonReset.state = false;
            setVolumeMixer( volume );
        }

        // Responsiveness - small delay to allow interrupts to finish.
        // Keep as small as possible.
        delay( cmdOptions.delay );
    }

    return 0;
}
