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
#include "graphics.h"

// SSD1322 supports 480x128 but display is 256x64.
#define COLS_VIS_MIN 0x00 // Visible cols - start.
#define COLS_VIS_MAX 0x3f // Visible cols - end.
#define ROWS_VIS_MIN 0x00 // Visible rows - start.
#define ROWS_VIS_MAX 0x3f // Visible rows - end.

// Display buffer.
uint8_t *ssd1322_fb[SSD1322_DISPLAYS_MAX];

// Mutex for locking updates to FB while buffer is being written to display.
pthread_mutex_t ssd1322_display_busy;

uint8_t ssd1322_fb_kill = 0;

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

    while ( !ssd1322_fb_kill )
    {
        pthread_mutex_lock( &ssd1322_display_busy );
        for ( i = 0; i < SSD1322_COLS * SSD1322_ROWS / 2; i++ )
        {
            data = ssd1322_fb[id][i*2] << 4 | ssd1322_fb[id][i*2+1];
            ssd1322_write_data( id, data );
//            gpioDelay( 1000 );
        }
        pthread_mutex_unlock( &ssd1322_display_busy );
//        gpioDelay( 10000 );
    }

    printf( "Thread kill!\n" );
//    gpioDelay( 1000000 );
    pthread_exit( NULL );

}

// ----------------------------------------------------------------------------
/*
    Fill the framebuffer.
*/
// ----------------------------------------------------------------------------
void ssd1322_fb_fill_display( uint8_t id, uint8_t grey )
{
    uint16_t i;
//    pthread_mutex_lock( &ssd1322_display_busy );
    for ( i = 0; i < SSD1322_COLS * SSD1322_ROWS; i++ )
        ssd1322_fb[ id ][ i ] = grey;
//    pthread_mutex_unlock( &ssd1322_display_busy );
}

// ----------------------------------------------------------------------------
/*
    Draw a pixel in the framebuffer.
*/
// ----------------------------------------------------------------------------
void ssd1322_fb_draw_pixel( uint8_t id, uint8_t x, uint8_t y, uint8_t grey )
{
    printf( "\tx = %u, y = %u, i = %u.\n", x, y, y * SSD1322_COLS + x );
//    pthread_mutex_lock( &ssd1322_display_busy );
    ssd1322_fb[id][ y * SSD1322_COLS + x ] = grey;
//    pthread_mutex_unlock( &ssd1322_display_busy );
}

// ----------------------------------------------------------------------------
/*
    Draw a graphic in the framebuffer.
*/
// ----------------------------------------------------------------------------
int8_t ssd1322_fb_draw_image( uint8_t id, uint8_t x, uint8_t y,
                              uint16_t dx, uint8_t dy, uint8_t image[] )
{
    uint16_t i, j, k;

    if ( x + dx > SSD1322_COLS ) return -1;
    if ( y + dy > SSD1322_ROWS ) return -1;

    printf( "Drawing %ux%u graphic at %u,%u.\n", dx, dy, x, y );

    k = 0;
//    pthread_mutex_lock( &ssd1322_display_busy );
    for ( j = y; j < y + dy; j++ )
    {
        for ( i = x; i < x + dx; i++ )
        {
            ssd1322_fb[id][ j * SSD1322_COLS + i ] = image[k];
            k++;
        }
//    printf( "\n" );
    }
//    pthread_mutex_unlock( &ssd1322_display_busy );

    printf( "i = %u, j = %u, k = %u.\n", i, j, k );
    return 0;
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
    uint8_t i, j;

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

    printf( "Drawing pixels - individuals.\n" );
    ssd1322_fb_draw_pixel( id, 0, 0, 0x4 );
    ssd1322_fb_draw_pixel( id, 255, 0, 0x4 );
    ssd1322_fb_draw_pixel( id, 0, 63, 0x4 );
    ssd1322_fb_draw_pixel( id, 255, 63, 0x4 );

    printf( "Drawing graphic - fallout animation loop.\n" );
    for ( i = 0; i < 5; i++ )
    {
        for ( j = 0; j < 7; j++ )
        {
            ssd1322_fb_draw_image( id, 20, 0, 64, 64, graphics_falloutOK[j] );
            gpioDelay( 200000 );
        }
    }

    printf( "Drawing graphic - Vault-Tec symbols.\n" );
    ssd1322_fb_draw_image( id, 0, 0, 128, 64, graphics_vaultteclogo64 );
    ssd1322_fb_draw_image( id, 192, 16, 64, 32, graphics_vaultteclogo32 );
    gpioDelay( 200000 );

//    printf( "Drawing graphic - beach.\n" );
//    ssd1322_fb_fill_display( id, 0 );
//    gpioDelay( 100000 );
//    ssd1322_fb_draw_image( id, 0, 0, 256, 64, graphics_beach );
//    gpioDelay( 100000 );

    // Tell framebuffer thread to stop.
    ssd1322_fb_kill = 1;
    gpioDelay( 5000000 );

    // Gracefully stop framebuffer thread.
    pthread_join( threads[0], NULL );

    // Clean up threads.
    pthread_mutex_destroy( &ssd1322_display_busy );
//    pthread_exit( NULL );

    gpioTerminate();

    return 0;
}
