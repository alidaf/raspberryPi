/****************************************************************************/
/*                                                                          */
/*  Rotary encoder control app for the Raspberry Pi.                        */
/*                                                                          */
/*  Copyright  2015 by Darren Faulke <darren@alidaf.co.uk>                  */
/*                                                                          */
/*  This program is free software; you can redistribute it and/or modify    */
/*  it under the terms of the GNU General Public License as published by    */
/*  the Free Software Foundation, either version 2 of the License, or       */
/*  (at your option) any later version.                                     */
/*                                                                          */
/*  This program is distributed in the hope that it will be useful,         */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/*  GNU General Public License for more details.                            */
/*                                                                          */
/*  You should have received a copy of the GNU General Public License       */
/*  along with this program. If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                          */
/****************************************************************************/

#define Version "Version 2.9"

/*****************************************************************************/
/*                                                                           */
/*  Authors:                 D.Faulke                 03/10/2015             */
/*  Contributors:            G.Garrity                30/08/2015             */
/*                                                                           */
/*  Changelog:                                                               */
/*                                                                           */
/*  v1.0 Forked from iqaudio.                                                */
/*  v1.5 Changed hard encoded values for card name and control.              */
/*  v2.0 Modified to accept command line parameters and created data         */
/*       structures for easier conversion to other uses.                     */
/*  v2.1 Additional command line parameters.                                 */
/*  v2.2 Changed volume control to allow shaping of profile via factor.      */
/*  v2.3 Tweaked default parameters.                                         */
/*  v2.4 Modified getWiringPiNum routine for efficiency.                     */
/*  v2.5 Split info parameters and added some helpful output.                */
/*  v2.6 Reworked bounds checking and error trapping.                        */
/*  v2.7 Added soft volume limits.                                           */
/*  v2.8 Added push button mute support for keyswitch.                       */
/*  v2.9 Rewrote encoder routine.                                            */
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/*  Compilation:                                                             */
/*                                                                           */
/*  Uses wiringPi, alsa and math libraries.                                  */
/*  Compile with gcc rotencvol.c -o rotencvol -lwiringPi -lasound -lm        */
/*  Also use the following flags for Raspberry Pi:                           */
/*         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp       */
/*         -ffast-math -pipe -O3                                             */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <alsa/asoundlib.h>
//#include <alsa/mixer.h>
#include <math.h>
#include <stdbool.h>

// Maximum GPIO pin number
static const int maxGPIO=35;

    /**************************************************/
    /* GPIO mapping - see http://wiringpi.com/pins/   */
    /*                                                */
    /*      +----------+----------+----------+        */
    /*      | GPIO pin | WiringPi |  Board   |        */
    /*      +----------+----------+----------+        */
    /*      |     0    |     8    |  Rev 1   |        */
    /*      |     1    |     9    |  Rev 1   |        */
    /*      |     2    |     8    |  Rev 2   |        */
    /*      |     3    |     9    |  Rev 2   |        */
    /*      |     4    |     7    |          |        */
    /*      |     7    |    11    |          |        */
    /*      |     8    |    10    |          |        */
    /*      |     9    |    13    |          |        */
    /*      |    10    |    12    |          |        */
    /*      |    11    |    14    |          |        */
    /*      |    14    |    15    |          |        */
    /*      |    15    |    16    |          |        */
    /*      |    17    |     0    |          |        */
    /*      |    21    |     2    |  Rev 1   |        */
    /*      |    22    |     3    |          |        */
    /*      |    23    |     4    |          |        */
    /*      |    24    |     5    |          |        */
    /*      |    25    |     6    |          |        */
    /*      |    27    |     2    |  Rev 2   |        */
    /*      |    28    |    17    |  Rev 2   |        */
    /*      |    29    |    18    |  Rev 2   |        */
    /*      |    30    |    19    |  Rev 2   |        */
    /*      |    31    |    20    |  Rev 2   |        */
    /*      +----------+----------+----------+        */
    /*                                                */
    /**************************************************/

static const int numGPIO = 23; // No of GPIO pins in array.
static int piGPIO[3][23] =
{
    { 0, 1, 2, 3, 4, 7, 8, 9,10,11,14,15,17,21,22,23,24,25,27,28,29,30,31 },
    { 8, 9, 8, 9, 7,11,10,13,12,14,15,16, 0, 2, 3, 4, 5, 6, 2,17,18,19,20 },
    { 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2 }
};

