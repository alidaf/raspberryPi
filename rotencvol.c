/********************************************************************************/
/*										*/
/* Rotary encoder volume control app for Raspberry Pi.				*/
/*										*/
/* Adjusts ALSA volume based on left channel value.				*/
/* Assumes IQaudIO.com Pi-DAC volume range -103dB to 0dB			*/
/*										*/
/* Original version: 	G.Garrity 	30/08/2015 IQaudIO.com.			*/
/*			--see https://github.com/iqaudio/tools.			*/
/* Authors:		D.Faulke	02/10/2015				*/
/* Contributors:								*/
/*										*/
/********************************************************************************/

#define Version "Version 2.5"

/********************************************************************************/
/*										*/
/* Changelog:									*/
/*										*/
/* V1.0 Forked from iqaudio.							*/
/* V1.5 Changed hard encoded values for card name and control.			*/
/* V2.0 Modified to accept command line parameters and created data structures	*/
/* 	for easier conversion to other uses.					*/
/* V2.1 Additional command line parameters.					*/
/* V2.2 Changed volume control to allow shaping of profile via factor.		*/
/* V2.3 Tweaked default parameters.						*/
/* V2.4 Modified getWiringPiNum routine for efficiency.				*/
/* v2.5 Split info parameters and added some helpful output.			*/
/*										*/
/********************************************************************************/

/********************************************************************************/
/*										*/
/* Compilation:									*/
/*										*/
/* Uses wiringPi, alsa and math libraries.					*/
/* Compile with gcc rotencvol.c -o rotencvol -lwiringPi -lasound -lm		*/
/* Also use the following flags for Raspberry Pi:				*/
/*	--march=???? --mtune=???? -O2 -pipe					*/
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
	char *Name;		// Name of card.
	char *Control;		// Name of control parameter.
	int gpioA;		// GPIO pin for rotary encoder.
	int gpioB;		// GPIO pin for rotary encoder.
	int gpioC;		// GPIO pin for mute button.
	int WiringPiPinA;	// Mapped wiringPi pin.
	int WiringPiPinB;	// Mapped wiringPi pin.
	int WiringPiPinC;	// Mapped wiringPi pin.
	int Initial;		// Initial volume.
	double Factor;		// Volume shaping factor
	int Increments;		// No. of increments over volume range.
	int Delay;		// Delay between encoder tics.
};

// Data structure for parameters bounds.
struct boundsList
{
	int InitialLow;
	int InitialHigh;
	double FactorLow;
	double FactorHigh;
	int IncrementsLow;
	int IncrementsHigh;
	int DelayLow;
	int DelayHigh;
};

// Data structure for debugging command line parameters.
struct infoList
{
	int Output;		// Printing output toggle.
	int Ranges;		// Parameter range printing toggle.
	int Defaults;		// Default parameters printing toggle.
	int Set;		// Set parameters printing toggle.
	int Mapping;		// WiringPi map printing toggle.
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
	.Name = "default",
	.Control = "PCM",	// Default ALSA control with no DAC.
	.gpioA = 23,		// GPIO 23 for rotary encoder.
	.gpioB = 24,		// GPIO 24 for rotary encoder.
	.gpioC = 0,		// GPIO 0 (for mute/unmute push button).
	.WiringPiPinA = 4,	// WiringPi number equivalent to GPIO 23.
	.WiringPiPinB = 5,	// WiringPi number equivalent to GPIO 24.
	.WiringPiPinC = 8,	// WiringPi number equivalent to GPIO 24.
	.Initial = 0,		// Mute.
	.Factor = 0.1,		// Volume change rate factor.
	.Increments = 20,	// 20 increments from 0 to 100%.
	.Delay = 250		// 250 cycles between tics.
};

struct boundsList paramBounds =
{
	.InitialLow = 0,	// Lower bound for initial volume.
	.InitialHigh = 100,	// Upper bound for initial volume.
	.FactorLow = 0.001,	// Lower bound for volume shaping factor.
	.FactorHigh = 10,	// Upper bound for volume shaping factor.
	.IncrementsLow = 10,	// Lower bound for no. of volume increments.
	.IncrementsHigh = 100,	// Upper bound for no. of volume increments.
	.DelayLow = 50,		// Lower bound for delay between tics.
	.DelayHigh = 1000	// Upper bound for delay between tics.
};

