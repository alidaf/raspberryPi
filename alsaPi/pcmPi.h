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
    Authors:        D.Faulke            02/02/2016

    Contributors:

    Changelog:

        v01.00      Original version.
*/
//  ===========================================================================

#ifndef PCMPI_H
#define PCMPI_H

//  Info. ---------------------------------------------------------------------
/*
    I have searched high and low for details on how to capture ALSA PCM data to
    be able to code some audio meters but most examples rely on creating a ring
    buffer, creating a .asoundrc file and opening a local audio file. This
    isn't suitable for tapping into existing streams or viable for most users.
    The primary aim is to use Squeezelite as a player on the Tiny Core Linux
    platform, streaming from a Logitech Media Server. Squeezelite has a
    companion control and visualisation app (Jivelite) that requires a
    graphical desktop but taps into a shared memory buffer. There is little in
    the way of an API so this code represents an effort to convert as little
    of the Jivelite code as possible to provide a more generic feed of PCM
    stream data to allow visualisations to be coded for small LCD or OLED
    displays.
*/

//  Types. --------------------------------------------------------------------

#define VIS_BUF_SIZE 16384
//#define VUMETER_DEFAULT_SAMPLE_WINDOW 1024 * 2

static struct vis_t
{
    pthread_rwlock_t rwlock;
    uint32_t buf_size;
    uint32_t buf_index;
    bool running;
    uint32_t rate;
    time_t updated;
    int16_t buffer[VIS_BUF_SIZE];
}  *vis_mmap = NULL;

/*
static struct pcm_data
{
    uint32_t num_samples;
    uint32_t value_L;
    uint32_t value_R;
    uint32_t max_L;
    uint32_t max_R;
    uint8_t  db_L;
    uint8_t  db_R;
} pcm_data;
*/

static bool running = false;
static int  vis_fd = -1;
static char *mac_address = NULL;

//  Functions. ----------------------------------------------------------------


#endif // #ifndef PCMPI_H
