/*
 *   Copyright 2004 Peter Osterlund <petero2@telia.com>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "eventcomm.h"
#include "synproto.h"

#include <xf86.h>



#define SYSCALL(call) while (((call) == -1) && (errno == EINTR))


/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static void
EventDeviceOnHook(LocalDevicePtr local)
{
    /* Try to grab the event device so that data don't leak to /dev/input/mice */
    int ret;
    SYSCALL(ret = ioctl(local->fd, EVIOCGRAB, (pointer)1));
    if (ret < 0) {
	xf86Msg(X_WARNING, "%s can't grab event device\n",
		local->name, errno);
    }
}

static void
EventDeviceOffHook(LocalDevicePtr local)
{
}

static Bool
EventQueryHardware(LocalDevicePtr local, struct synapticshw *synhw, Bool *hasGuest)
{
    return TRUE;
}


struct SynapticsProtocolOperations event_proto_operations = {
    EventDeviceOnHook,
    EventDeviceOffHook,
    EventQueryHardware
};
