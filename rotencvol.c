/********************************************************************************/
/* 										*/
/* Rotary encoder volume control app for Raspberry Pi.  			*/
/* 										*/
/* Original version: G.Garrity 30/08/2015 IQaudIO.com.  			*/
/* 		--see https://github.com/iqaudio/tools.  			*/
/*										*/
/* Authors: 		D.Faulke 		03/10/2015 			*/
/* Contributors: 								*/
/* 										*/
/********************************************************************************/

#define Version "Version 2.7"

/********************************************************************************/
/*										*/
/* Changelog:									*/
/*										*/
/* v1.0 Forked from iqaudio.							*/
/* v1.5 Changed hard encoded values for card name and control.			*/
/* v2.0 Modified to accept command line parameters and created data structures	*/
/* 	for easier conversion to other uses.					*/
/* v2.1 Additional command line parameters.					*/
/* v2.2 Changed volume control to allow shaping of profile via factor.		*/
/* v2.3 Tweaked default parameters.						*/
/* v2.4 Modified getWiringPiNum routine for efficiency.				*/
/* v2.5 Split info parameters and added some helpful output.			*/
/* v2.6 Reworked bounds checking and error trapping.				*/
/* v2.7 Added soft volume limits.						*/
/*										*/
/********************************************************************************/

/********************************************************************************/
/*										*/
/* Compilation:									*/
/*										*/
/* Uses wiringPi, alsa and math libraries.					*/
/* Compile with gcc rotencvol.c -o rotencvol -lwiringPi -lasound -lm		*/
/* Also use the following flags for Raspberry Pi:				*/
/*	-march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp		*/
/*	-ffast-math -pipe -O3							*/
/*										*/
/********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <argp.h>		// Command parameter parsing.
#include <wiringPi.h>		// Raspberry Pi GPIO access.
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <math.h>		// Needed for pow function.

// Boolean definitions.
#define TRUE	1
#define FALSE	0

// Maximum GPIO pin number
static const int maxGPIO=35;

// Encoder state.
static volatile int encoderPos;
static volatile int lastEncoded;
static volatile int encoded;
static volatile int inCriticalSection = FALSE;

// Data structure of command line parameters.
struct paramList
{
	char *cardName;		// Name of card.
	char *controlName;	// Name of control parameter.
	int gpioA;		// GPIO pin for rotary encoder.
	int gpioB;		// GPIO pin for rotary encoder.
	int gpioC;		// GPIO pin for mute button.
	int wiringPiPinA;	// Mapped wiringPi pin.
	int wiringPiPinB;	// Mapped wiringPi pin.
	int wiringPiPinC;	// Mapped wiringPi pin.
	int initVol;		// Initial volume (%).
	int minVol;		// Minimum soft volume (%).
	int maxVol;		// Maximum soft volume (%).
	int incVol;		// No. of increments over volume range.
	double facVol;		// Volume shaping factor
	int ticDelay;		// Delay between encoder tics.
};

// Data structure for parameters bounds.
struct boundsList
{
	int volumeLow;
	int volumeHigh;
	double factorLow;
	double factorHigh;
	int incLow;
	int incHigh;
	int delayLow;
	int delayHigh;
};

// Data structure for debugging command line parameters.
struct infoList
{
	int printOutput;	// Printing output toggle.
	int printRanges;	// Parameter range printing toggle.
	int printDefaults;	// Default parameters printing toggle.
	int printSet;		// Set parameters printing toggle.
	int printMapping;	// WiringPi map printing toggle.
};

/********************************************************************************/
/*										*/
/* Set default values.								*/
/*										*/
/* Notes:									*/
/*	for IQaudIODAC, .Control = "Digital".					*/
/*	currently a workaround is needed in PiCorePlayer due to both Pi and	*/
/*	DAC set as default. Delete "audio = on" from /mnt/mmcblk0p1/config.txt.	*/
/*										*/
/********************************************************************************/

// Set default values and keep a copy.
struct paramList setParams, defaultParams =
{
	.cardName = "default",
	.controlName = "PCM",	// Default ALSA control with no DAC.
	.gpioA = 23,		// GPIO 23 for rotary encoder.
	.gpioB = 24,		// GPIO 24 for rotary encoder.
	.gpioC = 0,		// GPIO 0 (for mute/unmute push button).
	.wiringPiPinA = 4,	// WiringPi number equivalent to GPIO 23.
	.wiringPiPinB = 5,	// WiringPi number equivalent to GPIO 24.
	.wiringPiPinC = 8,	// WiringPi number equivalent to GPIO 24.
	.initVol = 0,		// Mute.
	.minVol = 0,		// 0% of Maximum output level.
	.maxVol = 100,		// 100% of Maximum output level.
	.incVol = 20,		// 20 increments from 0 to 100%.
	.facVol = 0.1,		// Volume change rate factor.
	.ticDelay = 250		// 250 cycles between tics.
};

