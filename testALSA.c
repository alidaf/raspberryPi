#include <stdio.h>
#include <string.h>
#include "include/piALSA.h"
//#include <alsa/asoundlib.h>

// ****************************************************************************
//  Returns wiringPi pin number appropriate for board revision.
// ****************************************************************************

int main()
{
    listAllControls();
    listAllMixers();
    mixerInfo( "hw:1", "Digital" );
    return 0;
}
