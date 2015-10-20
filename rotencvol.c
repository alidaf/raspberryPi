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

#define Version "Version 2.8"

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
/*                                                                           */
/*****************************************************************************/

/*****************************************************************************/
/*                                                                           */
/* Compilation:                                                              */
/*                                                                           */
/* Uses wiringPi, alsa and math libraries.                                   */
/* Compile with gcc rotencvol.c -o rotencvol -lwiringPi -lasound -lm         */
/* Also use the following flags for Raspberry Pi:                            */
/*        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp        */
/*        -ffast-math -pipe -O3                                              */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <argp.h>               // Command parameter parsing.
#include <wiringPi.h>           // Raspberry Pi GPIO access.
#include <alsa/asoundlib.h>     // ALSA functions.
#include <alsa/mixer.h>         // ALSA functions.
#include <math.h>               // Maths functions.

// Boolean definitions.
#define TRUE        1
#define FALSE        0

// Maximum GPIO pin number
static const int maxGPIO=35;

// Encoder state.
static volatile int encoderDirection;
static volatile int lastEncoded;
static volatile int encoded;
static volatile int inCriticalSection = FALSE;

// Button state.
static volatile int muteState = 0;
static volatile int muteStateChanged = 0;

// Data structure of command line parameters.
struct paramStruct
{
        char *cardName;         // Name of card.
        char *controlName;      // Name of control parameter.
        int gpioA;              // GPIO pin for rotary encoder.
        int gpioB;              // GPIO pin for rotary encoder.
        int gpioC;              // GPIO pin for mute button.
        int wiringPiPinA;       // Mapped wiringPi pin.
        int wiringPiPinB;       // Mapped wiringPi pin.
        int wiringPiPinC;       // Mapped wiringPi pin.
        int initVol;            // Initial volume (%).
        int minVol;             // Minimum soft volume (%).
        int maxVol;             // Maximum soft volume (%).
        int incVol;             // No. of increments over volume range.
        float facVol;           // Volume shaping factor
        int ticDelay;           // Delay between encoder tics.
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

// Data structure for debugging command line parameters.
struct infoStruct
{
        int printOutput;        // Printing output toggle.
        int printRanges;        // Parameter range printing toggle.
        int printDefaults;      // Default parameters printing toggle.
        int printSet;           // Set parameters printing toggle.
        int printMapping;       // WiringPi map printing toggle.
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
/*                                                                           */
/*  Set default values.                                                      */
/*                                                                           */
/*  Notes:                                                                   */
/*      for IQaudIODAC, .Control = "Digital".                                */
/*      currently a workaround is needed in PiCorePlayer due to both Pi and  */
/*      DAC set as default -                                                 */
/*       Delete "audio = on" from /mnt/mmcblk0p1/config.txt.                 */
/*                                                                           */
/*****************************************************************************/

// Set default values and keep a copy.
struct paramStruct setParams, defaultParams =
{
        .cardName = "default",
        .controlName = "PCM",     // Default ALSA control with no DAC.
        .gpioA = 23,              // GPIO 23 for rotary encoder.
        .gpioB = 24,              // GPIO 24 for rotary encoder.
        .gpioC = 2,               // GPIO 0 (for mute/unmute push button).
        .wiringPiPinA = 4,        // WiringPi number equivalent to GPIO 23.
        .wiringPiPinB = 5,        // WiringPi number equivalent to GPIO 24.
        .wiringPiPinC = 8,        // WiringPi number equivalent to GPIO 2.
        .initVol = 0,             // Mute.
        .minVol = 0,              // 0% of Maximum output level.
        .maxVol = 100,            // 100% of Maximum output level.
        .incVol = 20,             // 20 increments from 0 to 100%.
        .facVol = 1,              // Volume change rate factor.
        .ticDelay = 100           // 100 cycles between tics.
};

struct boundsStruct paramBounds =
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

struct infoStruct infoParams =
{
        .printOutput = 0,         // No output printing.
        .printRanges = 0,         // No range printing.
        .printDefaults = 0,       // No defaults printing.
        .printSet = 0,            // No set parameters printing.
        .printMapping = 0         // No defaults printing.
};

/*****************************************************************************/
/*                                                                           */
/* Print volume output.                                                      */
/*                                                                           */
/*****************************************************************************/

void printOutput( struct volStruct *volParams, int header )
{
        float linearP; // Percentage of hard volume range (linear).
        float shapedP; // Percentage of hard volume range (shaped)..

        linearP = 100 - ( volParams->hardMax - volParams->linearVol ) * 100 /
                                volParams->hardRange;
        shapedP = 100 - ( volParams->hardMax - volParams->shapedVol ) * 100 /
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
                if ( volParams->muteState )
                        printf( "\t| MUTE |" );
                else        printf( "\t| %4d |", volParams->index );
        }
        printf( " %10d | %3d | %10d | %3d |\n",
                volParams->linearVol,
                lroundf( linearP ),
                volParams->shapedVol,
                lroundf( shapedP ));
};

/*****************************************************************************/
/*                                                                           */
/* Get linear volume.                                                        */
/*                                                                           */
/*****************************************************************************/

long getLinearVolume( struct volStruct *volParams )
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
                else if ( linearVol > volParams->softMax )
                        linearVol = volParams->softMax;
        }

        return linearVol;
};

