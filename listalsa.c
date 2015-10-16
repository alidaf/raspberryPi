/************************************************************************/
/*																		*/
/* Lists ALSA devices.													*/
/*																		*/
/************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

int main()
{

	int err;
	int cardNum = -1;
	snd_ctl_t *cardHandle;
	snd_ctl_card_info_t *cardInfo;
	char pcmDevice[64];

	while (1)
	{

		if (( err = snd_card_next( &cardNum )) < 0 ) // ALSA returns -1 on error.
		{
			printf( "Error: %s\n", snd_strerror( err )); // Prints ALSA error message.
			break;
		}

		if ( cardNum < 0 ) break; // ALSA returns -1 if no card found.

		{
			sprintf( pcmDevice, "hw:%i", cardNum ); // Concatenate strings to get PCM device.
			if (( err = snd_ctl_open( &cardHandle, pcmDevice, 0 )) < 0 )
			{
				printf( "Can't open card %i (%s): %s\n", cardNum, pcmDevice, snd_strerror( err ));
				continue;
			}
		}

		// Initialise card info type.
		snd_ctl_card_info_alloca( &cardInfo );

		if (( err = snd_ctl_card_info( cardHandle, cardInfo )) < 0 )
			printf( "Error getting info for card %i (%s): %s\n", cardNum, pcmDevice, snd_strerror( err ));
		else
			printf( "Card %i (%s) = %s\n", cardNum, pcmDevice, snd_ctl_card_info_get_name( cardInfo ));

		snd_ctl_close( cardHandle ); // Relinquish control.
	}

	snd_config_update_free_global(); // Close cleanly.
}