// Encoder state.
static volatile int encoderDirection;
static volatile int lastEncoded;
static volatile int encoded;
static volatile int busy = false;

// Button states.
static volatile int muteState = 0;
static volatile int muteStateChanged = 0;
static volatile int balState = 0;
static volatile int balStateChanged = 0;

// Data structure of command line parameters.
struct structArgs
{
    int card;       // ALSA card number.
    int control;    // Alsa control number.
    int gpioA;      // GPIO pin for vol rotary encoder.
    int gpioB;      // GPIO pin for vol rotary encoder.
    int gpioC;      // GPIO pin for mute button.
//    int gpioD;      // GPIO pin for bal rotary encoder.
//    int gpioE;      // GPIO pin for bal rotary encoder.
//    int gpioF;      // GPIO pin for bal reset button.
    int wPiPinA;    // Mapped wiringPi pin.
    int wPiPinB;    // Mapped wiringPi pin.
    int wPiPinC;    // Mapped wiringPi pin.
//    int wPiPinD;    // Mapped wiringPi pin.
//    int wPiPinE;    // Mapped wiringPi pin.
//    int wPiPinF;    // Mapped wiringPi pin.
    int initVol;    // Initial volume (%).
    int minVol;     // Minimum soft volume (%).
    int maxVol;     // Maximum soft volume (%).
    int incVol;     // No. of increments over volume range.
    float facVol;   // Volume shaping factor
    int ticDelay;   // Delay between encoder tics.
    int prOutput;   // Flag to print output.
    int prRanges;   // Flag to print ranges.
    int prDefaults; // Flag to print defaults.
    int prSet;      // Flag to print set parameters.
    int prMapping;  // Flag to print GPIO mapping.
};

// Data structure for parameters bounds.
struct boundsStruct
{
    int volumeLow;
    int volumeHigh;
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
    long index;
    float facVol;
    long volIncs;
    long softMin;
    long softMax;
    long softRange;
    long hardMin;
    long hardMax;
    long hardRange;
    long linearVol;
    long shapedVol;
    int muteState;
};

/*****************************************************************************/
/*  Set default values.                                                      */
/*****************************************************************************/

static struct structArgs cmdArgs, defaultArgs =
{
    .card = 0,          // 1st ALSA card.
    .control = 1,       // Default ALSA control.
    .gpioA = 23,        // GPIO 23 for rotary encoder.
    .gpioB = 24,        // GPIO 24 for rotary encoder.
    .gpioC = 2,         // GPIO 0 (for mute/unmute push button).
    .wPiPinA = 4,       // WiringPi number equivalent to GPIO 23.
    .wPiPinB = 5,       // WiringPi number equivalent to GPIO 24.
    .wPiPinC = 8,       // WiringPi number equivalent to GPIO 2.
    .initVol = 0,       // Mute.
    .minVol = 0,        // 0% of Maximum output level.
    .maxVol = 100,      // 100% of Maximum output level.
    .incVol = 20,       // 20 increments from 0 to 100%.
    .facVol = 1,        // Volume change rate factor.
    .ticDelay = 100,     // 100 cycles between tics.
    .prOutput = 0,      // No output printing.
    .prRanges = 0,      // No range printing.
    .prDefaults = 0,    // No defaults printing.
    .prSet = 0,         // No set parameters printing.
    .prMapping = 0      // No wiringPi map printing.
};

static struct boundsStruct paramBounds =
{
    .volumeLow = 0,           // Lower bound for initial volume.
    .volumeHigh = 100,        // Upper bound for initial volume.
    .factorLow = 0.001,       // Lower bound for volume shaping factor.
    .factorHigh = 10,         // Upper bound for volume shaping factor.
    .incLow = 1,              // Lower bound for no. of volume increments.
    .incHigh = 100,           // Upper bound for no. of volume increments.
    .delayLow = 50,           // Lower bound for delay between tics.
    .delayHigh = 1000         // Upper bound for delay between tics.
};

/*****************************************************************************/
/*  Print volume output.                                                     */
/*****************************************************************************/

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
        printf( "\t| Index | Linear Vol |  %%  | Shaped Vol |  %%  |\n" );
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

/*****************************************************************************/
/*  Get linear volume.                                                       */
/*****************************************************************************/

