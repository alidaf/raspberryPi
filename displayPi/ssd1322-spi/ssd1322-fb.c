//  ===========================================================================
/*
    ssd1322-fb:

    Basic SSD1322 OLED display framebuffer driver for the Raspberry Pi.

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

    gcc ssd1322-fb.c ssd1322-spi.c -Wall -o test-ssd1322 -lpigpio

    Also use the following flags for Raspberry Pi optimisation:
        -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
        -ffast-math -pipe -O3
*/
// Info -----------------------------------------------------------------------
/*
        Configuration used.

    ---------------------------------------------------------------------GND
                                            |                   |
                 RPi                        |                   |
                 1 2                        |                   |
                [o o]                       |                   |
                [o o]                       |      SSD1322      |
                [o o]                       |        1 2        |
                [o o]                       |----GND[o o]+5V----(-----------,
                [o o]                       |       [o o]SCLK---(-------,   |
                [o o]                   ,---(---SDIN[o o]       |       |   |
                [o o]                   |   |-------[o o]-------|       |   |
                [o o]GPIO23-----,       |   |-------[o o]-------|       |   |
                [o o]GPIO24-----(---,   |   |-------[o o]-------'       |   |
    ,-------MOSI[o o]           |   |   |   '--- WR#[o o]DC#--------,   |   |
    |           [o o]           |   '---(-----RESET#[o o]CS#----,   |   |   |
    |   ,---SCLK[o o]CE0----,   |       |            2 1        |   |   |   |
    |   |       [o o]       |   |       |                       |   |   |   |
    |   |       25 26       '---(-------|-----------------------'   |   |   |
    |   |                       |       |                           |   |   |
    |   |                       '-------(---------------------------'   |   |
    |   |                               |                               |   |
    |   '-------------------------------(-------------------------------'   |
    |                                   |                                   |
    '-----------------------------------'               ,-------------------'
                                                        |
    ---------------------------------------------------------------------+5V

*/

#include <stdio.h>
#include <stdint.h>
#include <pigpio.h>
#include <pthread.h>
#include <stdlib.h>

#include "ssd1322-spi.h"
//#include "beach.h"

// SSD1322 supports 480x128 but display is 256x64.
#define COLS_VIS_MIN 0x00 // Visible cols - start.
#define COLS_VIS_MAX 0x3f // Visible cols - end.
#define ROWS_VIS_MIN 0x00 // Visible rows - start.
#define ROWS_VIS_MAX 0x3f // Visible rows - end.

// Display buffer.
uint8_t *ssd1322_fb[SSD1322_DISPLAYS_MAX];

// Mutex for locking updates to FB while buffer is being written to display.
pthread_mutex_t ssd1322_display_busy;

// Structure for passing parameters through pthread void function.
struct ssd1322_display_struct
{
    uint8_t id;
};

// ----------------------------------------------------------------------------
/*
    Initialises framebuffer.
*/
// ----------------------------------------------------------------------------
int8_t ssd1322_fb_init( uint8_t id )
{
    ssd1322_fb[id] = malloc ( sizeof( uint8_t) * SSD1322_COLS * SSD1322_ROWS );
    if ( ssd1322_fb == NULL ) return -1;
    else return 0;
}

// ----------------------------------------------------------------------------
/*
    Writes framebuffer to display via SPI interface.
*/
// ----------------------------------------------------------------------------
void *ssd1322_fb_write( void *params )
{
    // Get parameters through void.
    struct ssd1322_display_struct *display = params;

    uint16_t i;
    uint8_t  data;
    uint8_t id;

    id = display->id;
    ssd1322_set_cols( id, 0, 255);
    ssd1322_set_rows( id, 0, 63 );
    ssd1322_set_write_continuous( id );

    while ( 1 )
    {
//        pthread_mutex_lock( &ssd1322_display_busy );
        for ( i = 0; i < SSD1322_COLS * SSD1322_ROWS / 2; i++ )
        {
            data = ssd1322_fb[id][i*2] << 4 | ssd1322_fb[id][i*2+1];
            ssd1322_write_data( id, data );
//            gpioDelay( 1000 );
        }
//        pthread_mutex_unlock( &ssd1322_display_busy );
//        gpioDelay( 10000 );
    }

    pthread_exit( NULL );

}

// ----------------------------------------------------------------------------
/*
    Fill the framebuffer.
*/
// ----------------------------------------------------------------------------
void ssd1322_fill_display( uint8_t id, uint8_t grey )
{
    uint16_t i;
    for ( i = 0; i < SSD1322_COLS * SSD1322_ROWS; i++ )
        ssd1322_fb[ id ][ i ] = grey;
}

// ----------------------------------------------------------------------------
/*
    Draw a pixel in the framebuffer.
*/
// ----------------------------------------------------------------------------
void ssd1322_draw_pixel( uint8_t id, uint8_t x, uint8_t y, uint8_t grey )
{
    printf( "\tx = %u, y = %u, i = %u.\n", x, y, y * SSD1322_COLS + x );
    ssd1322_fb[id][ y * SSD1322_COLS + x ] = grey;
}

// ----------------------------------------------------------------------------
/*
    Main
*/
// ----------------------------------------------------------------------------
int main()
{
    uint8_t id;
    int8_t err;
    uint8_t i;

    err = ssd1322_init( GPIO_DC, GPIO_RESET, SPI_CHANNEL, SPI_BAUD, SPI_FLAGS );

    if ( err < 0 )
    {
        printf( "Init failed!\n" );
        return -1;
    }
    else
    {
        id = err;
        printf( "Init successful.\n" );
        printf( "\tID   :%d\n", id );
        printf( "\tSPI  :%d\n", ssd1322[id]->spi_handle );
        printf( "\tDC   :%d\n", ssd1322[id]->gpio_dc );
        printf( "\tRESET:%d\n", ssd1322[id]->gpio_reset );
    }

    ssd1322_clear_display( id );

    struct ssd1322_display_struct ssd1322_display =
    {
        .id = id
    };

    err = ssd1322_fb_init( id );
    if ( err < 0 )
    {
        printf( "Couldn't allocate memory for framebuffer!\n" );
        return -1;
    }
    else
    {
        printf( "Memory successfully allocated for framebuffer.\n" );
    }

    // Create thread for writing framebuffer to display.
    pthread_mutex_init( &ssd1322_display_busy, NULL );
    pthread_t threads[1];
    pthread_create( &threads[0], NULL, ssd1322_fb_write, (void *) &ssd1322_display );

    ssd1322_fill_display( id, 0 );

    printf( "Drawing pixels - individuals.\n" );
    ssd1322_draw_pixel( id, 0, 0, 0x4 );
    ssd1322_draw_pixel( id, 255, 0, 0x4 );
    ssd1322_draw_pixel( id, 0, 63, 0x4 );
    ssd1322_draw_pixel( id, 255, 63, 0x4 );

    printf( "Drawing pixels - for loop.\n" );
    for ( i = 25; i < 36; i++ )
    {
        ssd1322_draw_pixel( id, i, i, 0xa );
    }

    while (1)
    {
    };

    ssd1322_clear_display( id );

    // Clean up threads.
    pthread_mutex_destroy( &ssd1322_display_busy );
    pthread_exit( NULL );

    return 0;
}
