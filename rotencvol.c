/********************************************************************************/
/*										*/
/* Rotary encoder volume control app for Raspberry Pi.				*/
/*										*/
/* Adjusts ALSA volume based on left channel value.				*/
/* Assumes IQaudIO.com Pi-DAC volume range -103dB to 0dB			*/
/*										*/
/* Original version: 	G.Garrity 	30/08/2015 IQaudIO.com 	v0.1 -> v1.5	*/
/*			--see https://github.com/iqaudio/tools.			*/
/* Authors:		D.Faulke	01/10/2015		v1.5 -> v2.4	*/
/* Contributors:								*/
/*										*/
/* V1.5 Changed hard encoded values for card name and control.			*/
/* V2.0 Modified to accept command line parameters and created data structures	*/
/* 	for easier conversion to other uses.					*/
/* V2.1 Additional command line parameters.					*/
/* V2.2 Changed volume control to allow shaping of profile via factor.		*/
/* V2.3 Tweaked default parameters.						*/
/* V2.4 Modified getWiringPiNum routine for efficiency.				*/
/*										*/
/* Uses wiringPi, alsa and math libraries.					*/
/* Compile with gcc rotencvol.c -o rotencvol -lwiringPi -lasound -lm		*/
/*										*/
/********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <argp.h>		// Command parameter parsing.
//#include <stdlib.h>
#include <wiringPi.h>		// Raspberry Pi GPIO access.
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
//#include <errno.h>
#include <math.h>		// Needed for power function.

// Boolean definitions.
#define TRUE	1
#define FALSE	0

// Program version
#define Version "Version 2.3"

// Encoder state.
static volatile int encoderPos;
static volatile int lastEncoded;
static volatile int encoded;
static volatile int inCriticalSection = FALSE;

// Data structure of command line parameters.
struct paramList
{
	char *Name;
	char *Control;
	int GPIO_A;
	int GPIO_B;
	int WiringPiPin_A;
	int WiringPiPin_B;
	int Initial;
	double Factor;
	int Increments;
	int Delay;
	int Debug;
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

// Set default values.
struct paramList Parameters = 
{
	.Name = "default",
	.Control = "PCM",	// Default ALSA control with no DAC.
	.GPIO_A = 23,		// GPIO 23.
	.GPIO_B = 24,		// GPIO 24.
	.WiringPiPin_A = 4,	// WiringPi number equivalent to GPIO 23.
	.WiringPiPin_B = 5,	// WiringPi number equivalent to GPIO 24.
	.Initial = 0,		// Mute.
	.Factor = 0.1,		// Volume change rate factor.
	.Increments = 20,	// 20 increments from 0 to 100%.
	.Delay = 250,		// 250 cycles between tics.
	.Debug = 0		// No debug printing.
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

	Power = (float)index / (float)Parameters.Increments;
	Volume = (pow((float)Parameters.Factor, Power) - 1) / ((float)Parameters.Factor - 1) * (float)max;

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

	int MSB = digitalRead(Parameters.WiringPiPin_A);
	int LSB = digitalRead(Parameters.WiringPiPin_B);

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
const char *argp_program_bug_address = "";
static char args_doc[] = "A program to control a sound card on the Raspberry Pi using a rotary encoder.";

/********************************************************************************/
/*										*/
/* Command line argument definitions. 						*/
/* Needs tidying up so --usage or --help is formatted better 			*/
/*										*/
/********************************************************************************/