static long getLinearVolume( struct volStruct *volParams )
{
    long linearVol;

    // Calculate linear volume.
    if ( volParams->muteState ) linearVol = volParams->hardMin;
    else
    {
        linearVol = lroundf(( (float) volParams->index /
                              (float) volParams->volIncs *
                              (float) volParams->softRange ) +
                              (float) volParams->softMin );

        // Check volume is within soft limits.
        if ( linearVol < volParams->softMin )
                linearVol = volParams->softMin;
        else
        if ( linearVol > volParams->softMax )
                linearVol = volParams->softMax;
    }
    return linearVol;
};

/*****************************************************************************/
/*  Get volume based on shape function.                                      */
/*****************************************************************************/

static long getShapedVolume( struct volStruct *volParams )
{
    float base;
    float power;
    long shapedVol;

    // Calculate shaped volume.
    /*********************************************************/
    /*  As facVol -> 0, volume input is logarithmic.         */
    /*  As facVol -> 1, volume input is more linear          */
    /*  As facVol >> 1, volume input is more exponential.    */
    /*********************************************************/
    if ( volParams->muteState ) shapedVol = volParams->hardMin;
    else
    {
        base = (float) volParams->facVol;
        power = (float) volParams->index / (float) volParams->volIncs;
        shapedVol = lroundf(( pow( (float) volParams->facVol,
                              power ) - 1 ) / (
                              (float) volParams->facVol - 1 ) *
                              (float) volParams->softRange +
                              (float) volParams->softMin );

        // Check volume is within soft limits.
        if ( shapedVol < volParams->softMin )
                        shapedVol = volParams->softMin;
        else if ( shapedVol > volParams->softMax )
                        shapedVol = volParams->softMax;
    }
    return shapedVol;
};

/*****************************************************************************/
/*  Get soft volume bounds.                                                  */
/*****************************************************************************/

static long getSoftVol( long paramVol, long hardRange, long hardMin )
{
    float softVol;
    softVol = (float) paramVol / 100 * (float) hardRange + (float) hardMin;
    return softVol;
};

/*****************************************************************************/
/*  Get volume index.                                                        */
/*****************************************************************************/

static long getIndex( struct structArgs *cmdArgs )
{
    int index = ( cmdArgs->incVol -
                ( cmdArgs->maxVol - cmdArgs->initVol ) *
                  cmdArgs->incVol /
                ( cmdArgs->maxVol - cmdArgs->minVol ));
    return index;
};

/*****************************************************************************/
/*  GPIO interrupt call for rotary encoder.                                  */
/*****************************************************************************/

static void encoderPulse()
{
    // Check if last interrupt has finished being processed.
    if ( busy == true ) return;
    busy = true;

    // Read values at pins.
    unsigned int pinA = digitalRead( cmdArgs.wPiPinA );
    unsigned int pinB = digitalRead( cmdArgs.wPiPinB );

    // Combine A and B with last readings using bitwise operators.
    int encoded = ( pinA << 1 ) | pinB;
    int sum = ( lastEncoded << 2 ) | encoded;

    /*********************************************************************/
    /*  Quadrature encoding:                                             */
    /*                                                                   */
    /*      :   :   :   :   :   :   :   :   :                            */
    /*      :   +-------+   :   +-------+   :     +---+-------+-------+  */
    /*      :   |   :   |   :   |   :   |   :     | P |  +ve  |  -ve  |  */
    /*  A   :   |   :   |   :   |   :   |   :     | h +---+---+---+---+  */
    /*  --------+   :   +-------+   :   +-------  | a | A | B | A | B |  */
    /*      :   :   :   :   :   :   :   :   :     +---+---+---+---+---+  */
    /*      :   :   :   :   :   :   :   :   :     | 1 | 0 | 0 | 1 | 0 |  */
    /*      +-------+   :   +-------+   :   +---  | 2 | 0 | 1 | 1 | 1 |  */
    /*      |   :   |   :   |   :   |   :   |     | 3 | 1 | 1 | 0 | 1 |  */
    /*  B   |   :   |   :   |   :   |   :   |     | 4 | 1 | 0 | 0 | 0 |  */
    /*  ----+   :   +-------+   :   +-------+     +---+---+---+---+---+  */
    /*      :   :   :   :   :   :   :   :   :                            */
    /*    1 : 2 : 3 : 4 : 1 : 2 : 3 : 4 : 1 : 2   <- Phase               */
    /*      :   :   :   :   :   :   :   :   :                            */
    /*                                                                   */
    /*********************************************************************/

    if ( sum == 0b0001 ||
         sum == 0b0111 ||
         sum == 0b1110 ||
         sum == 0b1000 )
         encoderDirection = 1;
    else
    if ( sum == 0b1011 ||
         sum == 0b1101 ||
         sum == 0b0100 ||
         sum == 0b0100 )
         encoderDirection = -1;

    lastEncoded = encoded;
    busy = false;
};

