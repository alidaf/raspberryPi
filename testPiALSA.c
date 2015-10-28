#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "include/piALSA.h"
//#include <alsa/asoundlib.h>

// ****************************************************************************
//  Returns wiringPi pin number appropriate for board revision.
// ****************************************************************************

int main()
{
    int loop;
//    listAllControls();
//    listAllMixers();
    mixerInfo( "hw:0", "PCM" );
//    mixerInfo( "hw:1", "Digital" );
    for ( loop = 0; loop <= 10; loop++ )
    {
        setVolMixer(  "hw:0", "PCM", loop * 10, 0, 0.05 );
//        setVolMixer(  "hw:1", "Digital", 100, loop * -10, 0.05 );
        sleep( 2 );
    }
    setVolMixer(  "hw:0", "PCM", 50, 0, 0.05 );
//    setVolMixer(  "hw:1", "Digital", 50, 0, 0.05 );
    return 0;
}