struct boundsList paramBounds =
{
	.volumeLow = 0,		// Lower bound for initial volume.
	.volumeHigh = 100,	// Upper bound for initial volume.
	.factorLow = 0.001,	// Lower bound for volume shaping factor.
	.factorHigh = 10,	// Upper bound for volume shaping factor.
	.incLow = 1,		// Lower bound for no. of volume increments. 1=Mute/Unmute.
	.incHigh = 100,		// Upper bound for no. of volume increments.
	.delayLow = 50,		// Lower bound for delay between tics.
	.delayHigh = 1000	// Upper bound for delay between tics.
};

struct infoList infoParams =
{
	.printOutput = 0,	// No output printing.
	.printRanges = 0,	// No range printing.
	.printDefaults = 0,	// No defaults printing.
	.printSet = 0,		// No set parameters printing.
	.printMapping = 0	// No defaults printing.
};

/********************************************************************************/
/*										*/
/* Get volume based on shape function. 	 					*/
/*										*/
/* The value of Parameters.Factor changes the profile of the volume control:	*/
/* As value -> 0, volume input is logarithmic.					*/
/* As value -> 1, volume input is more linear	 				*/
/* As value -> inf, volume is is more exponential.				*/
/* Note: A value of 1 is asymptotic!						*/
/* Note: A reasonable linear response is achieved with a value of between 0.0	*/
/*	and 0.2, i.e. 0.1.							*/
/* Note: Although a wider range is possible, extreme values less than 0.001 or	*/
/*	greater than 10 will probably have little benefit.			*/
/*										*/
/********************************************************************************/

long getVolume (long indexVol, long minVol, long maxVol, long incVol, float facVol)
{
	double power;
	double outVol;

	power = (float)indexVol / (float)incVol;
	outVol = (pow((float)facVol, power) - 1) / ((float)facVol - 1) * (float)maxVol;

	if (outVol < minVol)
	{
		outVol = minVol;
	}
	else if (outVol > maxVol)
	{
		outVol = maxVol;
	}

	return (long)outVol;

};

/********************************************************************************/
/*										*/
/* GPIO activity call.								*/
/*										*/
/*		+-------+	+-------+	+-------+	0		*/
/*			|	|	|	|	|			*/
/*		A	|	|	|	|	|			*/
/*			|	|	|	|	|			*/
/*			+-------+	+-------+	+-----	1		*/
/*										*/
/*			+-------+	+-------+	+-----	0		*/
/*			|	|	|	|	|			*/
/*		B	|	|	|	|	|			*/
/*			|	|	|	|	|			*/
/*		+-------+	+-------+	+-------+	1		*/
/*										*/
/********************************************************************************/

void encoderPulse()
{
	if ( inCriticalSection == TRUE ) return;
	inCriticalSection = TRUE;

	int MSB = digitalRead(setParams.wiringPiPinA);
	int LSB = digitalRead(setParams.wiringPiPinB);

	int encoded = (MSB << 1) | LSB;
	int sum = (lastEncoded << 2) | encoded;

	if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
	else if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;

	lastEncoded = encoded;
	inCriticalSection = FALSE;
};

/********************************************************************************/
/*										*/
/* Program documentation: 							*/
/* Determines what is printed with the -V, --version or --usage parameters. 	*/
/*										*/
/********************************************************************************/

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static char args_doc[] = "A program to control volume on the Raspberry Pi with or without a DAC using a rotary encoder.";

/********************************************************************************/
/*										*/
/* Command line argument definitions. 						*/
/* Needs tidying up so --usage or --help is formatted better 			*/
/*										*/
/********************************************************************************/

