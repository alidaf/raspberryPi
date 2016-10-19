//============================================================================
/*
    Bit-packs bitmap font stream to Adafruit-gfx format.
*/
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define BITS_BYTE 8
#define COLS_MAX  8 // Used for formatting binary output.

#define GLYPH_ROWS_MAX 20 // Maximum number of glyph rows (pixel height)
#define GLYPH_COLS_MAX  8 // Maximum number of glyph cols (pixel width).
#define GLYPHS_MAX    256 // Maximum number of glyphs (extended ASCII).

struct glyph
{
    uint8_t  ascii;     // Ascii table position.
    uint16_t offset;    // Offset into packed array.
    uint8_t  width;     // Packed width.
    uint8_t  height;    // Packed height.
    uint8_t  xadvance;  // Pixels to next char.
    int8_t   xoffset;   // Offset relative to base.
    int8_t   yoffset;   // Offset relative to left.
};

//=============================================================================
/*
    Converts numerical value (dec/hex) of char c.
*/
//=============================================================================
uint8_t char_to_hex( char c )
{
    uint8_t hex = ( uint8_t ) c;
    if( hex <  58 && hex > 47 ) return ( hex - 48 );
    if( hex <  71 && hex > 64 ) return ( hex - 55 );
    if( hex < 103 && hex > 96 ) return ( hex - 87 );
    return hex;
}

//=============================================================================
/*
    Returns bit state at position pos of byte hex.
*/
//=============================================================================
bool get_bit( uint8_t hex, uint8_t pos )
{
    if (( pos < 0 ) || ( pos > 7 ))
    {
        printf( "Bad position.\n" );
        return 0;
    }
    return ( hex & ( 1 << pos ));
}

//=============================================================================
/*
    Main.
*/
//=============================================================================

