#include <stdio.h>
#include <string.h>
#include "include/piInfo.h"

// ****************************************************************************
//  Returns wiringPi pin number appropriate for board revision.
// ****************************************************************************

int main()
{
    unsigned int loop;
    int WPiNum, GPIONum;

    for ( loop = 1; loop < 50; loop++ )
    {
    	GPIONum = piInfoGetGPIOHead( loop );
	    if ( GPIONum > 0 )
	    {
            WPiNum = piInfoGetWPiGPIO( GPIONum );
        	printf( "Header pin = %i, GPIO = %i, wiringPi = %i.\n",
        	    loop, GPIONum, WPiNum );
        }
    }

    return 0;
}
