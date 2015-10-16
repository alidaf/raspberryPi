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
	char pcmDevice[64];
};

int main()
{

	struct alsaStruct alsaInfo =
	{
		.deviceNum = -1,
		.subDevice = -1,
	};

	int err;
	snd_ctl_t *cardHandle;
	snd_ctl_card_info_t *cardInfo;

	while (1)
	{
		// Find next card number. If < 0 then returns 1st card.
		if (( err = snd_card_next( &alsaInfo.deviceNum )) < 0 ) // ALSA returns -1 on error.
		{
			printf( "Error: %s\n", snd_strerror( err )); // Prints ALSA error message.
			break;
		}

		if ( alsaInfo.deviceNum < 0 ) break; // ALSA returns -1 if no card found.

		{
			// Concatenate strings to get PCM device.
			sprintf( alsaInfo.pcmDevice, "hw:%i", alsaInfo.deviceNum );
			printf( "Device name = %s\n", alsaInfo.pcmDevice );
			if (( err = snd_ctl_open( &cardHandle, alsaInfo.pcmDevice, 0 )) < 0 )
			{
				printf( "Can't open card %i (%s): %s\n",
						alsaInfo.deviceNum,
						alsaInfo.pcmDevice,
						snd_strerror( err ));
				continue;
			}
		}

		// Initialise card info type.
		snd_ctl_card_info_alloca( &cardInfo );

		if (( err = snd_ctl_card_info( cardHandle, cardInfo )) < 0 )
			printf( "Error getting info for card %i (%s): %s\n",
					alsaInfo.deviceNum,
					alsaInfo.pcmDevice,
					snd_strerror( err ));
		else
			printf( "Card %i (%s) = %s\n", 
					alsaInfo.deviceNum,
					alsaInfo.pcmDevice,
					snd_ctl_card_info_get_name( cardInfo ));

		snd_ctl_close( cardHandle ); // Relinquish control.
	}

	snd_config_update_free_global(); // Close cleanly.
}