struct infoList infoParams =
{
	.Output = 0,		// No output printing.
	.Ranges = 0,		// No range printing.
	.Defaults = 0,		// No defaults printing.
	.Set = 0,		// No set parameters printing.
	.Mapping = 0		// No defaults printing.
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

long getVolume (long index, long min, long max)
{
	double Power;
	double Volume;

	Power = (float)index / (float)setParams.Increments;
	Volume = (pow((float)setParams.Factor, Power) - 1) / ((float)setParams.Factor - 1) * (float)max;

	if (Volume < min)
	{
		Volume = min;
	}
	else if (Volume > max)
	{
		Volume = max;
	}

	return (long)Volume;

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

	int MSB = digitalRead(setParams.WiringPiPinA);
	int LSB = digitalRead(setParams.WiringPiPinB);

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
	{ "name", 'n', "String", 0, "Usually default - check using alsamixer" },
	{ 0, 0, 0, 0, "Control name:" },
	{ "control", 'o', "String", 0, "e.g. PCM/Digital/etc. - check using alsamixer" },
	{ 0, 0, 0, 0, "GPIO pin numbers:" },
	{ "gpio1", 'a', "Integer", 0, "GPIO number (1 of 2)." },
	{ "gpio2", 'b', "Integer", 0, "GPIO number (2 of 2)." },
	{ "gpio3", 'c', "Integer", 0, "Optional for push button - not yet implemented" },
	{ 0, 0, 0, 0, "Volume options:" },
	{ "initial", 'i', "Integer", 0, "Initial volume (%)." },
	{ "increments", 'e', "Integer", 0, "No of Volume increments over range." },
	{ 0, 0, 0, 0, "Rate of volume change:" },
	{ "factor", 'f', "Double", 0, "Volume profile factor." },
	{ 0, 0, 0, 0, "Responsiveness:" },
	{ "delay", 'd', "Integer", 0, "Delay between encoder tics" },
	{ 0, 0, 0, 0, "Debugging:" },
	{ "gpiomap", 'm', 0, 0, "Print wiringPi mapping." },
	{ "output", 'p', 0, 0, "Print output." },
	{ "default", 'q', 0, 0, "Print default settings." },
	{ "ranges", 'r', 0, 0, "Print parameter ranges." },
	{ "params", 's', 0, 0, "Print set parameter." },
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

	wiringPiNum = -1;	// Returns -1 if no mapping found

	for (loop = 0; loop < elems; loop++)
	{
		if (gpio == pins[0][loop])
		{
			wiringPiNum = pins[1][loop];
		};
	};

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

	printf("Known GPIO pins and wiringPi mapping:\n\n");
	printf("+----------+----------+\n");
	printf("| GPIO pin | WiringPi |\n");
	printf("+----------+----------+\n");

	for (loop = 0; loop <= maxGPIO; loop++)
	{
		pin = getWiringPiNum(loop);
		if (pin != -1) printf("|    %2d    |    %2d    |\n", loop, pin);
	}

	printf("+----------+----------+\n\n");

	return;
};


/********************************************************************************/
/*										*/
/* Command line argument parser. 						*/
/*										*/
/********************************************************************************/

// Need to separate out error and bound checking.
// Need to add validity checks for Name and Control for graceful exit - could
// probe for default names - check ALSA documentation.

static int parse_opt (int param, char *arg, struct argp_state *state)
{
	switch (param)
		{
		case 'n' :
			setParams.Name = arg;			// No checks for validity.
			break;
		case 'o' :
			setParams.Control = arg;		// No checks for validity.
			break;
		case 'a' :
			setParams.gpioA = atoi (arg);
			setParams.WiringPiPinA = getWiringPiNum( setParams.gpioA );
			if (setParams.WiringPiPinA == -1)
			{
				setParams.gpioA = 23;
				setParams.WiringPiPinA = 4; 	// Use default if not recognised.
				printf("Warning. GPIO pin mapping not found, Set to default. GPIO pin = %d\n\n",
					setParams.gpioA);
			}
			break;
		case 'b' :
			setParams.gpioB = atoi (arg);
			setParams.WiringPiPinB = getWiringPiNum( setParams.gpioB );
			if (setParams.WiringPiPinB == -1)
			{
				setParams.gpioB = 24;
				setParams.WiringPiPinB = 5; 	// Use default if not recognised.
				printf("Warning. GPIO pin mapping not found, Set to default. GPIO pin = %d\n\n",
					setParams.gpioB);
			}
			break;
		case 'i' :
			setParams.Initial = atoi (arg);
			if (setParams.Initial > 100)		// Check upper limit (100%).
			{
				setParams.Initial = 100;
				printf("Warning. Initial volume set to %d%%. \n\n", setParams.Initial);
			}
			else if (setParams.Initial < 0)	// Check lower limit (0%).
			{
				setParams.Initial = 0;
				printf("Warning. Initial volume set to %d%%. \n\n", setParams.Initial);
			}
			break;
		case 'e' :
			setParams.Increments = atoi (arg);
			if (setParams.Increments < 1)		// Check increments > 0.
			{
				setParams.Increments = 1;	// Essentially Mute/Unmute.
				printf("Warning. Increments set to 1, i.e. Mute/Unmute. \n\n");
			}
			if (setParams.Increments > 100)	// Arbitrary limit.
			{
				setParams.Increments = 100;
				printf("Warning. Increments set to %d.\n\n", setParams.Increments);
			}
			break;
		case 'f' :
			setParams.Factor = atof (arg);
			if (setParams.Factor <= 0)
			{
				setParams.Factor = 0.001;	// Set to default.
				printf("Warning. Factor set to %g.\n\n", setParams.Factor);
			}
			else if (setParams.Factor == 1)
			{
				setParams.Factor = 0.999999;	// 1 is asymptotic.
				printf("Warning. Factor set to %g.\n\n", setParams.Factor);
			}
			else if (setParams.Factor > 10)
			{
				setParams.Factor = 10;		// Arbitrary limit.
				printf("Warning. Factor set to %g.\n\n", setParams.Factor);
			}
			break;
		case 'd' :
			setParams.Delay = atoi (arg);
			if ((setParams.Delay < 0) || (setParams.Delay > 1000))
			{
				setParams.Delay = 250;		// Set to default if out of reasonable bounds.
				printf("Warning. Delay set to %d.\n\n", setParams.Delay);
			}
			break;
		case 'm' :
			infoParams.Mapping = 1;			// Print GPIO mapping.
			break;
		case 'p' :
			infoParams.Output = 1;			// Print program output.
			break;
		case 'q' :
			infoParams.Defaults = 1;		// Print default parameters.
			break;
		case 'r' :
			infoParams.Ranges = 1;			// Print parameter ranges.
			break;
		case 's' :
			infoParams.Set = 1;			// Print set parameters.
			break;
		}

	return 0;
};

/********************************************************************************/
/*										*/
/* Print default or command line set parameters values. 			*/
/*										*/
/********************************************************************************/

void printParams (struct paramList *printList)
{
	printf ("Hardware name = %s\n", printList->Name);
	printf ("Hardware control = %s\n", printList->Control);
	printf ("GPIO pins %d", printList->gpioA);
	printf (" & %d\n", printList->gpioB);
	printf ("Mapped to WiringPi Numbers %d", printList->WiringPiPinA);
	printf (" & %d\n", printList->WiringPiPinB);
	printf ("Initial volume = %d%%\n", printList->Initial);
	printf ("Volume factor = %g\n", printList->Factor);
	printf ("Volume increments = %d\n", printList->Increments);
	printf ("Tic delay = %d\n\n", printList->Delay);
};

/********************************************************************************/
/*										*/
/* Print command line parameter ranges.						*/
/*										*/
/********************************************************************************/

void printRanges (struct boundsList *paramBounds)
{

	printf ("Command line parameter ranges:\n\n");
	printf ("Initial volume range  = %d to %d\n", paramBounds->InitialLow, paramBounds->InitialHigh);
	printf ("Volume shaping factor = %g to %g\n", paramBounds->FactorLow, paramBounds->FactorHigh);
	printf ("Increment range = %d to %d\n", paramBounds->IncrementsLow, paramBounds->IncrementsHigh);
	printf ("Delay range  = %d to %d\n\n", paramBounds->DelayLow, paramBounds->DelayHigh);

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
	long min, max;

	snd_mixer_t *handle;
	snd_mixer_selem_id_t *sid;

	int x, mute_state;
	long i, currentVolume, logVolume;
	int indexVolume;

	// Set default parameters.

	setParams = defaultParams;

	// Initialise wiringPi.

	wiringPiSetup ();

	// Get command line parameters.

	argp_parse (&argp, argc, argv, 0, 0, &setParams);

	// Print out any information requested on command line.

	if (infoParams.Mapping) printWiringPiMap();
	if (infoParams.Ranges) printRanges(&paramBounds);
	if (infoParams.Defaults)
	{
		printf("Default parameters:\n");
		printParams(&defaultParams);
	}
	if (infoParams.Set)
	{
		printf ("Set parameters:\n");
		printParams (&setParams);
	}

	// exit program for all information except program output.

	if ((infoParams.Mapping) || (infoParams.Defaults) || (infoParams.Set) || (infoParams.Ranges))
	{
		return 0;
	}

	// Pull up is needed as encoder common is grounded.

	pinMode (setParams.WiringPiPinA, INPUT);
	pullUpDnControl (setParams.WiringPiPinA, PUD_UP);
	pinMode (setParams.WiringPiPinB, INPUT);
	pullUpDnControl (setParams.WiringPiPinB, PUD_UP);

	// Initialise encoder position.

	encoderPos = pos;

	// Set up ALSA access.

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, setParams.Name);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, setParams.Control);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

	if (infoParams.Output) printf("Returned card VOLUME  range - min: %ld, max: %ld\n", min, max);

	// Set starting volume as a percentage of maximum.

	indexVolume = (setParams.Increments * setParams.Initial / 100  );
	currentVolume = getVolume (indexVolume, min, max);
	snd_mixer_selem_set_playback_volume_all(elem, currentVolume);

	// Monitor encoder level changes.

	wiringPiISR (setParams.WiringPiPinA, INT_EDGE_BOTH, &encoderPulse);
	wiringPiISR (setParams.WiringPiPinB, INT_EDGE_BOTH, &encoderPulse);

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
				if ((indexVolume >= setParams.Increments + 1) || (currentVolume > max))
				{
					indexVolume = setParams.Increments;
					currentVolume = max;
				}
				if (encoderPos > 250)
				{
					encoderPos = 250;	// Prevent encoderPos overflowing.
					pos = 250;
				}

				currentVolume = getVolume (indexVolume, min, max);

			}
			else if (encoderPos < pos)
			{
				pos = encoderPos;
				indexVolume--;
				if ((indexVolume < 0) || (currentVolume < min))
				{
					indexVolume = 0;
					currentVolume = min;
				}
				if (encoderPos < 0)
				{
					encoderPos = 0;		// Prevent encoderPos underflowing.
					pos = 0;
				}

				currentVolume = getVolume (indexVolume, min, max);

			}

			if (x = snd_mixer_selem_set_playback_volume_all(elem, currentVolume))
			{
				printf("ERROR %d %s\n", x, snd_strerror(x));
			}
			else if (infoParams.Output) printf("Volume = %ld, Encoder pos = %d, Index = %d\n",
					currentVolume, pos, indexVolume);
		}

		// Check x times per second. Adjust according to encoder.
		delay(setParams.Delay);

	}

	// Close sockets.
	snd_mixer_close(handle);

	return 0;
}