int main()
{
    uint8_t  glyph_width  =  8; // Width of glyph edit box (byte multiples).
    uint8_t  glyph_height = 20; // Height of glyph edit box.
    uint8_t  glyph_base   = 12; // Glyph baseline.

    uint8_t  glyph_start =  32; // ASCII table start position.
    uint8_t  glyph_end   =  45; // ASCII table end position.
//    uint8_t  glyph_end   = 126; // ASCII table end position.
    uint8_t  glyph_num;         // Number of glyphs.
    uint16_t glyph_bits;        // Bits in each unpacked glyph.
    uint16_t offset = 0;        // Counter for packed glyph offset.

    bool unpacked[65536];       // Enough for 128 chars of 16x32.
    bool packed[65536];         // Enough for 128 chars of 16x32.

    bool glyph_matrix[GLYPH_ROWS_MAX][GLYPH_COLS_MAX];

    struct glyph glyph[GLYPHS_MAX]; // Inefficient but easy.

    uint8_t hex; // Temp store for glyph hex value.

    char input_file[16] = "unpacked.txt"; // Input filename.
    char ch1, ch2; // Chars read from file.
    FILE *fp;      // File pointer.

    uint8_t bit;            // Counters.
    uint8_t glyph_current;  // Counter.
    bool    cell;           // Glyph bit value.
    uint8_t row, col, byte; // Counters.
    uint8_t row_min = 0;    // Row start of glyph.
    uint8_t row_max = 0;    // Row extent of glyph.
    uint8_t col_min = 0;    // COl start of glyph.
    uint8_t col_max = 0;    // Col extent of glyph.

    uint16_t packed_count = 0; // Packed glyph bit count.
    uint8_t packed_hex[GLYPH_ROWS_MAX * GLYPH_COLS_MAX];

    glyph_num  = glyph_end   - glyph_start;
    glyph_bits = glyph_width * glyph_height;

    printf( "Processing %d glyphs from ASCII %d (%c) to %d (%c).\n",
             glyph_num, glyph_start, glyph_start, glyph_end, glyph_end );

    // Open input file (read only). -------------------------------------------
    fp = fopen( input_file, "r" );

    if ( fp == NULL )
    {
        perror( "Error opening file.\n" );
        exit( EXIT_FAILURE );
    }

    // For each glyph. --------------------------------------------------------
    for ( glyph_current = 0; glyph_current < glyph_num + 1; glyph_current++ )
    {
        printf( "Glyph %d (%c):\n\n", glyph_current + glyph_start,
                                      glyph_current + glyph_start );

        printf( "Unpacked bitmap representation from file:\n\n" );

        // Reset bound counters for next glyph. -------------------------------
        row_min = glyph_height - 1;
        row_max = 0;
        col_min = glyph_width - 1;
        col_max = 0;

        // Glyph rows. --------------------------------------------------------
        for( row = 0; row < glyph_height; row++ )
        {
            // Glyph cols. ----------------------------------------------------
            col = 0;
            while( col < glyph_width )
            {
                // Read byte pair of chars from file. -------------------------
                if (( ch1 = fgetc( fp )) == EOF )
                {
                    printf( "Unexpected end of file!\n" );
                    return 0;
                };
                if (( ch2 = fgetc( fp )) == EOF )
                {
                    printf( "Unexpected end of file!\n" );
                    return 0;
                };
                fgetc( fp ); // Throw away the LF or CRLF.

                // Get hex value of byte pair chars. --------------------------
                hex = char_to_hex( ch1 ) << 4 | char_to_hex( ch2 );
                printf( "%c%c\t0x%02x\t", ch1, ch2, hex );

                // Get bits in big-endian order. ------------------------------
                for ( bit = 0; bit < BITS_BYTE; bit++ )
                {
                    cell = get_bit( hex, BITS_BYTE - bit - 1 );
                    glyph_matrix[row][col] = cell;
                    unpacked[glyph_current * glyph_bits +
                                       row * glyph_width + col] = cell;
                    // Get bounds and draw unpacked glyph. --------------------
                    if ( cell == 1 )
                    {
                        printf( "#" );

                        // Get glyph bounds. ----------------------------------
                        if ( row > row_max ) row_max = row;
                        if ( col > col_max ) col_max = col;
                        if ( row < row_min ) row_min = row;
                        if ( col < col_min ) col_min = col;
                    }
                    else printf( "." );
                    col++;
                }
            }
            printf( "\n" );
        }
        printf( "\n" );
        printf( "\tmin\tmax\n" );
        printf("row\t%d\t%d\n", row_min, row_max );
        printf("col\t%d\t%d\n", col_min, col_max );

        // Draw packed glyph. -------------------------------------------------
        printf( "\nPacked bitmap representation:\n" );
        for ( row = row_min; row <= row_max; row++ )
        {
            printf( "\n\t" );
            for ( col = col_min; col <= col_max; col++ )
            {
                packed[packed_count] = glyph_matrix[row][col];
                packed_count++;
                if ( glyph_matrix[row][col] == 1 ) printf( "#" );
                else printf( "." );
            }
        }
        printf( "\n" );

        // Fill glyph info struct. --------------------------------------------
        glyph[glyph_current].ascii   = glyph_current + glyph_start;
        glyph[glyph_current].width   = col_max - col_min;
        glyph[glyph_current].height  = row_max - row_min;
        glyph[glyph_current].offset  = offset;
        glyph[glyph_current].xoffset = col_min;
        glyph[glyph_current].yoffset = row_min - glyph_base;
        offset += row_max * col_max;


        // Print summary for glyph. -------------------------------------------
        printf( "\nGlyph %d summary:\n", glyph[glyph_current].ascii );
        printf( "\tOffset  = %d.\n", glyph[glyph_current].offset );
        printf( "\tWidth   = %d.\n", glyph[glyph_current].width );
        printf( "\tHeight  = %d.\n", glyph[glyph_current].height );
        printf( "\txOffset = %d.\n", glyph[glyph_current].xoffset );
        printf( "\tyOffset = %d.\n", glyph[glyph_current].yoffset );
        printf( "\n" );
    }

    // Pad packed bits to next byte boundary. ---------------------------------
    while (( packed_count % BITS_BYTE ) != 0 )
    {
        packed_count++;
        packed[packed_count] = 0;
    }

    // Print out unpacked bits matrix. ----------------------------------------
/*
    printf( "Unpacked bits matrix:\n" );
    uint16_t count;
    uint8_t col_count = 0;
    for( count = 0; count < glyph_bits * glyph_num; count++ )
    {
        printf( "%d", unpacked[count] );
        col_count++;
        if ( col_count >= COLS_MAX )
        {
            printf( "\n" );
            col_count = 0;
        }
    }
    printf( "\n" );
*/
    // Print out packed bits matrix. ------------------------------------------
/*
    printf( "Packed bits matrix:\n" );
    col_count = 0;
    for( count = 0; count < packed_count; count++ )
    {
        printf( "%d", packed[count] );
        col_count++;
        if ( col_count >= COLS_MAX )
        {
            printf( "\n" );
            col_count = 0;
        }
    }
    printf( "\n\n" );
*/
    // Print some summary information. ----------------------------------------
    printf( "Packed %d bytes into %d.\n",
             ( glyph_bits * glyph_num ) / BITS_BYTE, packed_count / BITS_BYTE );
    printf( "Saved %d bytes.\n",
             ( glyph_bits * glyph_num - packed_count ) / BITS_BYTE );
    printf( "\n\n" );

    // Now turn packed bits into bytes. ---------------------------------------
    uint16_t byte_count;
    uint8_t  byte_value;
    printf( "Hex Values:\n\n" );
    for( byte_count = 0; byte_count < packed_count / BITS_BYTE; byte_count++ )
    {
        byte_value = 0;
        printf( "\t" );
        for( bit = 0; bit < BITS_BYTE; bit++ )
        {
            byte_value += ( packed[byte_count * BITS_BYTE + bit] ) *
                          ( 1 << ( BITS_BYTE - bit - 1 ));
            packed_hex[byte_count] = byte_value;
            printf( "%d", packed[byte_count * BITS_BYTE + bit] );
        }
        printf( " = 0x%02x\n", byte_value );

    }
    printf( "\n" );

    fclose( fp );

    return 0;
}