/*****************************************************************************/
/*                                                                           */
/* Get volume based on shape function.                                       */
/*                                                                           */
/* The value of Parameters.Factor changes the profile of the volume control: */
/* As value -> 0, volume input is logarithmic.                               */
/* As value -> 1, volume input is more linear                                */
/* As value >> 1, volume input is more exponential.                          */
/*                                                                           */
/*****************************************************************************/

long getShapedVolume( struct volStruct *volParams )
{
        float base;
        float power;
        long shapedVol;

        // Calculate shaped volume.
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
/*                                                                           */
/* Get soft volume bounds.                                                   */
/*                                                                           */
/*****************************************************************************/

long getSoftVol( long paramVol, long hardRange, long hardMin )
{
        float softVol;

        softVol = (float) paramVol / 100 * (float) hardRange + (float) hardMin;

        return softVol;
};

/*****************************************************************************/
/*                                                                           */
/* Get volume index.                                                         */
/*                                                                           */
/*****************************************************************************/

long getIndex( struct paramStruct *setParams )
{
        int index = ( setParams->incVol -
                        ( setParams->maxVol - setParams->initVol ) *
                          setParams->incVol /
                        ( setParams->maxVol - setParams->minVol ));

        return index;
};

/*****************************************************************************/
/*  GPIO activity call for rotary encoder.                                   */
/*****************************************************************************/

void encoderPulse()
{
        if ( inCriticalSection == TRUE ) return;
        inCriticalSection = TRUE;

        int MSB = digitalRead( setParams.wiringPiPinA );
        int LSB = digitalRead( setParams.wiringPiPinB );

        int encoded = ( MSB << 1 ) | LSB;
        int sum = ( lastEncoded << 2 ) | encoded;

        /*********************************************************************/
        /*  Quadrature encoding                                              */
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

        if ( sum == 0b1101 ||
             sum == 0b0100 ||
             sum == 0b0010 ||
             sum == 0b1011 ) encoderDirection = 1;

        else
        if ( sum == 0b1110 ||
             sum == 0b0111 ||
             sum == 0b0001 ||
             sum == 0b1000 ) encoderDirection = -1;

        lastEncoded = encoded;
        inCriticalSection = FALSE;
};

/*****************************************************************************/
/*                                                                           */
/*  GPIO activity call for mute button.                                      */
/*                                                                           */
/*****************************************************************************/

void buttonMute()
{
        int buttonRead = digitalRead( setParams.wiringPiPinC );

        if ( buttonRead ) muteState = !muteState;
};

/*****************************************************************************/
/*                                                                           */
/*  Program documentation:                                                   */
/*  Determines what is printed with the -V, --version or --usage parameters. */
/*                                                                           */
/*****************************************************************************/

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static char args_doc[] = "rotencvol [options]";

/*****************************************************************************/
/*                                                                           */
/*  Command line argument definitions.                                       */
/*                                                                           */
/*****************************************************************************/

static struct argp_option options[] =
{
        { 0, 0, 0, 0, "Card name:" },
        { "name", 'n', "String", 0, "Usually default." },
        { 0, 0, 0, 0, "Control name:" },
        { "control", 'o', "String", 0, "e.g. PCM/Digital/etc." },
        { 0, 0, 0, 0, "GPIO pin numbers:" },
        { "gpio1", 'a', "Integer", 0, "GPIO pin 1 for encoder." },
        { "gpio2", 'b', "Integer", 0, "GPIO pin 2 for encoder." },
        { "gpio3", 'c', "Integer", 0, "GPIO pin for mute." },
        { 0, 0, 0, 0, "Volume options:" },
        { "init", 'i', "Integer", 0, "Initial volume (% of range)." },
        { "min", 'j', "Integer", 0, "Minimum volume (% of full output)." },
        { "max", 'k', "Integer", 0, "Maximum volume (% of full output)." },
        { "inc", 'l', "Integer", 0, "No of volume increments over range." },
        { "fac", 'f', "Float", 0, "Volume profile factor." },
        { 0, 0, 0, 0, "Responsiveness:" },
        { "delay", 'd', "Integer", 0, "Delay between encoder tics (mS)." },
        { 0, 0, 0, 0, "Debugging:" },
        { "gpiomap", 'm', 0, 0, "Print GPIO/wiringPi map." },
        { "output", 'p', 0, 0, "Print program output." },
        { "default", 'q', 0, 0, "Print default parameters." },
        { "ranges", 'r', 0, 0, "Print parameter ranges." },
        { "params", 's', 0, 0, "Print set parameters." },
        { 0 }
};

/*****************************************************************************/
/*                                                                           */
/*  Map GPIO number to WiringPi number.                                      */
/*  See http://wiringpi.com/pins/                                            */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/*                     +----------+----------+----------+                    */
/*                     | GPIO pin | WiringPi |  Board   |                    */
/*                     +----------+----------+----------+                    */
/*                     |     0    |     8    |  Rev 1   |                    */
/*                     |     1    |     9    |  Rev 1   |                    */
/*                     |     2    |     8    |  Rev 2   |                    */
/*                     |     3    |     9    |  Rev 2   |                    */
/*                     |     4    |     7    |          |                    */
/*                     |     7    |    11    |          |                    */
/*                     |     8    |    10    |          |                    */
/*                     |     9    |    13    |          |                    */
/*                     |    10    |    12    |          |                    */
/*                     |    11    |    14    |          |                    */
/*                     |    14    |    15    |          |                    */
/*                     |    15    |    16    |          |                    */
/*                     |    17    |     0    |          |                    */
/*                     |    21    |     2    |  Rev 1   |                    */
/*                     |    22    |     3    |          |                    */
/*                     |    23    |     4    |          |                    */
/*                     |    24    |     5    |          |                    */
/*                     |    25    |     6    |          |                    */
/*                     |    27    |     2    |  Rev 2   |                    */
/*                     |    28    |    17    |  Rev 2   |                    */
/*                     |    29    |    18    |  Rev 2   |                    */
/*                     |    30    |    19    |  Rev 2   |                    */
/*                     |    31    |    20    |  Rev 2   |                    */
/*                     +----------+----------+----------+                    */
/*                                                                           */
/*****************************************************************************/

int getWiringPiNum( int gpio )
{
        int loop;
        int wiringPiNum;
        static const int elems = 23;
        int pins[2][23] =
        {
                { 0, 1, 2, 3, 4, 7, 8, 9,10,11,14,15,17,21,22,23,24,25,27,28,29,30,31 },
                { 8, 9, 8, 9, 7,11,10,13,12,14,15,16, 0, 2, 3, 4, 5, 6, 2,17,18,19,20 }
        };

        wiringPiNum = -1;        // Returns -1 if no mapping found.

        for ( loop = 0; loop < elems; loop++ )
        {
                if ( gpio == pins[0][loop] ) wiringPiNum = pins[1][loop];
        }

        return wiringPiNum;
};

/*****************************************************************************/
/*                                                                           */
/*  Print list of known GPIO pin mappings with wiringPi.                     */
/*  See http://wiringpi.com/pins/                                            */
/*                                                                           */
/*****************************************************************************/

void printWiringPiMap()
{
        int loop;
        int pin;

        printf( "\nKnown GPIO pins and wiringPi mapping:\n\n" );
        printf( "\t+----------+----------+\n" );
        printf( "\t| GPIO pin | WiringPi |\n" );
        printf( "\t+----------+----------+\n" );

        for ( loop = 0; loop <= maxGPIO; loop++ )
        {
                pin = getWiringPiNum( loop );
                if ( pin != -1 ) printf( "\t|    %2d    |    %2d    |\n",
                        loop, pin );
        }

        printf("\t+----------+----------+\n\n");

        return;
};

/*****************************************************************************/
/*                                                                           */
/*  Command line argument parser.                                            */
/*                                                                           */
/*****************************************************************************/

static int parse_opt( int param, char *arg, struct argp_state *state )
{
        switch ( param )
                {
                case 'n' :                        // Card name.
                        setParams.cardName = arg;
                        break;
                case 'o' :                        // Control name.
                        setParams.controlName = arg;
                        break;
                case 'a' :                        // GPIO 1 for rotary encoder.
                        setParams.gpioA = atoi (arg);
                        break;
                case 'b' :                        // GPIO 2 for rotary encoder.
                        setParams.gpioB = atoi (arg);
                        break;
                case 'c' :                        // GPIO 3 for push button.
                        setParams.gpioC = atoi (arg);
                        break;
                case 'f' :                        // Volume shape factor.
                        setParams.facVol = atof (arg);
                        break;
                case 'i' :                        // Initial volume.
                        setParams.initVol = atoi (arg);
                        break;
                case 'j' :                        // Minimum soft volume.
                        setParams.minVol = atoi (arg);
                        break;
                case 'k' :                        // Maximum soft volume.
                        setParams.maxVol = atoi (arg);
                        break;
                case 'l' :                        // No. of volume increments.
                        setParams.incVol = atoi (arg);
                        break;
                case 'd' :                        // Tic delay.
                        setParams.ticDelay = atoi (arg);
                        break;
                case 'm' :                        // Print GPIO mapping.
                        infoParams.printMapping = 1;
                        break;
                case 'p' :                        // Print program output.
                        infoParams.printOutput = 1;
                        break;
                case 'q' :                        // Print default values.
                        infoParams.printDefaults = 1;
                        break;
                case 'r' :                        // Print parameter ranges.
                        infoParams.printRanges = 1;
                        break;
                case 's' :                        // Print set ranges.
                        infoParams.printSet = 1;
                        break;
                }

        return 0;
};

/*****************************************************************************/
/*                                                                           */
/*  Check if a parameter is within bounds.                                   */
/*                                                                           */
/*****************************************************************************/

int checkIfInBounds( float value, float lower, float upper )
{
        if (( value < lower ) || ( value > upper )) return 1;
        else return 0;
};

/*****************************************************************************/
/*                                                                           */
/*  Command line parameter validity checks.                                  */
/*                                                                           */
/*****************************************************************************/

// Need to add validity checks for Name and Control for graceful exit - could
// probe for default names - check ALSA documentation.

int checkParams ( struct paramStruct *setParams, struct boundsStruct *paramBounds )
{
        int error = 0;
        int pinA, pinB;

        pinA = getWiringPiNum( setParams->gpioA );
        pinB = getWiringPiNum( setParams->gpioB );

        if (( pinA == -1 ) || ( pinB == -1 )) error = 1;

        else
        {
                setParams->wiringPiPinA = pinA;
                setParams->wiringPiPinB = pinB;
        }

        error =        ( error ||
                  checkIfInBounds( setParams->initVol,
                                     setParams->minVol,
                                     setParams->maxVol ) ||
                  checkIfInBounds( setParams->initVol,
                                   paramBounds->volumeLow,
                                   paramBounds->volumeHigh ) ||
                  checkIfInBounds( setParams->minVol,
                                   paramBounds->volumeLow,
                                   paramBounds->volumeHigh ) ||
                  checkIfInBounds( setParams->maxVol,
                                   paramBounds->volumeLow,
                                   paramBounds->volumeHigh ) ||
                  checkIfInBounds( setParams->incVol,
                                   paramBounds->incLow,
                                   paramBounds->incHigh ) ||
                  checkIfInBounds( setParams->facVol,
                                   paramBounds->factorLow,
                                   paramBounds->factorHigh ) ||
                  checkIfInBounds( setParams->ticDelay,
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
/*                                                                           */
/*  Print default or command line set parameters values.                     */
/*                                                                           */
/*****************************************************************************/

void printParams ( struct paramStruct *printList, int defSet )
{
        if ( defSet == 0 ) printf( "\nDefault parameters:\n\n" );
        else printf ( "\nSet parameters:\n\n" );

        printf ( "\tHardware name = %s.\n", printList->cardName );
        printf ( "\tHardware control = %s.\n", printList->controlName );
        printf ( "\tRotary encoder attached to GPIO pins %d", printList->gpioA );
        printf ( " & %d,\n", printList->gpioB );
        printf ( "\tMapped to WiringPi pin numbers %d", printList->wiringPiPinA );
        printf ( " & %d.\n", printList->wiringPiPinB );
        printf ( "\tInitial volume = %d%%.\n", printList->initVol );
        printf ( "\tMinimum volume = %d%%.\n", printList->minVol );
        printf ( "\tMaximum volume = %d%%.\n", printList->maxVol );
        printf ( "\tVolume increments = %d.\n", printList->incVol );
        printf ( "\tVolume factor = %g.\n", printList->facVol );
        printf ( "\tTic delay = %d.\n\n", printList->ticDelay );
};

/*****************************************************************************/
/*                                                                           */
/*  Print command line parameter ranges.                                     */
/*                                                                           */
/*****************************************************************************/

void printRanges( struct boundsStruct *paramBounds )
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
/*                                                                           */
/*  argp parser parameter structure.                                         */
/*                                                                           */
/*****************************************************************************/

static struct argp argp = { options, parse_opt };

/*****************************************************************************/
/*                                                                           */
/*  Main program.                                                            */
/*                                                                           */
/*****************************************************************************/

int main( int argc, char *argv[] )
{

        snd_mixer_t *handle;                // ALSA variables.
        snd_mixer_selem_id_t *sid;        // ALSA variables.

        int pos = 0;                        // Starting encoder position.
        int error = 0;                        // Error trapping flag.

        struct volStruct volParams;        // Data structure for volume.

        // Set default parameters and keep a copy for future use.
        setParams = defaultParams;

        // Get command line parameters and check within bounds.
        argp_parse( &argp, argc, argv, 0, 0, &setParams );
        error = checkParams( &setParams, &paramBounds );
        if ( error ) return 0;

        // Print out any information requested on command line.
        if ( infoParams.printMapping ) printWiringPiMap();
        if ( infoParams.printRanges ) printRanges( &paramBounds );
        if ( infoParams.printDefaults ) printParams( &defaultParams, 0 );
        if ( infoParams.printSet ) printParams ( &setParams, 1 );

        // exit program for all information except program output.
        if (        ( infoParams.printMapping ) ||
                ( infoParams.printDefaults ) ||
                ( infoParams.printSet ) ||
                ( infoParams.printRanges )) return 0;

        // Initialise wiringPi.
        wiringPiSetup ();

        // Set encoder pin mode.
        pinMode( setParams.wiringPiPinA, INPUT );
        pullUpDnControl( setParams.wiringPiPinA, PUD_UP );
        pinMode( setParams.wiringPiPinB, INPUT );
        pullUpDnControl( setParams.wiringPiPinB, PUD_UP );

        // Set mute button pin mode.
        pinMode( setParams.wiringPiPinC, INPUT );
        pullUpDnControl( setParams.wiringPiPinC, PUD_UP );

        // Initialise encoder direction.
        encoderDirection = 0;

        // Set up ALSA access.
        snd_mixer_open( &handle, 0 );
        snd_mixer_attach( handle, setParams.cardName );
        snd_mixer_selem_register( handle, NULL, NULL );
        snd_mixer_load( handle );

        snd_mixer_selem_id_alloca( &sid );
        snd_mixer_selem_id_set_index( sid, 0 );
        snd_mixer_selem_id_set_name( sid, setParams.controlName );
        snd_mixer_elem_t* elem = snd_mixer_find_selem( handle, sid );
        snd_mixer_selem_set_playback_switch_all(elem, !muteState);

        // Get hardware min and max values.
        snd_mixer_selem_get_playback_volume_range(
                elem,
                &volParams.hardMin,
                &volParams.hardMax );

        // Set up volume data structure.
        volParams.volIncs = setParams.incVol;
        volParams.facVol = setParams.facVol;
        volParams.hardRange = volParams.hardMax - volParams.hardMin;
        volParams.softMin = getSoftVol( setParams.minVol,
                                        volParams.hardRange,
                                        volParams.hardMin );
        volParams.softMax = getSoftVol( setParams.maxVol,
                                        volParams.hardRange,
                                        volParams.hardMin );
        volParams.softRange = volParams.softMax - volParams.softMin;
        volParams.index = getIndex( &setParams );
        volParams.muteState = 0;

        // Calculate initial volume.
        volParams.linearVol = getLinearVolume( &volParams );
        if ( volParams.facVol == 1 ) volParams.shapedVol = volParams.linearVol;
        else volParams.shapedVol = getShapedVolume( &volParams );

        // Set initial volume.
        snd_mixer_selem_set_playback_volume_all( elem, volParams.shapedVol );

        // Print starting information and header.
        if ( infoParams.printOutput ) printOutput( &volParams, 1 );

        // Monitor encoder level changes.
        wiringPiISR( setParams.wiringPiPinA, INT_EDGE_BOTH, &encoderPulse );
        wiringPiISR( setParams.wiringPiPinB, INT_EDGE_BOTH, &encoderPulse );

        // Monitor mute button.
        wiringPiISR( setParams.wiringPiPinC, INT_EDGE_BOTH, &buttonMute );

        // Wait for GPIO pins to activate.
        while ( 1 )
        {
                if ( encoderDirection != 0 )
                {
                        // Volume +.
                        if (( encoderDirection > 0 ) && ( !volParams.muteState ))
                        {
                                volParams.index++;
                                if ( volParams.index >= setParams.incVol + 1 )
                                        volParams.index = volParams.volIncs;
                        }
                        // Volume -.
                        else if (( encoderDirection < 0 ) && ( !volParams.muteState ))
                        {
                                volParams.index--;
                                if ( volParams.index < 0 ) volParams.index = 0;
                        }
                        encoderDirection = 0;

                        // Get volume.
                        volParams.linearVol = getLinearVolume( &volParams );
                        if ( volParams.facVol == 1 ) volParams.shapedVol = volParams.linearVol;
                        else volParams.shapedVol = getShapedVolume( &volParams );

                        // Set volume.
                        snd_mixer_selem_set_playback_volume_all( elem, volParams.shapedVol );

                        if ( infoParams.printOutput ) printOutput( &volParams, 0 );
                }

                if ( muteStateChanged != muteState )
                {
                        // Set mute state.
                        snd_mixer_selem_set_playback_switch_all(elem, !muteState);
                        muteStateChanged = !muteStateChanged;
                        volParams.muteState = muteState;
                        if ( infoParams.printOutput ) printOutput( &volParams, 0 );
                }

                // Tic delay in mS. Adjust according to encoder.
                delay( setParams.ticDelay );
        }

        // Close sockets.
        snd_mixer_close( handle );

        return 0;
}
