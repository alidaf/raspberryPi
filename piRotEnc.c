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
//  Compile with gcc rotencvol.c -o rotencvol -lwiringPi -lasound -lm
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//  Authors:        D.Faulke    24/10/2015
//  Contributors:
//
//  Changelog:
//
//  v0.1 Rewrite of rotencvol.c

#include <stdio.h>
#include <string.h>
#include <argp.h>
#include <wiringPi.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <stdbool.h>
#include <piListAlsa.h>
#include <piInfo.h>

// Use external libraries to list mixers and find Pi version etc.
//  - listmixer, listctl, piPins.
// Add parameter options to piPins to print layout and return broadcom numbers.
// Use global struct for command line arguments.
// Void functions for any rotary encoder control, registered with wiringPi.
// Possible multithreading of each function.
// Function to open, set and close device for each encoder tic.
//  - local struct for alsa.
//  - could possibly be library?
//








// GPIO mapping - see http://wiringpi.com/pins/
/*
         +----------+----------+----------+
          | GPIO pin | WiringPi |  Board   |
          +----------+----------+----------+
          |     0    |     8    |  Rev 1   |
          |     1    |     9    |  Rev 1   |
          |     2    |     8    |  Rev 2   |
          |     3    |     9    |  Rev 2   |
          |     4    |     7    |          |
          |     7    |    11    |          |
          |     8    |    10    |          |
          |     9    |    13    |          |
          |    10    |    12    |          |
          |    11    |    14    |          |
          |    14    |    15    |          |
          |    15    |    16    |          |
          |    17    |     0    |          |
          |    21    |     2    |  Rev 1   |
          |    22    |     3    |          |
          |    23    |     4    |          |
          |    24    |     5    |          |
          |    25    |     6    |          |
          |    27    |     2    |  Rev 2   |
          |    28    |    17    |  Rev 2   |
          |    29    |    18    |  Rev 2   |
          |    30    |    19    |  Rev 2   |
          |    31    |    20    |  Rev 2   |
          +----------+----------+----------+
*/

static const int numGPIO = 23; // No of GPIO pins in array.
static int piGPIO[3][23] =
{
    { 0, 1, 2, 3, 4, 7, 8, 9,10,11,14,15,17,21,22,23,24,25,27,28,29,30,31 },
    { 8, 9, 8, 9, 7,11,10,13,12,14,15,16, 0, 2, 3, 4, 5, 6, 2,17,18,19,20 },
    { 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 2, 2, 2, 2, 2 }
};

// Encoder state.
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

// Data structure of command line parameters.
struct structArgs
{
    int card;       // ALSA card number.
    int control;    // Alsa control number.
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

// Data structure for parameters bounds.
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
    long index;
    float facVol;
    long volIncs;
    long softMin;
    long softMax;
    long softRange;
    int balance;
    long hardMin;
    long hardMax;
    long hardRange;
    long linearVol;
    long shapedVol;
    int muteState;
};


// ****************************************************************************
//  Set default values.
// ****************************************************************************

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
//  Rotary encoder for volume.
// ****************************************************************************

static void encoderPulseBal()
{
    // Check if last interrupt has finished being processed.
    if (( busyVol ) || ( busyBal )) return;
    busyBal = true;

    // Read values at pins.
    unsigned int pinA = digitalRead( cmdArgs.wPiPinD );
    unsigned int pinB = digitalRead( cmdArgs.wPiPinE );

    // Combine A and B with last readings using bitwise operators.
    int balEncoded = ( pinA << 1 ) | pinB;
    int sum = ( lastBalEncoded << 2 ) | balEncoded;

    if ( sum == 0b0001 ||
         sum == 0b0111 ||
         sum == 0b1110 ||
         sum == 0b1000 )
         balDirection = 1;
    else
    if ( sum == 0b1011 ||
         sum == 0b1101 ||
         sum == 0b0100 ||
         sum == 0b0100 )
         balDirection = -1;

    lastBalEncoded = balEncoded;
    busyBal = false;
};


// ****************************************************************************
//  GPIO activity call for balance reset button.
// ****************************************************************************

static void buttonBalance()
{
    int buttonRead = digitalRead( cmdArgs.wPiPinF );
    if ( buttonRead ) balanceStateSet = true;
};


// ****************************************************************************
//  Calculation functions.
// ****************************************************************************

// ****************************************************************************
//  Get linear volume.
// ****************************************************************************

static long getLinearVolume( struct volStruct *volParams )
{
    long linearVol;

    // Calculate linear volume.
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

    return linearVol;
};


// ****************************************************************************
//  Return shaped volume.
// ****************************************************************************

