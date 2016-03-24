###SSD1322-SPI

A library for using a SSD1322 based OLED display via the SPI bus.

Compilation:

    gcc -c -fpic -Wall ssd1322-spi.c -lpigpio

    Also use the following flags for Raspberry Pi optimisation:

    -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp -ffast-math -pipe -O3

Based on the following guides and codes:
* SSD1322 datasheet from Solomon Systech (www.solomon-systech.com).

An example of such a display is the East Rising ER-OLEDM032-1 series:

 ** Caveat Emptor: **

 It appears that the operating mode is configured by a couple of zero Ohm resistors soldered onto the back of the board rather than jumpers. It isn't at all clear that the buyer should specify a preference or be prepared to desolder and move them. Not an easy task given how small they are.

The configurations according to where the resistors should be placed are:

    ,---------------------------,
    | Mode          | BS1 | BS0 |
    |---------------+-----+-----|
    | 6800 parallel | R18 | R20 |
    | 8080 parallel | R18 | R21 |
    | 4-wire SPI    | R19 | R21 |
    | 3-wire SPI    | R19 | R20 |
    '---------------------------'

The pin configuration is:

    ,-------------,
    |  2 [o o]  1 |
    |  4 [o o]  3 |
    |  6 [o o]  5 |
    |  8 [o o]  7 |
    | 10 [o o]  9 |
    | 12 [o o] 11 |
    | 14 [o o] 13 |
    | 16 [o o] 15 |
    '-------------'

For SPI mode, the pin designations and relevant RPi pins are:

    ,---------------------------------------------------------,
    | Pin | Fn     | Notes                      | RPi connect |
    |-----+--------+----------------------------+-------------+
    |   1 | VSS    | Ground.                    | GND         |
    |   2 | VBAT   | Power (3.3-5V).            | +5V         |
    |   3 | NC     | Leave floating.            | -           |
    |   4 | DB0    | SPI SCLK                   | SPI0_SCLK   |
    |   5 | DB1    | SPI SDIN                   | SPI0_MOSI   |
    |   6 | DB2    | Leave floating.            | -           |
    |   7 | DB3    | -                          | GND         |
    |   8 | DB4    | -                          | GND         |
    |   9 | DB5    | -                          | GND         |
    |  10 | DB6    | -                          | GND         |
    |  11 | DB7    | -                          | GND         |
    |  12 | /RD    | -                          | GND         |
    |  13 | /WR    | -                          | GND         |
    |  14 | /DC    | Data/Command for 4-wire.   | GPIOxx      |
    |  15 | /RESET | Power Reset.               | GPIOxx      |
    |  16 | /CS    | SPI Chip Select.           | SPI0_CEx_N  |
    '---------------------------------------------------------'

SPI mode does not allow reads so MISO is not used.

Command/Data mode is set by the /DC flag (low for command, high for data). SDIN is shifted into an 8-bit register on every rising edge of SCLK (DB0) with the MSB first, i.e. D7, D6, D5, D4, D3, D2, D1, D0.

####Operation:

The display I have is 256x64 but the SSD1322 controller supports up to 480x128. This means that there is additional ram that can be used and shifted or scrolled.

Rows correspond to the number of pixels top to bottom but the columns group 4 pixels in each, hence there are 120 columns of 4 pixels. Each pixel has a 4-bit address in the display RAM.

Drawing an arbitrary pixel therefore requires setting the row and column and writing 2 bytes of data that contain the pixel to set.
