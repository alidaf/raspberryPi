/*****************************************************************************/
/*                                                                           */
/*    ALSA volume control using mixer controls.                              */
/*    Used for testing ways of controlling volume and balance on a           */
/*    Raspberry Pi.                                                          */
/*                                                                           */
/*    Copyright  2015 by Darren Faulke <darren@alidaf.co.uk>                 */
/*                                                                           */
/*    This program is free software; you can redistribute it and/or modify   */
/*    it under the terms of the GNU General Public License as published by   */
/*    the Free Software Foundation, either version 2 of the License, or      */
/*    (at your option) any later version.                                    */
/*                                                                           */
/*    This program is distributed in the hope that it will be useful,        */
/*    but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          */
/*    GNU General Public License for more details.                           */
/*                                                                           */
/*    You should have received a copy of the GNU General Public License      */
/*    along with this program. If not, see <http://www.gnu.org/licenses/>.   */
/*                                                                           */
/*****************************************************************************/

#define Version "Version 0.2"

/*****************************************************************************/
/*                                                                           */
/*    Authors:            D.Faulke                    19/10/15               */
/*    Contributors:                                                          */
/*                                                                           */
/*    Changelog:                                                             */
/*                                                                           */
/*    v0.1 Initial version.                                                  */
/*    v0.2 Added command line parameters.                                    */
/*                                                                           */
/*****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <argp.h>

/*****************************************************************************/
/*                                                                           */
/*    Program documentation:                                                 */
/*                                                                           */
/*****************************************************************************/

const char *argp_program_version = Version;
const char *argp_program_bug_address = "darren@alidaf.co.uk";
static char doc[] = "A short test program to set ALSA control values.";
static char args_doc[] = "alsavol <options>";

/*****************************************************************************/
/*                                                                           */
/*    Data definitions:                                                      */
/*                                                                           */
/*****************************************************************************/

// Data structure to hold command line arguments.
struct structArgs
{
    int card;
    int control;
    char deviceID[8];
    int value1;
    int value2;
};

/*****************************************************************************/
/*                                                                           */
/*    Command line argument definitions.                                     */
/*                                                                           */
/*****************************************************************************/

static struct argp_option options[] =
{
    { 0, 0, 0, 0, "Card information:" },
    { "card", 'c', "<n>", 0, "Card ID number." },
    { "control", 'd', "<n>", 0, "Control ID number." },
    { 0, 0, 0, 0, "Control parameters:" },
    { "val", 'v', "<%>/<%,%>", 0, "Set control value(s)." },
    { 0 }
};

/*****************************************************************************/
/*                                                                           */
/*    Command line argument parser.                                          */
/*                                                                           */
/*****************************************************************************/

static int parse_opt( int param, char *arg, struct argp_state *state )
{
    char *str;
    char *token;
    const char delimiter[] = ",";
    struct structArgs *commandArgs = state->input;

    switch( param )
    {
        case 'c' :
            commandArgs->card = atoi( arg );
            break;
        case 'd' :
            commandArgs->control = atoi( arg );
            break;
        case 'v' :
            str = arg;
            token = strtok( str, delimiter );
            commandArgs->value1 = atoi( token );
            token = strtok( NULL, delimiter );
            if ( token == NULL )
                commandArgs->value2 = commandArgs->value1;
            else
                commandArgs->value2 = atoi( token );
            break;
    }
    return 0;
};

/*****************************************************************************/
/*                                                                           */
/*    argp parser parameter structure.                                       */
/*                                                                           */
/*****************************************************************************/

static struct argp argp = { options, parse_opt, args_doc, doc };

/*****************************************************************************/
/*                                                                           */
/* Main routine.                                                             */
/*                                                                           */
/*****************************************************************************/

void main( int argc, char *argv[] )
{

    struct structArgs commandArgs;
    int errNum;

    argp_parse( &argp, argc, argv, 0, 0, &commandArgs );

    printf( "Card = %i\n", commandArgs.card );
    printf( "Control = %i\n", commandArgs.control );
    printf( "Value 1 = %i\n", commandArgs.value1 );
    printf( "Value 2 = %i\n", commandArgs.value2 );

    snd_ctl_t *ctl;                    // Simple control handle.
    snd_ctl_elem_id_t *id;            // Simple control element id.
    snd_ctl_elem_value_t *control;    // Simple control element value.
    snd_ctl_elem_type_t type;        // Simple control element type.
    snd_ctl_elem_info_t *info;        // Simple control info container.

    printf( "Opening a control.\n" );
    // Open a high level control and load it's data.
    sprintf( commandArgs.deviceID, "hw:%i", commandArgs.card );
    printf( "Device ID = %s\n", commandArgs.deviceID );
    errNum = snd_ctl_open( &ctl, commandArgs.deviceID, 1 );
    if ( errNum < 0 ) printf( "Error opening control.\n" );

    printf( "Initialising control element.\n" );
    // Initialise a simple control element id structure.
    snd_ctl_elem_id_alloca( &id );
    snd_ctl_elem_id_set_numid( id, commandArgs.control );

    // Initialise info element.
    snd_ctl_elem_info_alloca( &info );
    printf( "Setting numid.\n" );
    snd_ctl_elem_info_set_numid( info, commandArgs.control );

    // Is the control valid?
    printf( "Is the control valid?\n" );
    if (( errNum = snd_ctl_elem_info( ctl, info )) < 0 )
        printf( "Error: %s\n", snd_strerror( errNum ));

    printf( "Getting some control information.\n" );
    // Find type of control.
    // either:
    // SND_CTL_ELEM_TYPE_INTEGER,
    // SND_CTL_ELEM_TYPE_INTEGER64,
    // SND_CTL_ELEM_TYPE_ENUMERATED.
    // Only interested in INTEGER.
    type = snd_ctl_elem_info_get_type( info );
    if ( type != SND_CTL_ELEM_TYPE_INTEGER )
    {
        printf( "Control type is not integer.\n" );
        printf( "Type = %s\n", type );
        return;
    }
    // Get some control information.
    printf( "Min value for control = %d\n", snd_ctl_elem_info_get_min( info ));
    printf( "Max value for control = %d\n", snd_ctl_elem_info_get_max( info ));
    printf( "Step value for control = %d\n", snd_ctl_elem_info_get_step( info ));

    // Initialise the control element value container.
    snd_ctl_elem_value_alloca( &control );
    snd_ctl_elem_value_set_id( control, id );

    // Set control values.
    snd_ctl_elem_value_set_integer( control, 0, 100 );    // Left channel.
    errNum = snd_ctl_elem_write( ctl, control );
    if ( errNum < 0 ) printf( "Error setting volume.\n" );
    snd_ctl_elem_value_set_integer( control, 1, 100 );    // Right channel.
    errNum = snd_ctl_elem_write( ctl, control );
    if ( errNum < 0 ) printf( "Error setting volume.\n" );

    snd_ctl_close( ctl );

    return;
}
