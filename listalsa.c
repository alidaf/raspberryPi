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
	int cardNum;
	int deviceNum;
	int subDeviceNum;
	const char *cardName;
	const char *deviceName;
	const char *subDeviceName;
	char deviceID[64];
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
		printf( "--------------------------+" );
		printf( "--------------------------+\n" );
		printf( "\t| Card                     |" );
		printf( " Device                   |" );
		printf( " Sub Device               |\n" );
		printf( "\t+-----+--------------------+" );
		printf( "-----+--------------------+" );
		printf( "-----+--------------------+\n" );
		printf( "\t| No. | Name               |" );
		printf( " No. | Name               |" );
		printf( " No. | Name               |\n" );
		printf( "\t+-----+--------------------+" );
		printf( "-----+--------------------+" );
		printf( "-----+--------------------+\n" );
	}
	else
		printf( "\t| %3i | %-12s       | %3i | %-12s       | %3i | %-12s       |\n",
						alsaInfo.cardNum,
						alsaInfo.cardName,
						alsaInfo.deviceNum,
						alsaInfo.deviceName,
						alsaInfo.subDeviceNum,
						alsaInfo.subDeviceName );
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

void main()
{

	struct alsaStruct alsaInfo =
	{
		.cardNum = -1,
		.deviceNum = -1,
		.subDeviceNum = -1,
		.cardName = "",
		.deviceName = "",
		.subDeviceName = "",
		.deviceID = "",
	};

	int errNum;
	int deviceCount;
	int loop;

	snd_ctl_t *cardHandle;
	snd_ctl_card_info_t *cardInfo;
	snd_pcm_info_t *deviceInfo;

	// Initialise ALSA card and device types.
	snd_ctl_card_info_alloca( &cardInfo );
	snd_pcm_info_alloca( &deviceInfo );

	// Print header.
	printInfo( alsaInfo, 0 );

	// Find next card number. If < 0 then returns 1st card.
	errNum = snd_card_next( &alsaInfo.cardNum );
	if ( errNum < 0 ) return;
	// ALSA returns -1 if no card found.
	if ( alsaInfo.cardNum < 0 ) return;

	while ( alsaInfo.cardNum >= 0 )
	{

		// Concatenate strings to get card's control interface.
		sprintf( alsaInfo.deviceID, "hw:%i", alsaInfo.cardNum );

		//  Try to open card.
		errNum = snd_ctl_open( &cardHandle, alsaInfo.deviceID, 0 );
		if ( errNum < 0 ) continue;

		// Fill snd_ctl_card_info_t.
		errNum = snd_ctl_card_info( cardHandle, cardInfo );
		if ( errNum < 0 )
		{
			snd_ctl_close( cardHandle );
//			continue;
		}
		else alsaInfo.cardName = snd_ctl_card_info_get_name( cardInfo );

		while (1)
		{

			// Find next device number. If < 0 then returns 1st device.
			errNum = snd_ctl_pcm_next_device( cardHandle, &alsaInfo.deviceNum );

			if ( alsaInfo.deviceNum < 0 ) break;

			snd_pcm_info_set_device( deviceInfo, alsaInfo.deviceNum );
			snd_pcm_info_set_subdevice( deviceInfo, 0 );
			snd_pcm_info_set_stream( deviceInfo, SND_PCM_STREAM_PLAYBACK );

			errNum = snd_ctl_pcm_info( cardHandle, deviceInfo );
			if ( errNum < 0 ) continue;

//		}

			alsaInfo.cardName = snd_ctl_card_info_get_name( cardInfo );
			alsaInfo.deviceName = snd_pcm_info_get_name( deviceInfo );

			// Get number of sub devices.
			deviceCount = snd_pcm_info_get_subdevices_count( deviceInfo );

			for ( loop = 0; loop < (int) deviceCount; loop++ )
			{
				snd_pcm_info_set_subdevice( deviceInfo, loop );
				errNum = snd_ctl_pcm_info( cardHandle, deviceInfo );
				alsaInfo.subDeviceNum = loop;
				alsaInfo.subDeviceName = snd_pcm_info_get_subdevice_name( deviceInfo );

				// Print card and device information.
				printInfo( alsaInfo, 1 );
			}

		}

	}

	snd_ctl_close( cardHandle );

	snd_config_update_free_global(); // Close cleanly.

	return;
}