static struct argp_option options[] =
{
	{ 0, 0, 0, 0, "Hardware options:" },
	{ "name", 'n', "String", 0, "Name of Raspberry Pi card, e.g. default/IQaudIODAC/etc." },
	{ "control", 'c', "String", 0, "Name of control, e.g. PCM/Digital/etc." },
	{ "gpio1", 'a', "Integer", 0, "GPIO number (1 of 2)." },
	{ "gpio2", 'b', "Integer", 0, "GPIO number (2 of 2)." },
	{ 0, 0, 0, 0, "Volume options:" },
	{ "initial", 'i', "Integer", 0, "Initial volume (%)." },
	{ "increments", 'e', "Integer", 0, "No of Volume increments over range, 0 < inc < 100" },
	{ 0, 0, 0, 0, "Rate of volume change:" },
	{ "factor", 'f', "Real", 0, "Volume profile factor, 0.001 <= fac >= 10, fac != 1" },
	{ 0, 0, 0, 0, "Responsiveness:" },
	{ "delay", 'd', "Integer", 0, "Delay between tics, 0 < delay < 1000." },
	{ 0, 0, 0, 0, "Debugging:" },
	{ "debug", 'z', "0/1", 0, "0 = Debug print off, 1 = Debug print on." },
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
/* Command line argument parser. 						*/
/*										*/
/********************************************************************************/

static int parse_opt (int param, char *arg, struct argp_state *state)
{
//	struct paramList *Parameters = state->input;
//	*Parameters = state->input;

	switch (param)
		{
		case 'n' :
			Parameters.Name = arg;			// No checks for validity.
			break;
		case 'c' :
			Parameters.Control = arg;		// No checks for validity.
			break;
		case 'a' :
			Parameters.GPIO_A = atoi (arg);
			Parameters.WiringPiPin_A = getWiringPiNum( Parameters.GPIO_A );
			if (Parameters.WiringPiPin_A == -1)
			{
				Parameters.GPIO_A = 23;
				Parameters.WiringPiPin_A = 4; 	// Use default if not recognised.
				printf("Warning. GPIO pin mapping not found, Set to default. GPIO pin = %d\n\n",
					Parameters.GPIO_A);
			}
			break;
		case 'b' :
			Parameters.GPIO_B = atoi (arg);
			Parameters.WiringPiPin_B = getWiringPiNum( Parameters.GPIO_B );
			if (Parameters.WiringPiPin_B == -1)
			{
				Parameters.GPIO_B = 24;
				Parameters.WiringPiPin_B = 5; 	// Use default if not recognised.
				printf("Warning. GPIO pin mapping not found, Set to default. GPIO pin = %d\n\n",
					Parameters.GPIO_B);
			}
			break;
		case 'i' :
			Parameters.Initial = atoi (arg);
			if (Parameters.Initial > 100)		// Check upper limit (100%).
			{
				Parameters.Initial = 100;
				printf("Warning. Initial volume set to %d%%. \n\n", Parameters.Initial);
			}
			else if (Parameters.Initial < 0)	// Check lower limit (0%).
			{
				Parameters.Initial = 0;
				printf("Warning. Initial volume set to %d%%. \n\n", Parameters.Initial);
			}
			break;
		case 'e' :
			Parameters.Increments = atoi (arg);
			if (Parameters.Increments < 1)		// Check increments > 0.
			{
				Parameters.Increments = 1;	// Essentially Mute/Unmute.
				printf("Warning. Increments set to 1, i.e. Mute/Unmute. \n\n");
			}
			if (Parameters.Increments > 100)	// Arbitrary limit.
			{
				Parameters.Increments = 100;
				printf("Warning. Increments set to %d.\n\n", Parameters.Increments);
			}
			break;
		case 'f' :
			Parameters.Factor = atof (arg);
			if (Parameters.Factor <= 0)
			{
				Parameters.Factor = 0.001;	// Set to default.
				printf("Warning. Factor set to %g.\n\n", Parameters.Factor);
			}
			else if (Parameters.Factor == 1)
			{
				Parameters.Factor = 0.999999;	// 1 is asymptotic.
				printf("Warning. Factor set to %g.\n\n", Parameters.Factor);
			}
			else if (Parameters.Factor > 10)
			{
				Parameters.Factor = 10;		// Arbitrary limit.
				printf("Warning. Factor set to %g.\n\n", Parameters.Factor);
			}
			break;
		case 'd' :
			Parameters.Delay = atoi (arg);
			if ((Parameters.Delay < 0) || (Parameters.Delay > 1000))
			{
				Parameters.Delay = 250;		// Set to default if out of reasonable bounds.
				printf("Warning. Delay set to %d.\n\n", Parameters.Delay);
			}
			break;
		case 'z' :
			Parameters.Debug = atoi (arg);
			if (Parameters.Debug != 0)
			{
				Parameters.Debug = 1;		// Force debug printing.
				printf("Warning. Debug printing set to on. \n\n");
			}
			break;
		}
	return 0;
};

/********************************************************************************/
/*										*/
/* Print default or command line set values. 					*/
/*										*/
/********************************************************************************/

void printParams ()
{
	printf ("Hardware name = %s\n", Parameters.Name);
	printf ("Hardware control = %s\n", Parameters.Control);
	printf ("GPIO pins %d", Parameters.GPIO_A);
	printf (" & %d\n", Parameters.GPIO_B);
	printf ("Mapped to WiringPi Numbers %d", Parameters.WiringPiPin_A);
	printf (" & %d\n", Parameters.WiringPiPin_B);
	printf ("Initial volume = %d%%\n", Parameters.Initial);
	printf ("Volume factor = %g\n", Parameters.Factor);
	printf ("Volume increments = %d\n", Parameters.Increments);
	printf ("Tic delay = %d\n", Parameters.Delay);
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

	// Initialise wiringPi.

	wiringPiSetup ();

	// Get command line parameters.

	argp_parse (&argp, argc, argv, 0, 0, &Parameters);

	// Print out values set by command line - for debugging.

	if (Parameters.Debug)
	{
		printf ("Parameters passed:\n");
		printParams ();
		printf ("\n");
	}

	// Pull up is needed as encoder common is grounded.

	pinMode (Parameters.WiringPiPin_A, INPUT);
	pullUpDnControl (Parameters.WiringPiPin_A, PUD_UP);
	pinMode (Parameters.WiringPiPin_B, INPUT);
	pullUpDnControl (Parameters.WiringPiPin_B, PUD_UP);

	// Initialise encoder position.

	encoderPos = pos;

	// Set up ALSA access.

	snd_mixer_open(&handle, 0);
	snd_mixer_attach(handle, Parameters.Name);
	snd_mixer_selem_register(handle, NULL, NULL);
	snd_mixer_load(handle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, Parameters.Control);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

	if (Parameters.Debug) printf("Returned card VOLUME  range - min: %ld, max: %ld\n", min, max);

	// Set starting volume as a percentage of maximum.

	indexVolume = (Parameters.Increments * Parameters.Initial / 100  );
	currentVolume = getVolume (indexVolume, min, max);
	snd_mixer_selem_set_playback_volume_all(elem, currentVolume);

	// Monitor encoder level changes.

	wiringPiISR (Parameters.WiringPiPin_A, INT_EDGE_BOTH, &encoderPulse);
	wiringPiISR (Parameters.WiringPiPin_B, INT_EDGE_BOTH, &encoderPulse);

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
				if ((indexVolume >= Parameters.Increments + 1) || (currentVolume > max))
				{
					indexVolume = Parameters.Increments;
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
			else if (Parameters.Debug) printf("Volume = %ld, Encoder pos = %d, Index = %d\n",
					currentVolume, pos, indexVolume);
		}

		// Check x times per second. Adjust according to encoder.
		delay(Parameters.Delay);

	}

	// Close sockets.
	snd_mixer_close(handle);

	return 0;
}
