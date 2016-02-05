//  ===========================================================================
/*
    pcmPi:

    Library to provide PCM stream info. Intended for use with LMS streams
    via squeezelite to feed small LCD displays.

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
    Authors:        D.Faulke            05/02/2016

    Contributors:

    Changelog:

        v01.00      Original version.
        v01.01      Added ncurses demo window.
*/
//  ===========================================================================

#ifndef PCMPI_H
#define PCMPI_H

//  Info. ---------------------------------------------------------------------
/*
    I have searched high and low for details on how to capture ALSA PCM data to
    be able to code some audio meters but the API is difficult to understand
    and most examples rely on creating a recording ring buffer, creating or
    modifying a .asoundrc file and opening a local audio file rather than a
    stream.
    The primary aim is to use Squeezelite as a player on the Tiny Core Linux
    platform, streaming from a Logitech Media Server. Squeezelite has a
    companion control and visualisation app (Jivelite) that requires a
    graphical desktop but taps into a shared memory buffer. There is little in
    the way of an API so this code represents an effort to figure out the
    shared buffer and use it allow visualisations to be coded for small LCD or
    OLED displays.
*/

/*
    Peak Level Meter:

    According to IEC 60268-18 (1995), peak level meters should have the
    following characteristics:

        Delay time < 150ms.
        Integration time < 5ms.
        Return time < 1.7s +/- 0.3s.
        Hold time = 1.0s +/- 0.5s.

    The IEC60268-18 (1995) specification defines minimum scale markings per dB.

            +-------------------------------------------------+
            |     Range      |    Numbers     |     Ticks     |
            |----------------+----------------+---------------|
            |   0dB -> -20dB | 5dB  intervals | 1dB intervals |
            | -20dB -> -40dB | 10dB intervals | none          |
            | -40dB -> -60dB | 10dB intervals | at 45dB       |
            +-------------------------------------------------+

    A typical LED panel display can extend down to -96dB for 16-bit audio or
    -144dB for 24-bit audio. However, there are limitations due to The intended
    use here for an LCD with 16 columns

    A hardware peak meter with level hold using LEDs can be made with two
    multiplexed circuits; a bar graph mode circuit for the integrated signal
    and a dot mode circuit for the peak hold. These are multiplexed together
    to form the PPM display.

    The display levels will be encoded as two binary strings; one for the
    current level as contiguous bits (bar graph), and one for the peak hold as
    an individual bit (dot mode). These can then be OR'ed together to form a
    composite string that can be used to construct the meter.

            Typical panel       LCD 16x2 (transposed)

           dBfs  L   R
              0 [ ] [ ]
             -2 [ ] [ ]         col  dBfs  cell   bit
             -4 [ ] [ ]         16      0 [ ] [ ] 15
             -6 [ ] [ ]         15     -2 [ ] [ ] 14
             -8 [ ] [ ]         14     -4 [ ] [ ] 13
            -10 [ ] [ ]         13     -6 [ ] [ ] 12
            -12 [ ] [ ]         12     -8 [ ] [ ] 11
            -14 [ ] [ ]         11    -10 [ ] [ ] 10
            -16 [ ] [ ]         10    -12 [ ] [ ]  9
            -18 [ ] [ ]          9    -14 [ ] [ ]  8
            -20 [ ] [ ]          8    -16 [ ] [ ]  7
            -30 [ ] [ ]          7    -18 [ ] [ ]  6
            -40 [ ] [ ]          6    -20 [ ] [ ]  5
            -50 [ ] [ ]          5    -30 [ ] [ ]  4
                [ ] [ ]          4    -40 [ ] [ ]  3
            -70 [ ] [ ]          3    -50 [ ] [ ]  2
                [ ] [ ]          2    -60 [ ] [ ]  1
            -96 [ ] [ ]          1    -80 [L] [R]  0
                [ ] [ ]
           -inf [ ] [ ]

    Even though 0dBfs is full scale, it is still possible to get an overload
    with DAC overshoot. There are no specifications for detecting this in the
    IEC spec but Sony (?) emply a rule that an overload is defined by three
    consecutive 0dBFS readings. There are other variations but this one is
    fairly straightforward to understand...

    e.g.

     DAC overshoot -------> , '      } Overload
                          .     '    }
     dbFS -------------- [ ] [ ] ['] --------------------------------------
                         [ ] [ ] [ ].
                        '[ ] [ ] [ ] [']
                     ['] [ ] [ ] [ ] [ ]'
                    .[ ] [ ] [ ] [ ] [ ] [']
                 ['] [ ] [ ] [ ] [ ] [ ] [ ]'[.]
             [.]'[ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ].
           . [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [ ] [.]
    Mean  + - + - + - + - + - + - + - + - + - + - + - + - + - + - + - + -
                                                     [ ] [ ] [ ] [ ] [ ]
                                                     ['],[ ] [ ] [ ] [ ]
                                                         [,] [ ] [ ] [ ]
                                                            '[,] [ ] [ ]
                                                                ,[ ] [ ]
                                                                 [ ] [ ]
                                                                 [,] [ ]
    dBFS ---------------------------------------------------------- '[,] ----

*/

//  Types. --------------------------------------------------------------------

#define VIS_BUF_SIZE 16384 // Predefined in Squeezelite.

/*
    This is the structure of the shared memory object defined in Squeezelite.
    See https://github.com/ralph-irving/squeezelite/blob/master/output_vis.c.
    It looks like the samples in this buffer are fixed to 16-bit.
    Audio samples > 16-bit are right shifted so that only the higher 16-bits
    are used.
*/
static struct vis_t
{
    pthread_rwlock_t rwlock;
    uint32_t buf_size;
    uint32_t buf_index;
    bool     running;
    uint32_t rate;
    time_t   updated;
    int16_t  buffer[VIS_BUF_SIZE];
}  *vis_mmap = NULL;

#define PEAK_METER_INTERVALS 16 // Number of peak meter intervals / LEDs.
#define HOLD_DELAY 4

static struct peak_meter_t
{
    uint8_t  int_time;      // Integration time (ms).
    int8_t   dBfs[2];       // dBfs values.
    uint8_t  dBfs_index[4]; // indices for looking up bar and dot codes.
    int8_t   floor;         // Noise floor for meter (dB);
    uint16_t reference;     // Reference level.
    int16_t  intervals[PEAK_METER_INTERVALS]; // Scale intervals and bit maps.
    uint16_t leds_bar [PEAK_METER_INTERVALS]; // Bar mode binary strings.
    uint16_t leds_dot [PEAK_METER_INTERVALS]; // Dot mode binary strings.
}
    peak_meter =
{
    .int_time       = 1,
    .dBfs           = { 0, 0 },
    .dBfs_index     = { 0, 0, 0, 0 },
    .floor          = -80,
    .reference      = 32768,
    .intervals      =
        // dBfs scale intervals.
        {    -80,    -60,    -50,    -40,    -30,    -20,    -18,    -16,
             -14,    -12,    -10,     -8,     -6,     -4,     -2,      0  },
    .leds_bar       =
        // dBfs bar graph (peak level).
        { 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
          0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff  },
        // dBfs dot mode (peak hold).
    .leds_dot       =
        { 0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100,
          0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001   }
};

// String representations for LCD display.
char lcd16x2_peak_meter[2][PEAK_METER_INTERVALS + 1] = { "L" , "R" };

static bool running = false;
static int  vis_fd = -1;
static char *mac_address = NULL;

//  Functions. ----------------------------------------------------------------


#endif // #ifndef PCMPI_H
