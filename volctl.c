// ****************************************************************************
// ****************************************************************************
/*
    volctl:

    Simple test set volume using ALSA controls.

    Copyright  2015 by Darren Faulke <darren@alidaf.co.uk>

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
// ****************************************************************************
// ****************************************************************************

#define Version "Version 0.2"

//  Compilation:
//
//  Compile with gcc volctl.c -o volctl -lasound
//  Also use the following flags for Raspberry Pi optimisation:
//         -march=armv6 -mtune=arm1176jzf-s -mfloat-abi=hard -mfpu=vfp
//         -ffast-math -pipe -O3

//    Authors:     	D.Faulke	19/10/15
//    Contributors:
//
//    Changelog:
//
//    v0.1 Initial version.
//    v0.2 Added command line parameters.

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <argp.h>

// ****************************************************************************
//  argp documentation.
// ****************************************************************************

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static char doc[] = "A short test program to set ALSA control values.";
static char args_doc[] = "alsavol <options>";

// ****************************************************************************
//  Data definitions.
// ****************************************************************************

// Data structure to hold command line arguments.
struct structArgs
{
    int card;
    int control;
    char deviceID[8];
    int value1;
    int value2;
};

// ****************************************************************************
//  Command line argument definitions.
// ****************************************************************************

static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Card information:" },
    { "card", 'c', "<n>", 0, "Card ID number." },
    { "control", 'd', "<n>", 0, "Control ID number." },
    { 0, 0, 0, 0, "Control parameters:" },
    { "val", 'v', "<%>/<%,%>", 0, "Set control value(s)." },
    { 0 }
};

// ****************************************************************************
//  Command line argument parser.
// ****************************************************************************

static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str;
    char *token;
    const char delimiter[] = ",";
    struct structArgs *cmdArgs = state->input;

    switch( param )
    {
        case 'c' :
            cmdArgs->card = atoi( arg );
            break;
        case 'd' :
            cmdArgs->control = atoi( arg );
            break;
        case 'v' :
            str = arg;
            token = strtok( str, delimiter );
            cmdArgs->value1 = atoi( token );
            token = strtok( NULL, delimiter );
            if ( token == NULL )
                cmdArgs->value2 = cmdArgs->value1;
            else
                cmdArgs->value2 = atoi( token );
            break;
    }
    return 0;
};

// ****************************************************************************
//  argp parser parameter structure.
// ****************************************************************************

static struct argp argp = { options, parse_opt, args_doc, doc };

// ****************************************************************************
//  Main section.
// ****************************************************************************

void main( int argc, char *argv[] )
{

    struct structArgs cmdArgs;
    int errNum;


    // ************************************************************************
    //  ALSA control elements.
    // ************************************************************************
    snd_ctl_t *ctl;                 // Simple control handle.
    snd_ctl_elem_id_t *id;          // Simple control element id.
    snd_ctl_elem_value_t *control;  // Simple control element value.
    snd_ctl_elem_type_t type;       // Simple control element type.
    snd_ctl_elem_info_t *info;      // Simple control info container.


    // ************************************************************************
    //  Get command line parameters.
    // ************************************************************************
    argp_parse( &argp, argc, argv, 0, 0, &cmdArgs );

    printf( "Card = %i\n", cmdArgs.card );
    printf( "Control = %i\n", cmdArgs.control );
    printf( "Value 1 = %i\n", cmdArgs.value1 );
    printf( "Value 2 = %i\n", cmdArgs.value2 );


    // ************************************************************************
    //  Set up ALSA control.
    // ************************************************************************
    sprintf( cmdArgs.deviceID, "hw:%i", cmdArgs.card );
    printf( "Device ID = %s.\n", cmdArgs.deviceID );

    if ( snd_ctl_open( &ctl, cmdArgs.deviceID, 1 ) < 0 )
    {
        printf( "Error opening control.\n" );
        return;
    }
    // Initialise a simple control element id structure.
    snd_ctl_elem_id_alloca( &id );
    snd_ctl_elem_id_set_numid( id, cmdArgs.control );

    // Initialise info element.
    snd_ctl_elem_info_alloca( &info );
    snd_ctl_elem_info_set_numid( info, cmdArgs.control );

    // Is the control valid?
    if ( snd_ctl_elem_info( ctl, info ) < 0 )
    {
        printf( "Error getting control element info.\n" );
        return;
    }

    // Find type of control.
    // either:
    // SND_CTL_ELEM_TYPE_INTEGER,
    // SND_CTL_ELEM_TYPE_INTEGER64,
    // SND_CTL_ELEM_TYPE_ENUMERATED, etc.
    // Only interested in INTEGER.
    type = snd_ctl_elem_info_get_type( info );
    if ( type != SND_CTL_ELEM_TYPE_INTEGER )
    {
        printf( "Control type is not integer.\n" );
        printf( "Type = %s\n", type );
        return;
    }


    // ************************************************************************
    //  Get some information for selected control.
    // ************************************************************************
    printf( "Min value for control = %d\n", snd_ctl_elem_info_get_min( info ));
    printf( "Max value for control = %d\n", snd_ctl_elem_info_get_max( info ));
    printf( "Step value for control = %d\n", snd_ctl_elem_info_get_step( info ));

    // Initialise the control element value container.
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_value_set_id( control, id );


    // ************************************************************************
    //  Set values for selected control.
    // ************************************************************************
    snd_ctl_elem_value_set_integer( control, 0, cmdArgs.value1 );
    if ( snd_ctl_elem_write( ctl, control ) < 0 )
        printf( "Error setting L volume" );
    else
        printf( "Set L volume to %d.\n", cmdArgs.value1 );
    snd_ctl_elem_value_set_integer( control, 1, cmdArgs.value2 );
    if ( snd_ctl_elem_write( ctl, control ) < 0 )
        printf( "Error setting R volume" );
    else
        printf( "Set R volume to %d.\n", cmdArgs.value2 );


    // ************************************************************************
    //  Clean up.
    // ************************************************************************
    snd_ctl_close( ctl );

    return;
}
