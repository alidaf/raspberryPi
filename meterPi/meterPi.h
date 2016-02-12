//  ===========================================================================
/*
    meterPi:

    Library to provide audio metering. Intended for use with LMS streams
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
    Authors:        D.Faulke            12/02/2016

    Contributors:

    Changelog:

        v01.00      Original version.
        v01.01      Renamed and converted to library.
*/
//  ===========================================================================

#ifndef METERPI_H
#define METERPI_H

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

            Typical panel       LCD 16x2 example (transposed)

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

#define VIS_BUF_SIZE 16384 // Predefined in Squeezelite.
#define PEAK_METER_LEVELS_MAX 16 // Number of peak meter intervals / LEDs.
#define METER_CHANNELS 2
#define TEST_LOOPS 10

//  Types. --------------------------------------------------------------------

struct peak_meter_t
{
    uint8_t  int_time;   // Integration time (ms).
    uint16_t samples;    // Samples for integration time.
    uint16_t hold_time;  // Peak hold time (ms).
    uint8_t  hold_count; // Hold time counter.
    uint8_t  num_levels; // Number of display levels
    int8_t   floor;      // Noise floor for meter (dB).
    uint16_t reference;  // Reference level.
    int8_t   dBfs      [METER_CHANNELS]; // dBfs values.
    uint8_t  bar_index [METER_CHANNELS]; // Index for bar display.
    uint8_t  dot_index [METER_CHANNELS]; // Index for dot display (peak hold).
    uint32_t elapsed   [METER_CHANNELS]; // Elapsed time (us).
    int16_t  scale     [PEAK_METER_LEVELS_MAX]; // Scale intervals.
};


//  Functions. ----------------------------------------------------------------

//  ---------------------------------------------------------------------------
//  Checks status of mmap and attempt to open/reopen if not updated recently.
//  ---------------------------------------------------------------------------
void vis_check( void );

//  ---------------------------------------------------------------------------
//  Returns the stream bit rate.
//  ---------------------------------------------------------------------------
uint32_t vis_get_rate( void );

//  ---------------------------------------------------------------------------
//  Calculates peak dBfs values (L & R) of a number of stream samples.
//  ---------------------------------------------------------------------------
void get_dBfs( struct peak_meter_t *peak_meter );

//  ---------------------------------------------------------------------------
//  Calculates the indices for string representations of the peak levels.
//  ---------------------------------------------------------------------------
void get_dB_indices( struct peak_meter_t *peak_meter );

#endif // #ifndef METERPI_H
