/********************************************************************************/
/*										*/
/* GPIO pin mapping to wiringPi numbers for Raspberry Pi.			*/
/*										*/
/* Authors:		D.Faulke	01/10/2015		v1.0		*/
/*										*/
/* Uses wiringPi library.							*/
/* Compile with gcc gpiomap.c -o gpiomap					*/
/*										*/
/********************************************************************************/

#include <stdio.h>
//#include <string.h>
//#include <stdlib.h>
//#include <errno.h>

/********************************************************************************/
/*										*/
/* Map GPIO number to WiringPi number.	 					*/
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
/* Main program.			 					*/
/*										*/
/********************************************************************************/

void main ()
{

	int loop;
	static const int elems = 23;
	int gpioPins[23] = {0,1,2,3,4,7,8,9,10,11,14,15,17,21,22,23,24,25,27,28,29,30,31};

	for (loop = 0; loop < elems; loop++)
	{
		printf("wiringPi pin for GPIO pin %d is %d\n", gpioPins[loop], getWiringPiNum(gpioPins[loop]));
	};

	// check some numbers that have no mapping - should return -1.

	printf("wiringPi pin for GPIO pin %d is %d\n", 99, getWiringPiNum(99));

}
