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

#include "ssd1322-spi.h"
#include "beach.h"

// SSD1322 supports 480x128 but display is 256x64.
#define COLS_VIS_MIN 0x00 // Visible cols - start.
#define COLS_VIS_MAX 0x3f // Visible cols - end.
#define ROWS_VIS_MIN 0x00 // Visible rows - start.
#define ROWS_VIS_MAX 0x3f // Visible rows - end.

// Display buffer.
uint8_t ssd1322_buffer[SSD1322_COLS_MAX * SSD1322_ROWS_MAX / 2];

// ----------------------------------------------------------------------------
/*
    Display test - checkerboard pattern.
*/
// ----------------------------------------------------------------------------
void test_checkerboard( uint8_t id )
{
    uint8_t row, col;

    ssd1322_clear_display( id );
    ssd1322_set_cols( id, COLS_VIS_MIN, COLS_VIS_MAX  );
    ssd1322_set_rows( id, ROWS_VIS_MIN, ROWS_VIS_MAX );
    ssd1322_set_write_continuous( id );
    for ( row = 0; row <= ROWS_VIS_MAX; row++ )
    {
        for ( col = 0; col <= COLS_VIS_MAX; col++ )
        {
            ssd1322_write_data( id, 0xf0 );
            ssd1322_write_data( id, 0xf0 );
        }
        for ( col = 0; col < COLS_VIS_MAX; col++ )
        {
            ssd1322_write_data( id, 0x0f );
            ssd1322_write_data( id, 0x0f );
        }
    }

    gpioDelay( 1000000 );
}

// ----------------------------------------------------------------------------
/*
    Fills a block with a greyscale.
*/
// ----------------------------------------------------------------------------
void fill_block( uint8_t id,
                 uint8_t x, uint8_t y,
                 uint8_t dx, uint8_t dy,
                 uint8_t grey )
{
    uint8_t i, j;
    ssd1322_set_cols( id, x, x + dx );
    ssd1322_set_rows( id, y, y + dy );
    ssd1322_set_write_continuous( id );

    for ( j = 0; j < ( dy / 2 + 1 ); j++ )
    {
        for ( i = 0; i < ( dx + 1 ); i++ )
        {
            // Need to replace with draw_pixel.
            ssd1322_write_data( id, grey );
            ssd1322_write_data( id, grey );
        }
    }
}

// ----------------------------------------------------------------------------
/*
    Display test - display greyscales.
*/
// ----------------------------------------------------------------------------
void test_greyscales( uint8_t id )
{

    uint8_t i;

    ssd1322_clear_display( id );
    for ( i = 0; i < 16; i++ )
    {
        fill_block( id, i * 16, 0, 16, 32, i );
        fill_block( id, i * 16, 32, 16, 32, 15 - i );
    }
    gpioDelay( 1000000 );
}

// ----------------------------------------------------------------------------
/*
    Sets the RAM address for position x,y.
*/
// ----------------------------------------------------------------------------
void ssd1322_gotoXY( uint8_t id, uint8_t x, uint8_t y )
{

    uint8_t col, row;

    col = x;
    row = y;
    ssd1322_set_cols( id, col, col + 1 );
    ssd1322_set_rows( id, row, row + 1 );
}

// ----------------------------------------------------------------------------
/*
    Display test - draw a pixel.
    Display columns are grouped into 4s, 4 bits per column. Hence, 4 columns
    is 2 bytes. The pixel to draw should be masked with the existing buffer.
*/
// ----------------------------------------------------------------------------
void ssd1322_draw_pixel( uint8_t id, uint8_t x, uint8_t y, uint8_t grey )
{

    uint8_t pixel[2] = { 0, 0 };

    printf( "x       = %u.\n", x );
    printf( "x mod 4 = %u.\n", x%4 );
    printf( "x norm  = %u.\n", x/4*4 );

    switch ( x % 4 )
    {
        case 0: pixel[0] = ( grey << 4 ) | ( pixel[0] & 0x0f );
                break;
        case 1: pixel[0] = ( grey ) | ( pixel[0] & 0xf0 );
                break;
        case 2: pixel[1] = ( grey << 4 ) | ( pixel[1] & 0x0f );
                break;
        case 3: pixel[1] = ( grey ) | ( pixel[1] & 0xf0 );
                break;
    }
//    ssd1322_gotoXY( id, x, y );
    ssd1322_set_cols( id, x/4*4, x/4*4 );
    ssd1322_set_rows( id, y, y );
    ssd1322_set_write_continuous( id );
    printf( "Pixel bytes: 0x%02x:0x%02x.\n", pixel[0], pixel[1] );
    ssd1322_write_data( id, pixel[0] );
    ssd1322_write_data( id, pixel[1] );
}

