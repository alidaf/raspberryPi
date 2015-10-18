/************************************************************************/
/*																		*/
/* Lists ALSA mixer devices.											*/
/*																		*/
/************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

static struct snd_mixer_selem_regopt smixerOptions;

/************************************************************************/
/*																		*/
/* Print info routine.													*/
/*																		*/
/************************************************************************/

void printInfo( int cardNumber,
				const char *cardName,
				int mixerNumber,
				const char * mixerName,
				int body )
{

	if ( body == 0 )
	{
		printf( "\t+--------------------------+" );
		printf( "--------------------------+\n" );
		printf( "\t| Card                     |" );
		printf( " Mixer                    |\n" );
		printf( "\t+-----+--------------------+" );
		printf( "-----+--------------------+\n" );
		printf( "\t| No. | Name               |" );
		printf( " No. | Name               |\n" );
		printf( "\t+-----+--------------------+" );
		printf( "-----+--------------------+\n" );
	}
	else if ( body == 1 )
		printf( "\t| %3i | %-12s       | %3i | %-12s       |\n",
						cardNumber,
						cardName,
						mixerNumber,
						mixerName );
	else
		printf( "\t|     |                    | %3i | %-12s       |\n",
						mixerNumber,
						mixerName );
};

/************************************************************************/
/*																		*/
/* Main routine.														*/
/*																		*/
/************************************************************************/

void main()
{

	int cardNumber = -1;
	int mixerNumber = -1;
	const char *cardName = "";
	const char *mixerName = "";

	char deviceID[16];
	int errNum;

	snd_ctl_t *ctlHandle;				// Simple control handle.
	snd_hctl_t *hctlHandle;				// High level control handle;
	snd_hctl_elem_t *hctlElemHandle;	// High level control element handle.
	snd_pcm_info_t *pcmInfo;			// PCM generic info container.
	snd_ctl_card_info_t *cardInfo;		// Simple control card info container.
	snd_ctl_elem_info_t *elemInfo;		// Simple control element info container.
	snd_ctl_elem_type_t *elemType;		// Simple control element type;
	snd_ctl_elem_value_t *elemValue;	// Simple control element value container.
	snd_ctl_elem_id_t *elemID;			// Simple control element ID.

	snd_mixer_t *mixerHandle;			// Mixer handle.
	snd_mixer_selem_id_t *mixerSelemID;	// Mixer simple element channel ID.
	snd_mixer_elem_t *mixerElemHandle;	// Mixer element handle.


	// Initialise ALSA card and device types.
	snd_ctl_card_info_alloca( &cardInfo );
	snd_ctl_elem_info_alloca( &elemInfo );
	snd_pcm_info_alloca( &pcmInfo );
	snd_ctl_elem_id_alloca( &elemID );
	snd_ctl_elem_info_alloca( &elemInfo );
	snd_ctl_elem_value_alloca( &elemValue );

	snd_mixer_selem_id_alloca( &mixerSelemID );

	// Print header.
	printInfo( cardNumber, cardName, mixerNumber, mixerName, 0 );

	// For each card.
	while (1)
	{
		// Find next card number. If < 0 then returns 1st card.
		errNum = snd_card_next( &cardNumber );
		if (( errNum < 0 ) || ( cardNumber < 0 )) break;

		// Concatenate strings to get card's control interface.
		sprintf( deviceID, "hw:%i", cardNumber );

		//  Try to open card.
		errNum = snd_ctl_open( &ctlHandle, deviceID, 0 );
		if ( errNum < 0 ) continue;

		// Fill control card info element type.
		errNum = snd_ctl_card_info( ctlHandle, cardInfo );

		// Get card name.
		cardName = snd_ctl_card_info_get_name( cardInfo );

		// Find mixer names.

		// Open an empty mixer.
		errNum = snd_mixer_open( &mixerHandle, 0 );
		if ( errNum < 0 ) printf( "Error opening mixer\n" );

		// Attach a control to the opened mixer.
		errNum = snd_mixer_attach( mixerHandle, deviceID );
		if ( errNum < 0 ) printf( "Error attaching mixer\n" );

		// Register mixer simple element class.
		errNum = snd_mixer_selem_register( mixerHandle, NULL, NULL );
		if ( errNum < 0 ) printf( "Error registering mixer\n" );

		// Load mixer elements.
		errNum = snd_mixer_load( mixerHandle );
		if ( errNum < 0 ) printf( "Error loading mixer\n" );

		int body = 1;

		// For each mixer element.
		for ( mixerElemHandle = snd_mixer_first_elem( mixerHandle );
				mixerElemHandle;
				mixerElemHandle = snd_mixer_elem_next( mixerElemHandle ))
		{

			// Get ID of simple mixer element.
			snd_mixer_selem_get_id( mixerElemHandle, mixerSelemID );
			printInfo( cardNumber,
					   snd_ctl_card_info_get_name( cardInfo ),
					   snd_mixer_selem_id_get_index( mixerSelemID ),
					   snd_mixer_selem_id_get_name( mixerSelemID ), body++ );
		}

		body = 1;

		// Open an empty high level control.
		errNum = snd_hctl_open( &hctlHandle, deviceID, 0 );

		// Load high level control element.
		errNum = snd_hctl_load( hctlHandle );
		if ( errNum < 0 ) printf( "Error loading high level control\n" );



		// For each control element.
		for ( hctlElemHandle = snd_hctl_first_elem( hctlHandle );
				hctlElemHandle;
				hctlElemHandle = snd_hctl_elem_next( hctlElemHandle ))
		{

			// Get ID of high level control element.
			snd_hctl_elem_get_id( hctlElemHandle, elemID );
//			show_control_id( elemID );
			printInfo( 0,
					   "",
					   snd_hctl_elem_get_numid( hctlElemHandle ),
					   "", body++ );
		}

		snd_ctl_close( ctlHandle );
		snd_mixer_close( mixerHandle );
	}

	return;
}
