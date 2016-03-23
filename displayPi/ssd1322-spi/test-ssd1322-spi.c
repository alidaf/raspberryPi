//  ===========================================================================
/*
    test-ssd1322:

    Tests SSD1322 OLED display driver for the Raspberry Pi.

    Copyright 2016 Darren Faulke <darren@alidaf.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
//  ===========================================================================
/*
    Compile with:

    gcc test-ssd1322.c ssd1322-spi.c -Wall -o test-ssd1322 -lpigpio

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3
*/
// Info -----------------------------------------------------------------------
/*
        Configuration used.

                 RPi        ,---------------------------------------,
                 1 2        |           |                           |
                [o o]+5V----(-----------(---,                       |
                [o o]       |       ,---(---(-------------------,   |
                [o o]GND----'       |   |   |       16 15       |   |
                [o o]       ,-------(---(---(----CS#[o o]RESET#-'   |
                [o o]       |   ,---(---(---(----DC#[o o]WR#--------|
                [o o]       |   |   |   |   |   ,---[o o]-------,   |
                [o o]       |   |   |   '---(---+---[o o]-------+---|
                [o o]GPIO23-(---'   |       |   '---[o o]-------'   |
                [o o]GPIO24-(-------'       |       [o o]SDIN---,   |
    ,-------MOSI[o o]       |       ,-------(---SCLK[o o]       |   |
    |           [o o]       |       |       '----+5V[o o]GND----(---'
    |   ,---SCLK[o o]CE0----'       |                2 1        |
    |   |       [o o]               |              SSD1322      |
    |   |       25 26               |                           |
    |   '---------------------------'                           |
    '-----------------------------------------------------------'
*/

#include <stdio.h>
#include <stdint.h>

#include "ssd1322-spi.h"

int main()
{
    uint8_t id;
    int8_t err;

    err = ssd1322_init( GPIO_DC, GPIO_RESET, SPI_CHANNEL, SPI_BAUD, SPI_FLAGS );

    if ( err < 0 ) printf( "Init failed!\n" );
    else
    {
        id = err;
        printf( "Init successful.\n" );
        printf( "\tID   :%d\n", id );
        printf( "\tSPI  :%d\n", ssd1322[id]->spi_handle );
        printf( "\tDC   :%d\n", ssd1322[id]->gpio_dc );
        printf( "\tRESET:%d\n", ssd1322[id]->gpio_reset );
    }

    return 0;
}