// ----------------------------------------------------------------------------
/*
    Display test - draws a rectangle using ssd1322_draw_pixel function.
*/
// ----------------------------------------------------------------------------
void test_draw_pixel( uint8_t id )
{

    uint8_t x, y;

    for ( y = 48; y < 63; y++ )
        for ( x = 48; x < 64; x++ )
            ssd1322_draw_pixel( id, x, y, 0x4 );
}

// ----------------------------------------------------------------------------
/*
    Display test - Continuously streams pixels to the display.
*/
// ----------------------------------------------------------------------------
void test_stream_pixel( uint8_t id )
{

    uint16_t i;
//    uint8_t grey;

//    uint8_t x, y;
//    uint8_t rows, cols;

    ssd1322_clear_display( id );
//    x = 32;
//    y = 16;
//    cols = 32;
//    rows = 32;

    ssd1322_set_cols( id, 0, 255);
    ssd1322_set_rows( id, 20, 63 );
    ssd1322_set_write_continuous( id );
    for ( i = 0; i < 64; i++ )
    {
        ssd1322_write_data( id, 0xaa );
        ssd1322_write_data( id, 0x00 );
    }


    ssd1322_set_cols( id, 32, 63);
    ssd1322_set_rows( id, 16, 63 );
    ssd1322_set_write_continuous( id );
    for ( i = 0; i < 8; i++ )
    {
        ssd1322_write_data( id, 0x00 );
        ssd1322_write_data( id, 0xaa );
    }
}

// ----------------------------------------------------------------------------
/*
    Display test - display 256x64 4-bit image.
*/
// ----------------------------------------------------------------------------
void test_draw_image( uint8_t id, uint8_t image[8192] )
{
    ssd1322_clear_display( id );
    ssd1322_set_cols( id, COLS_VIS_MIN, COLS_VIS_MAX );
    ssd1322_set_rows( id, ROWS_VIS_MIN, ROWS_VIS_MAX );
    ssd1322_set_write_continuous( id );
    ssd1322_write_stream( id, image, 8192 );
}

// ----------------------------------------------------------------------------
/*
    Display test - 256x64 4bpp image.
*/
// ----------------------------------------------------------------------------
void test_load_image( uint8_t id )
{
    uint16_t i;
    uint8_t  image[8192];

    for ( i = 0; i < 8192; i++ )
    {
        printf( "%u: %u, %u -> 0x%x.\n", i, beach[i*2], beach[i*2+1],
                                          ( beach[i*2]<<4 | beach[i*2+1] ));
        image[i] = ( beach[i*2]<<4 | beach[i*2+1] );
    }
    test_draw_image( id, image );
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

    ssd1322_clear_display( id );
//    test_checkerboard( id );
//    test_greyscales( id );
//    test_load_image( id );
    test_stream_pixel( id );
//    ssd1322_clear_display( id );

    for ( i = 25; i < 36; i++ )
    {
        ssd1322_draw_pixel( id, i, i, 0xa );
    }
    ssd1322_draw_pixel( id, 40, 40, 0x4 );
    ssd1322_draw_pixel( id, 0, 0, 0x4 );
    ssd1322_draw_pixel( id, 0, 63, 0x4 );
    ssd1322_draw_pixel( id, 255, 0, 0x4 );
    ssd1322_draw_pixel( id, 255, 63, 0x4 );

    test_draw_pixel( id );
//    ssd1322_clear_display( id );
    return 0;
}