/*****************************************************************************/
/*  GPIO activity call for mute button.                                      */
/*****************************************************************************/

static void buttonMute()
{
    int buttonRead = digitalRead( cmdArgs.wPiPinC );
    if ( buttonRead ) muteState = !muteState;
};

/*****************************************************************************/
/*  Program documentation:                                                   */
/*****************************************************************************/

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static const char doc[] = "Raspberry Pi volume control using rotary encoders.";
static const char args_doc[] = "rotencvol <options>";

/*****************************************************************************/
/*  Command line argument definitions.                                       */
/*****************************************************************************/

static struct argp_option options[] =
{
    { 0, 0, 0, 0, "ALSA:" },
    { "card", 'c', "Integer", 0, "ALSA card number" },
    { "control", 'd', "Integer", 0, "ALSA control number" },
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

/*****************************************************************************/
/*  Map GPIO number to WiringPi number.                                      */
/*****************************************************************************/

static int getWiringPiNum( int gpio )
{
    int loop;
    int wiringPiNum;

    wiringPiNum = -1;   // Returns -1 if no mapping found.

    for ( loop = 0; loop < numGPIO; loop++ )
        if ( gpio == piGPIO[0][loop] ) wiringPiNum = piGPIO[1][loop];

    return wiringPiNum;
};

/*****************************************************************************/
/*                                                                           */
/*  Print list of known GPIO pin mappings with wiringPi.                     */
/*  See http://wiringpi.com/pins/                                            */
/*                                                                           */
/*****************************************************************************/

static void printWiringPiMap()
{
    int loop;
    int pin;

    printf( "\nKnown GPIO pins and wiringPi mapping:\n\n" );
    printf( "\t+----------+----------+----------+\n" );
    printf( "\t| GPIO pin | WiringPi | Pi ver.  |\n" );
    printf( "\t+----------+----------+----------+\n" );

    for ( loop = 0; loop < numGPIO; loop++ )
    {
        printf( "\t|    %2d    |    %2d    ",
                piGPIO[0][loop],
                piGPIO[1][loop] );
        if ( piGPIO[2][loop] == 0 )
                printf( "|          |\n");
        else
                printf( "|    %2d    |\n", piGPIO[2][loop] );
    }
    printf( "\t+----------+----------+----------+\n\n" );
    return;
};

/*****************************************************************************/
/*  Command line argument parser.                                            */
/*****************************************************************************/

static int parse_opt( int param, char *arg, struct argp_state *state )
{
    struct structArgs *cmdArgs = state->input;
    switch ( param )
    {
        case 'c' :
            cmdArgs->card = atoi( arg );
            break;
        case 'd' :
            cmdArgs->control = atoi( arg );
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

/*****************************************************************************/
/*  Check if a parameter is within bounds.                                   */
/*****************************************************************************/

static int checkIfInBounds( float value, float lower, float upper )
{
        if (( value < lower ) || ( value > upper )) return 1;
        else return 0;
};

/*****************************************************************************/
/*  Command line parameter validity checks.                                  */
/*****************************************************************************/

static int checkParams ( struct structArgs *cmdArgs,
                  struct boundsStruct *paramBounds )
{
    static int error = 0;
    static int pinA, pinB;

    pinA = getWiringPiNum( cmdArgs->gpioA );
    pinB = getWiringPiNum( cmdArgs->gpioB );

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

/*****************************************************************************/
/*  Print default or command line set parameters values.                     */
/*****************************************************************************/

static void printParams ( struct structArgs *printList, int defSet )
{
    if ( defSet == 0 ) printf( "\nDefault parameters:\n\n" );
    else printf ( "\nSet parameters:\n\n" );

    printf ( "\tHardware name = %s.\n", printList->card );
    printf ( "\tHardware control = %s.\n", printList->control );
    printf ( "\tRotary encoder attached to GPIO pins %d", printList->gpioA );
    printf ( " & %d,\n", printList->gpioB );
    printf ( "\tMapped to WiringPi pin numbers %d", printList->wPiPinA );
    printf ( " & %d.\n", printList->wPiPinB );
    printf ( "\tInitial volume = %d%%.\n", printList->initVol );
    printf ( "\tMinimum volume = %d%%.\n", printList->minVol );
    printf ( "\tMaximum volume = %d%%.\n", printList->maxVol );
    printf ( "\tVolume increments = %d.\n", printList->incVol );
    printf ( "\tVolume factor = %g.\n", printList->facVol );
    printf ( "\tTic delay = %d.\n\n", printList->ticDelay );
};

/*****************************************************************************/
/*  Print command line parameter ranges.                                     */
/*****************************************************************************/

static void printRanges( struct boundsStruct *paramBounds )
{
    printf ( "\nCommand line parameter ranges:\n\n" );
    printf ( "\tInitial volume range (-i) = %d to %d.\n",
                    paramBounds->volumeLow,
                    paramBounds->volumeHigh );
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

/*****************************************************************************/
/*  argp parser parameter structure.                                         */
/*****************************************************************************/

static struct argp argp = { options, parse_opt, args_doc, doc };

/*****************************************************************************/
/*  Main program.                                                            */
/*****************************************************************************/

int main( int argc, char *argv[] )
{
    static int volPos = 0;  // Starting encoder position.
    static int error = 0;   // Error trapping flag.
    static char cardID[8];  // ALSA card identifier.

    static struct volStruct volParams; // Data structure for volume.
    cmdArgs = defaultArgs;  // Set command line arguments to default.


    /*************************************************************************/
    /*  ALSA control elements.
    /*************************************************************************/
    snd_ctl_t *ctl;                 // Simple control handle.
    snd_ctl_elem_id_t *id;          // Simple control element ID.
    snd_ctl_elem_value_t *control;  // Simple control element value.
    snd_ctl_elem_type_t type;       // Simple control element type.
    snd_ctl_elem_info_t *info;      // Simple control info container.


    /*************************************************************************/
    /*  Get command line arguments and check within bounds.                  */
    /*************************************************************************/
    argp_parse( &argp, argc, argv, 0, 0, &cmdArgs );
    error = checkParams( &cmdArgs, &paramBounds );
    if ( error ) return 0;


    /*************************************************************************/
    /*  Print out any information requested on command line.                 */
    /*************************************************************************/
    if ( cmdArgs.prMapping ) printWiringPiMap();
    if ( cmdArgs.prRanges ) printRanges( &paramBounds );
    if ( cmdArgs.prDefaults ) printParams( &defaultArgs, 0 );
    if ( cmdArgs.prSet ) printParams ( &cmdArgs, 1 );

    // exit program for all information except program output.
    if (( cmdArgs.prMapping ) ||
        ( cmdArgs.prDefaults ) ||
        ( cmdArgs.prSet ) ||
        ( cmdArgs.prRanges )) return 0;


    /*************************************************************************/
    /*  Initialise wiringPi.                                                 */
    /*************************************************************************/
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
    encoderDirection = 0;


    /*************************************************************************/
    /*  Set up ALSA control.                                                 */
    /*************************************************************************/
    sprintf( cardID, "hw:%i", cmdArgs.card );
    if ( snd_ctl_open( &ctl, cardID, 1 ) < 0 )
    {
        printf( "Error opening control for card %s.\n", cardID );
        return 0;
    }
    snd_ctl_elem_id_alloca( &id );
    snd_ctl_elem_id_set_numid( id, cmdArgs.control );

    snd_ctl_elem_info_alloca( &info );
    snd_ctl_elem_info_set_numid( info, cmdArgs.control );

    if ( snd_ctl_elem_info( ctl, info ) < 0 )
    {
        printf( "Error getting control info element for device %s,%i.\n",
            cardID, cmdArgs.control );
        return 0;
    }
    type = snd_ctl_elem_info_get_type( info );
    if ( type != SND_CTL_ELEM_TYPE_INTEGER )
    {
        printf( "Device has no integer control.\n" );
        return 0;
    }


    /*************************************************************************/
    /*  Get some information for selected control.                           */
    /*************************************************************************/
    volParams.hardMin = snd_ctl_elem_info_get_min( info );
    volParams.hardMax = snd_ctl_elem_info_get_max( info );


    /*************************************************************************/
    /*  Initialise the control element value container.                      */
    /*************************************************************************/
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_value_set_id( control, id );


    /*************************************************************************/
    /*  Set up volume data structure.                                        */
    /*************************************************************************/
    volParams.volIncs = cmdArgs.incVol;
    volParams.facVol = cmdArgs.facVol;
    volParams.hardRange = volParams.hardMax - volParams.hardMin;
    volParams.softMin = getSoftVol( cmdArgs.minVol,
                                    volParams.hardRange,
                                    volParams.hardMin );
    volParams.softMax = getSoftVol( cmdArgs.maxVol,
                                    volParams.hardRange,
                                    volParams.hardMin );
    volParams.softRange = volParams.softMax - volParams.softMin;
    volParams.index = getIndex( &cmdArgs );
    volParams.muteState = 0;


    /*************************************************************************/
    /*  Calculate and set initial volume.                                    */
    /*************************************************************************/
    volParams.linearVol = getLinearVolume( &volParams );
    if ( volParams.facVol == 1 ) volParams.shapedVol = volParams.linearVol;
    else volParams.shapedVol = getShapedVolume( &volParams );

    snd_ctl_elem_value_set_integer( control, 0, volParams.shapedVol );
    if ( snd_ctl_elem_write( ctl, control ) < 0 )
        printf( "Error setting L volume." );
    snd_ctl_elem_value_set_integer( control, 1, volParams.shapedVol );
    if ( snd_ctl_elem_write( ctl, control ) < 0 )
        printf( "Error setting R volume." );


    // Print starting information and header.
    if ( cmdArgs.prOutput ) printOutput( &volParams, 1 );


    /*************************************************************************/
    /*  Register interrupt functions.
    /*************************************************************************/
    wiringPiISR( cmdArgs.wPiPinA, INT_EDGE_BOTH, &encoderPulse );
    wiringPiISR( cmdArgs.wPiPinB, INT_EDGE_BOTH, &encoderPulse );
    wiringPiISR( cmdArgs.wPiPinC, INT_EDGE_BOTH, &buttonMute );


    /*************************************************************************/
    /*  Wait for GPIO activity.                                              */
    /*************************************************************************/
    while ( 1 )
    {
        if ( encoderDirection != 0 )
        {
            // Volume +.
            if (( encoderDirection > 0 ) && ( !volParams.muteState ))
            {
                volParams.index++;
                if ( volParams.index >= cmdArgs.incVol + 1 )
                            volParams.index = volParams.volIncs;
            }
            // Volume -.
            else
            if (( encoderDirection < 0 ) && ( !volParams.muteState ))
            {
                volParams.index--;
                if ( volParams.index < 0 ) volParams.index = 0;
            }
            encoderDirection = 0;


            /*****************************************************************/
            /*  Calculate and set volume.                                    */
            /*****************************************************************/
            volParams.linearVol = getLinearVolume( &volParams );
            if ( volParams.facVol == 1 ) volParams.shapedVol = volParams.linearVol;
            else volParams.shapedVol = getShapedVolume( &volParams );

            snd_ctl_elem_value_set_integer( control, 0, volParams.shapedVol );
            if ( snd_ctl_elem_write( ctl, control ) < 0 )
                printf( "Error setting L volume." );
            snd_ctl_elem_value_set_integer( control, 1, volParams.shapedVol );
            if ( snd_ctl_elem_write( ctl, control ) < 0 )
                printf( "Error setting R volume." );
            if ( cmdArgs.prOutput ) printOutput( &volParams, 0 );
        }

        /*********************************************************************/
        /*  Check and set mute state.                                        */
        /*********************************************************************/
        if ( muteStateChanged != muteState )
        {
            if ( volParams.muteState )
            {
                snd_ctl_elem_value_set_integer(
                        control, 0, volParams.hardMin );
                snd_ctl_elem_value_set_integer(
                        control, 1, volParams.hardMin );
            }
            else
            {
                snd_ctl_elem_value_set_integer(
                        control, 0, volParams.shapedVol );
                snd_ctl_elem_value_set_integer(
                        control, 1, volParams.shapedVol );
            }
            if ( snd_ctl_elem_write( ctl, control ) < 0 )
                printf( "Error setting L volume." );
            if ( snd_ctl_elem_write( ctl, control ) < 0 )
                printf( "Error setting R volume." );

            muteStateChanged = !muteStateChanged;
            volParams.muteState = muteState;
            if ( cmdArgs.prOutput ) printOutput( &volParams, 0 );
        }
        // Tic delay in mS. Adjust according to encoder.
        delay( cmdArgs.ticDelay );
    }
    // Close sockets.
    snd_ctl_close( ctl );

    return 0;
}
