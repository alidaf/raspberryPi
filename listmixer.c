/************************************************************************/
/*																		*/
/* Lists ALSA mixer devices.											*/
/*																		*/
/************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

char deviceID[16];
static struct snd_mixer_selem_regopt smixerOptions;

/************************************************************************/
/*																		*/
/* Print controls.														*/
/*																		*/
/************************************************************************/

void printControls( const char *cardName )
{

	int errNum;
	char deviceNum[16];

	snd_hctl_t *hctlHandle;				// High level control handle;
	snd_hctl_elem_t *hctlElemHandle;	// High level control element handle.
	snd_ctl_elem_id_t *elemID;			// Simple control element ID.
	snd_ctl_elem_info_t *elemInfo;		// Simple control element info container.

	snd_ctl_elem_id_alloca( &elemID );
	snd_ctl_elem_info_alloca( &elemInfo );

	// Open an empty high level control.
	errNum = snd_hctl_open( &hctlHandle, deviceID, 0 );

	// Load high level control element.
	errNum = snd_hctl_load( hctlHandle );

	printf( "\t+-----------------------------------------------+\n" );
	printf( "\t| Card: %-40s", cardName );
	printf( "|\n" );
	printf( "\t+----------+------------------------------------+\n" );
	printf( "\t| ID       | Name                               |\n" );
	printf( "\t+----------+------------------------------------+\n" );


	// For each control element.
	for ( hctlElemHandle = snd_hctl_first_elem( hctlHandle );
				hctlElemHandle;
				hctlElemHandle = snd_hctl_elem_next( hctlElemHandle ))
	{
		// Get ID of high level control element.
		snd_hctl_elem_get_id( hctlElemHandle, elemID );
//		printf( "%i\n", snd_hctl_elem_get_numid( hctlElemHandle ));

		// Concatenate strings to get card's control interface.
		sprintf( deviceNum, "%s,%i",
				 snd_hctl_name( hctlHandle ),
				 snd_hctl_elem_get_numid( hctlElemHandle ));

		printf( "\t| %-8s | %-34s |\n", deviceNum, snd_hctl_elem_get_name( hctlElemHandle ));

//		printf( "%s,%i %s\n", snd_hctl_name( hctlHandle ),
//							snd_hctl_elem_get_numid( hctlElemHandle ),
//							snd_hctl_elem_get_name( hctlElemHandle ));
	}

	printf( "\t+----------+------------------------------------+\n\n" );

	snd_hctl_close( hctlHandle );
};

/************************************************************************/
/*																		*/
/* Print mixer elements.												*/
/*																		*/
/************************************************************************/

void printMixers( const char *cardName )
{

	int errNum;
	snd_mixer_t *mixerHandle;			// Mixer handle.
	snd_mixer_selem_id_t *mixerSelemID;	// Mixer simple element channel ID.
	snd_mixer_elem_t *mixerElemHandle;	// Mixer element handle.

	snd_mixer_selem_id_alloca( &mixerSelemID );

	// Open an empty mixer.
	errNum = snd_mixer_open( &mixerHandle, 0 );

	// Attach a control to the opened mixer.
	errNum = snd_mixer_attach( mixerHandle, deviceID );

	// Register mixer simple element class.
	errNum = snd_mixer_selem_register( mixerHandle, NULL, NULL );

	// Load mixer elements.
	errNum = snd_mixer_load( mixerHandle );

	// For each mixer element.
	for ( mixerElemHandle = snd_mixer_first_elem( mixerHandle );
			mixerElemHandle;
			mixerElemHandle = snd_mixer_elem_next( mixerElemHandle ))
	{
		// Get ID of simple mixer element.
		snd_mixer_selem_get_id( mixerElemHandle, mixerSelemID );
		printf( "\t%s\n",snd_mixer_selem_id_get_name( mixerSelemID ));
	}

	snd_mixer_close( mixerHandle );
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

	int errNum;
	int body = 1;
	int controlArray[128];

	snd_ctl_t *ctlHandle;				// Simple control handle.
//	snd_pcm_info_t *pcmInfo;			// PCM generic info container.
	snd_ctl_card_info_t *cardInfo;		// Simple control card info container.
	snd_ctl_elem_type_t *elemType;		// Simple control element type;
	snd_ctl_elem_value_t *elemValue;	// Simple control element value container.

	// Initialise ALSA card and device types.
	snd_ctl_card_info_alloca( &cardInfo );
//	snd_pcm_info_alloca( &pcmInfo );
//	snd_ctl_elem_info_alloca( &elemInfo );
	snd_ctl_elem_value_alloca( &elemValue );


	// Print header.
//	printInfo( cardNumber, cardName, mixerNumber, mixerName, 0 );

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

		printControls( cardName );
//		printMixers( cardName );

		snd_ctl_close( ctlHandle );
	}

	return;
}
