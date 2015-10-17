/************************************************************************/
/*																		*/
/* Lists ALSA devices.													*/
/*																		*/
/************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

struct alsaStruct
{
	int deviceNum;
	int subDevice;
	const char *deviceName;
	const char *subName;
	char deviceRef[64];
};

/************************************************************************/
/*																		*/
/* Print info routine.													*/
/*																		*/
/************************************************************************/

void printInfo( struct alsaStruct alsaInfo, int body )
{

	if ( body == 0 )
	{
		printf( "\t+--------------------------+" );
		printf( "--------------------------+\n" );
		printf( "\t| Device                   |" );
		printf( " Sub Device               |\n" );
		printf( "\t+-----+--------------------+" );
		printf( "-----+--------------------+\n" );
		printf( "\t| No. | Name               |" );
		printf( " No. | Name               |\n" );
		printf( "\t+-----+--------------------+" );
		printf( "-----+--------------------+\n" );
	}
	else
		printf( "\t| %3i | %-12s       | %3i | %-12s       |\n",
						alsaInfo.deviceNum,
						alsaInfo.deviceName,
						alsaInfo.subDevice,
						alsaInfo.subName );

};

/************************************************************************/
/*																		*/
/* Print error message.													*/
/*																		*/
/************************************************************************/

void printError( int error )
{

	printf( "Error: %s\n", snd_strerror( error ));

};

/************************************************************************/
/*																		*/
/* Main routine.														*/
/*																		*/
/************************************************************************/

int main()
{

	struct alsaStruct alsaInfo =
	{
		.deviceNum = -1,
		.subDevice = -1,
		.deviceName = "",
		.subName = "",
	};

	int errNum;
	snd_ctl_t *cardHandle;
	snd_ctl_card_info_t *cardInfo;

	// Print header.
	printInfo( alsaInfo, 0 );

	while (1)
	{
		// Find next card number. If < 0 then returns 1st card.
		errNum = snd_card_next( &alsaInfo.deviceNum );
		if ( errNum < 0 )
		{
			printError( errNum );
			break;
		}

		// ALSA returns -1 if no card found.
		if ( alsaInfo.deviceNum < 0 ) break;

		{
			// Concatenate strings to get PCM device.
			sprintf( alsaInfo.deviceRef, "hw:%i", alsaInfo.deviceNum );

			//  Open device.
			errNum = snd_ctl_open( &cardHandle, alsaInfo.deviceRef, 0 );

			if ( errNum < 0 )
			{
				printError( errNum );
				continue;
			}
		}

		// Initialise card info type.
		snd_ctl_card_info_alloca( &cardInfo );

		errNum = snd_ctl_card_info( cardHandle, cardInfo );
		if ( errNum < 0 )
			printError( errNum );
		else
		{
			alsaInfo.deviceName = snd_ctl_card_info_get_name( cardInfo );
			printInfo( alsaInfo, 1 );
		}

		snd_ctl_close( cardHandle ); // Relinquish control.
	}

	snd_config_update_free_global(); // Close cleanly.

	return 0;
}