static struct argp_option options[] =
{
	{ 0, 0, 0, 0, "Card name:" },
	{ "name", 'n', "String", 0, "Usually default - check using alsamixer." },
	{ 0, 0, 0, 0, "Control name:" },
	{ "control", 'o', "String", 0, "e.g. PCM/Digital/etc. - check using alsamixer." },
	{ 0, 0, 0, 0, "GPIO pin numbers:" },
	{ "gpio1", 'a', "Integer", 0, "GPIO number (1 of 2)." },
	{ "gpio2", 'b', "Integer", 0, "GPIO number (2 of 2)." },
	{ "gpio3", 'c', "Integer", 0, "Optional for push button - not yet implemented." },
	{ 0, 0, 0, 0, "Volume options:" },
	{ "init", 'i', "Integer", 0, "Initial volume (% of range)." },
	{ "min", 'j', "Integer", 0, "Minimum volume (% of full output)." },
	{ "max", 'k', "Integer", 0, "Maximum volume (% of full output)." },
	{ "inc", 'l', "Integer", 0, "No of volume increments over range." },
	{ "fac", 'f', "Double", 0, "Volume profile factor." },
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

/********************************************************************************/
/*										*/
/* Map GPIO number to WiringPi number.						*/
/* See http://wiringpi.com/pins/						*/
/*										*/
/********************************************************************************/
/*										*/
/*			+----------+----------+-------+				*/
/*			| GPIO pin | WiringPi | Board |				*/
/*			+----------+----------+-------+				*/
/*			|     0    |     8    | Rev 1 |				*/
/*			|     1    |     9    | Rev 1 |				*/
/*			|     2    |     8    | Rev 2 |				*/
/*			|     3    |     9    | Rev 2 |				*/
/*			|     4    |     7    |       |				*/
/*			|     7    |    11    |       |				*/
/*			|     8    |    10    |       |				*/
/*			|     9    |    13    |       |				*/
/*			|    10    |    12    |       |				*/
/*			|    11    |    14    |       |				*/
/*			|    14    |    15    |       |				*/
/*			|    15    |    16    |       |				*/
/*			|    17    |     0    |       |				*/
/*			|    21    |     2    | Rev 1 |				*/
/*			|    22    |     3    |       |				*/
/*			|    23    |     4    |       |				*/
/*			|    24    |     5    |       |				*/
/*			|    25    |     6    |       |				*/
/*			|    27    |     2    | Rev 2 |				*/
/*			|    28    |    17    | Rev 2 |				*/
/*			|    29    |    18    | Rev 2 |				*/
/*			|    30    |    19    | Rev 2 |				*/
/*			|    31    |    20    | Rev 2 |				*/
/*			+----------+----------+-------+				*/
/*										*/
/********************************************************************************/

int getWiringPiNum ( int gpio )

{
	int loop;
	int wiringPiNum;
	static const int elems = 23;
	int pins[2][23] =
	{
		{ 0, 1, 2, 3, 4, 7, 8, 9,10,11,14,15,17,21,22,23,24,25,27,28,29,30,31},
		{ 8, 9, 8, 9, 7,11,10,13,12,14,15,16, 0, 2, 3, 4, 5, 6, 2,17,18,19,20}
	};

	wiringPiNum = -1;	// Returns -1 if no mapping found.

	for (loop = 0; loop < elems; loop++)
	{
		if (gpio == pins[0][loop])
		{
			wiringPiNum = pins[1][loop];
		};
	}

	return wiringPiNum;

};

/********************************************************************************/
/*										*/
/* Print list of known GPIO pin mappings with wiringPi.				*/
/* See http://wiringpi.com/pins/						*/
/*										*/
/********************************************************************************/

void printWiringPiMap ()

{
	int loop;
	int pin;

	printf("\nKnown GPIO pins and wiringPi mapping:\n\n");
	printf("\t+----------+----------+\n");
	printf("\t| GPIO pin | WiringPi |\n");
	printf("\t+----------+----------+\n");

	for (loop = 0; loop <= maxGPIO; loop++)
	{
		pin = getWiringPiNum(loop);
		if (pin != -1) printf("\t|    %2d    |    %2d    |\n", loop, pin);
	}

	printf("\t+----------+----------+\n\n");

	return;
};

/********************************************************************************/
/*										*/
/* Command line argument parser. 						*/
/*										*/
/********************************************************************************/

static int parse_opt (int param, char *arg, struct argp_state *state)
{
	switch (param)
		{
		case 'n' :				// Card name.
			setParams.cardName = arg;
			break;
		case 'o' :				// Control name.
			setParams.controlName = arg;
			break;
		case 'a' :				// GPIO 1 for rotary encoder.
			setParams.gpioA = atoi (arg);
			break;
		case 'b' :				// GPIO 2 for rotary encoder.
			setParams.gpioB = atoi (arg);
			break;
		case 'c' :				// GPIO 3 for push button.
			setParams.gpioC = atoi (arg);
			break;
		case 'f' :				// Volume shape factor.
			setParams.facVol = atof (arg);
			break;
		case 'i' :				// Initial volume.
			setParams.initVol = atoi (arg);
			break;
		case 'j' :				// Minimum soft volume.
			setParams.minVol = atoi (arg);
			break;
		case 'k' :				// Maximum soft volume.
			setParams.maxVol = atoi (arg);
			break;
		case 'l' :				// No. of volume increments.
			setParams.incVol = atoi (arg);
			break;
		case 'd' :				// Tic delay.
			setParams.ticDelay = atoi (arg);
			break;
		case 'm' :				// Print GPIO mapping.
			infoParams.printMapping = 1;
			break;
		case 'p' :				// Print program output.
			infoParams.printOutput = 1;
			break;
		case 'q' :				// Print default values.
			infoParams.printDefaults = 1;
			break;
		case 'r' :				// Print parameter ranges.
			infoParams.printRanges = 1;
			break;
		case 's' :				// Print set ranges.
			infoParams.printSet = 1;
			break;
		}

	return 0;
};

/********************************************************************************/
/*										*/
/* Check if a parameter is within bounds. 					*/
/*										*/
/********************************************************************************/

int checkIfInBounds(double value, double lower, double upper)

{

	if ((value < lower) || (value > upper)) return 1;
	else return 0;

};

/********************************************************************************/
/*										*/
/* Command line parameter validity checks. 					*/
/*										*/
/********************************************************************************/

// Need to add validity checks for Name and Control for graceful exit - could
// probe for default names - check ALSA documentation.

int checkParams (struct paramList *setParams, struct  boundsList *paramBounds)

{
	int error = 0;
	int pinA, pinB;

	pinA = getWiringPiNum( setParams->gpioA );
	pinB = getWiringPiNum( setParams->gpioB );
	if ((pinA == -1) || (pinB == -1))
	{
	 	error = 1;
	}
	else
	{
		setParams->wiringPiPinA = pinA;
		setParams->wiringPiPinB = pinB;
	}

	error =	( error ||
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

	if (setParams->facVol == 1) setParams->facVol = 0.999999;

	return error;
};

/********************************************************************************/
/*										*/
/* Print default or command line set parameters values. 			*/
/*										*/
/********************************************************************************/

void printParams (struct paramList *printList)
{
	printf ("\tHardware name = %s.\n", printList->cardName);
	printf ("\tHardware control = %s.\n", printList->controlName);
	printf ("\tRotary encoder attached to GPIO pins %d", printList->gpioA);
	printf (" & %d,\n", printList->gpioB);
	printf ("\tMapped to WiringPi pin numbers %d", printList->wiringPiPinA);
	printf (" & %d.\n", printList->wiringPiPinB);
	printf ("\tInitial volume = %d%%.\n", printList->initVol);
	printf ("\tMinimum volume = %d%%.\n", printList->minVol);
	printf ("\tMaximum volume = %d%%.\n", printList->maxVol);
	printf ("\tVolume increments = %d.\n", printList->incVol);
	printf ("\tVolume factor = %g.\n", printList->facVol);
	printf ("\tTic delay = %d.\n\n", printList->ticDelay);
};

/********************************************************************************/
/*										*/
/* Print command line parameter ranges.						*/
/*										*/
/********************************************************************************/

void printRanges (struct boundsList *paramBounds)
{
	printf ("\nCommand line parameter ranges:\n\n");
	printf ("\tInitial volume range (-i) = %d to %d.\n",
			paramBounds->volumeLow,
			paramBounds->volumeHigh);
	printf ("\tVolume shaping factor (-f) = %g to %g.\n",
			paramBounds->factorLow,
			paramBounds->factorHigh);
	printf ("\tIncrement range (-e) = %d to %d.\n",
			paramBounds->incLow,
			paramBounds->incHigh);
	printf ("\tDelay range (-d) = %d to %d.\n\n",
			paramBounds->delayLow,
			paramBounds->delayHigh);
};

/********************************************************************************/
/*										*/
/* argp parser parameter structure.	 					*/
/*										*/
/********************************************************************************/

static struct argp argp = { options, parse_opt };

/********************************************************************************/
/*										*/
/* Main program.			 					*/
/*										*/
/********************************************************************************/

int main (int argc, char *argv[])
{
	int pos = 125;
	long softMin, softMax;

	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;

	int x, mute_state;
	long i, currentVolume, logVolume;
	int indexVolume;
	int error = 0;

	// Set default parameters and keep a copy of defaults.

	setParams = defaultParams;

	// Get command line parameters and check within bounds.

	argp_parse( &argp, argc, argv, 0, 0, &setParams );
	error = checkParams( &setParams, &paramBounds );
	if(error)
	{
		printf("\nThere is something wrong with the parameters you set.\n");
		printf("Use the -m -p -q -r -s flags to check values or -? to check flags.\n\n");
		printRanges(&paramBounds);
		return 0;
	}

	// Print out any information requested on command line.

	if (infoParams.printMapping) printWiringPiMap();
	if (infoParams.printRanges) printRanges(&paramBounds);
	if (infoParams.printDefaults)
	{
		printf("\nDefault parameters:\n\n");
		printParams(&defaultParams);
	}
	if (infoParams.printSet)
	{
		printf ("\nSet parameters:\n\n");
		printParams (&setParams);
	}

	// exit program for all information except program output.

	if ((infoParams.printMapping) || (infoParams.printDefaults) || (infoParams.printSet) || (infoParams.printRanges))
	{
		return 0;
	}

	// Initialise wiringPi.

	wiringPiSetup ();

	// Pull up is needed as encoder common is grounded.

	pinMode (setParams.wiringPiPinA, INPUT);
	pullUpDnControl (setParams.wiringPiPinA, PUD_UP);
	pinMode (setParams.wiringPiPinB, INPUT);
	pullUpDnControl (setParams.wiringPiPinB, PUD_UP);

	// Initialise encoder position.

	encoderPos = pos;

	// Set up ALSA access.

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, setParams.cardName);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, setParams.controlName);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