static long getShapedVolume( struct volStruct *volParams )
{
    float base;
    float power;
    long shapedVol;

    //  As facVol -> 0, volume input is logarithmic.
    //  As facVol -> 1, volume input is more linear.
    //  As facVol >> 1, volume input is more exponential.
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

    return shapedVol;
};


// ****************************************************************************
//  Return softs volume bounds.
// ****************************************************************************

static long getSoftVol( long paramVol, long hardRange, long hardMin )
{
    float softVol;
    softVol = (float) paramVol / 100 * (float) hardRange + (float) hardMin;
    return softVol;
};


// ****************************************************************************
//  Returns volume index.
// ****************************************************************************

static long getIndex( struct structArgs *cmdArgs )
{
    int index = ( cmdArgs->incVol -
                ( cmdArgs->maxVol - cmdArgs->initVol ) *
                  cmdArgs->incVol /
                ( cmdArgs->maxVol - cmdArgs->minVol ));
    return index;
};


// ****************************************************************************
//  Returns WiringPi number for given GPIO number.
// ****************************************************************************

static int getWiringPiNum( int gpio )
{
    int loop;
    int wiringPiNum;

    wiringPiNum = -1;   // Returns -1 if no mapping found.

    for ( loop = 0; loop < numGPIO; loop++ )
        if ( gpio == piGPIO[0][loop] ) wiringPiNum = piGPIO[1][loop];

    return wiringPiNum;
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


// ****************************************************************************
//  Prints list of known GPIO pin information.
// ****************************************************************************

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


// ****************************************************************************
//  Prints default or command line set parameters values.
// ****************************************************************************

static void printParams ( struct structArgs *printList, int defSet )
{
    if ( defSet == 0 ) printf( "\nDefault parameters:\n\n" );
    else printf ( "\nSet parameters:\n\n" );

    printf( "\tCard number = %i.\n", printList->card );
    printf( "\tALSA control = %i.\n", printList->control );
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
static const char doc[] = "Raspberry Pi volume control using rotary encoders.";
static const char args_doc[] = "rotencvol <options>";


// ****************************************************************************
//  Command line argument definitions.
// ****************************************************************************

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
//  Main program.
// ****************************************************************************

int main( int argc, char *argv[] )
{
    static int volPos = 0;  // Starting encoder position.
    static int error = 0;   // Error trapping flag.
    static char cardID[8];  // ALSA card identifier.

    static struct volStruct volParams; // Data structure for volume.
    cmdArgs = defaultArgs;  // Set command line arguments to default.


    // ************************************************************************
    //  ALSA control elements.
    // ************************************************************************
    snd_ctl_t *ctl;                 // Simple control handle.
    snd_ctl_elem_id_t *id;          // Simple control element ID.
    snd_ctl_elem_value_t *control;  // Simple control element value.
    snd_ctl_elem_type_t type;       // Simple control element type.
    snd_ctl_elem_info_t *info;      // Simple control info container.


    // ************************************************************************
    //  Get command line arguments and check within bounds.
    // ************************************************************************
    argp_parse( &argp, argc, argv, 0, 0, &cmdArgs );
    error = checkParams( &cmdArgs, &paramBounds );
    if ( error ) return 0;


    // ************************************************************************
    //  Print out any information requested on command line.
    // ************************************************************************
    if ( cmdArgs.prMapping ) printWiringPiMap();
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
    //  Set up ALSA control.
    // ************************************************************************
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


    // ************************************************************************
    //  Get some information for selected control.
    // ************************************************************************
    volParams.hardMin = snd_ctl_elem_info_get_min( info );
    volParams.hardMax = snd_ctl_elem_info_get_max( info );


    // ************************************************************************
    //  Initialise the control element value container.
    // ************************************************************************
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_value_set_id( control, id );


    // ************************************************************************
    //  Set up volume data structure.
    // ************************************************************************
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


    // ************************************************************************
    //  Calculate and set initial volume.
    // ************************************************************************
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
                volParams.index++;
                if ( volParams.index >= cmdArgs.incVol + 1 )
                            volParams.index = volParams.volIncs;
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

        // ********************************************************************
        //  Check and set mute state.
        // ********************************************************************
        if ( muteStateChanged )
        {
            volParams.muteState = !volParams.muteState;
            muteStateChanged = !muteStateChanged;
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

            if ( cmdArgs.prOutput ) printOutput( &volParams, 0 );
        }
        // Tic delay in mS. Adjust according to encoder.
        delay( cmdArgs.ticDelay );
    }
    // Close sockets.
    snd_ctl_close( ctl );

    return 0;
}
