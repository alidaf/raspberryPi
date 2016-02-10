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
    Authors:        D.Faulke            10/02/2016

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

            Typical panel       LCD 16x2 (transposed)

           dBfs  L   R
              0 [ ] [ ]
             -2 [ ] [ ]         col  dBfs  cell
             -4 [ ] [ ]         16      0 [ ] [ ]
             -6 [ ] [ ]         15     -2 [ ] [ ]
             -8 [ ] [ ]         14     -4 [ ] [ ]
            -10 [ ] [ ]         13     -6 [ ] [ ]
            -12 [ ] [ ]         12     -8 [ ] [ ]
            -14 [ ] [ ]         11    -10 [ ] [ ]
            -16 [ ] [ ]         10    -12 [ ] [ ]
            -18 [ ] [ ]          9    -14 [ ] [ ]
            -20 [ ] [ ]          8    -16 [ ] [ ]
            -30 [ ] [ ]          7    -18 [ ] [ ]
            -40 [ ] [ ]          6    -20 [ ] [ ]
            -50 [ ] [ ]          5    -30 [ ] [ ]
                [ ] [ ]          4    -40 [ ] [ ]
            -70 [ ] [ ]          3    -50 [ ] [ ]
                [ ] [ ]          2    -60 [ ] [ ]
            -96 [ ] [ ]          1    -80 [L] [R]
                [ ] [ ]
           -inf [ ] [ ]

    Even though 0dBfs is full scale, it is still possible to get an overload
    with DAC overshoot. There are no specifications for detecting this in the
    IEC spec but Sony (?) emply a rule that an overload is defined by three
    consecutive 0dBFS readings. There are other variations but this one is
    fairly straightforward to understand...

    e.g.

     DAC overshoot -------> . '      } Overload
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
                                                     ['].[ ] [ ] [ ] [ ]
                                                         [.] [ ] [ ] [ ]
                                                            '[.] [ ] [ ]
                                                                .[ ] [ ]
                                                                 [ ] [ ]
                                                                 [.] [ ]
    dBFS ---------------------------------------------------------- '[.] ----

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

#define PEAK_METER_MAX_LEVELS 16 // Number of peak meter intervals / LEDs.
#define METER_CHANNELS_MAX 2
#define HOLD_DELAY 4

static struct peak_meter_t
{
    uint8_t  int_time;      // Integration time (ms).
    int8_t   dBfs[2];       // dBfs values.
    uint8_t  bar_index[2];  // Index for bar display.
    uint8_t  dot_index[2];  // Index for dot display (peak hold).
    uint8_t  num_levels;    // Number of display levels
    int8_t   floor;         // Noise floor for meter (dB);
    uint16_t reference;     // Reference level.
    int16_t  intervals[PEAK_METER_MAX_LEVELS]; // Scale intervals and bit maps.
}
    peak_meter =
{
    .int_time       = 1,
    .dBfs           = { 0, 0 },
    .bar_index      = { 0, 0 },
    .dot_index      = { 0, 0 },
    .num_levels     = 16,
    .floor          = -80,
    .reference      = 32768,
    .intervals      =
        // dBfs scale intervals - need to make this user controlled.
//        {    -80,    -60,    -50,    -40,    -30,    -20,    -18,    -16,
//             -14,    -12,    -10,     -8,     -6,     -4,     -2,      0  },
        {    -48,    -42,    -36,    -30,    -24,    -20,    -18,    -16,
             -14,    -12,    -10,     -8,     -6,     -4,     -2,      0  }
};

// String representations for LCD display.
char lcd16x2_peak_meter[METER_CHANNELS_MAX][PEAK_METER_MAX_LEVELS + 1] = { "L" , "R" };

static bool running = false;
static int  vis_fd = -1;
static char *mac_address = NULL;

//  Functions. ----------------------------------------------------------------


#endif // #ifndef PCMPI_H