	// Get ALSA min and max levels and set soft limits.

	snd_mixer_selem_get_playback_volume_range(elem, &softMin, &softMax);
	if (infoParams.printOutput) printf("\nCard volume range = %ld to %ld\n\n", softMin, softMax);

	indexVolume = (setParams.incVol * setParams.initVol / 100  );

	softMin = getVolume( setParams.minVol * setParams.incVol / 100, softMin, softMax, setParams.incVol, setParams.facVol );
	softMax = getVolume( setParams.maxVol * setParams.incVol / 100, softMin, softMax, setParams.incVol, setParams.facVol );
	if( infoParams.printOutput ) printf( "\nSoft limts are %d to %d\n\n", softMin, softMax );

	// Set starting volume.

	currentVolume = getVolume (indexVolume, softMin, softMax, setParams.incVol, setParams.facVol);
	snd_mixer_selem_set_playback_volume_all(elem, currentVolume);

	// Monitor encoder level changes.

	wiringPiISR( setParams.wiringPiPinA, INT_EDGE_BOTH, &encoderPulse );
	wiringPiISR( setParams.wiringPiPinB, INT_EDGE_BOTH, &encoderPulse );

	// Wait for GPIO pins to activate.

	while (1)
	{
		if (encoderPos != pos)
		{
			// Find encoder direction and adjust volume.
			if (encoderPos > pos)
			{
				pos = encoderPos;
				indexVolume++;
				if ((indexVolume >= setParams.incVol + 1) || (currentVolume > softMax))
				{
					indexVolume = setParams.incVol;
					currentVolume = softMax;
				}
				if (encoderPos > 250)
				{
					encoderPos = 250;	// Prevent encoderPos overflowing.
					pos = 250;
				}
				currentVolume = getVolume( indexVolume, softMin, softMax, setParams.incVol, setParams.facVol );
			}
			else if (encoderPos < pos)
			{
				pos = encoderPos;
				indexVolume--;
				if ((indexVolume < 0) || (currentVolume < softMin))
				{
					indexVolume = 0;
					currentVolume = softMin;
				}
				if (encoderPos < 0)
				{
					encoderPos = 0;		// Prevent encoderPos underflowing.
					pos = 0;
				}
				currentVolume = getVolume( indexVolume, softMin, softMax, setParams.incVol, setParams.facVol );
			}
			if (x = snd_mixer_selem_set_playback_volume_all(elem, currentVolume))
			{
				printf("ERROR %d %s\n", x, snd_strerror(x));
			}
			else if (infoParams.printOutput) printf("Volume = %ld, Encoder pos = %d, Index = %d\n",
					currentVolume, pos, indexVolume);
		}

		// Tic delay in mS. Adjust according to encoder.
		delay(setParams.ticDelay);

	}

	// Close sockets.
	snd_mixer_close(handle);

	return 0;
}
