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
#include "synaptics.h"
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
event_query_is_synaptics(int fd)
{
    struct input_id id;
    int ret;

    SYSCALL(ret = ioctl(fd, EVIOCGID, &id));
    if (ret >= 0) {
	if ((id.bustype == BUS_I8042) &&
	    (id.vendor == 0x0002) &&
	    (id.product == PSMOUSE_SYNAPTICS)) {
	    return TRUE;
	}
    }
    return FALSE;
}

static Bool
EventQueryHardware(LocalDevicePtr local, struct SynapticsHwInfo *synhw)
{
    /* is the synaptics touchpad active? */
    if (!event_query_is_synaptics(local->fd))
	return FALSE;

    xf86Msg(X_PROBED, "%s synaptics touchpad found\n", local->name);
    return TRUE;
}

static Bool
SynapticsReadEvent(struct CommData *comm, struct input_event *ev)
{
    int i, c;
    unsigned char *pBuf, u;

    for (i = 0; i < sizeof(struct input_event); i++) {
	if ((c = XisbRead(comm->buffer)) < 0)
	    return FALSE;
	u = (unsigned char)c;
	pBuf = (unsigned char *)ev;
	pBuf[i] = u;
    }
    return TRUE;
}

static Bool
EventReadHwState(LocalDevicePtr local, struct SynapticsHwInfo *synhw,
		 struct SynapticsProtocolOperations *proto_ops,
		 struct CommData *comm, struct SynapticsHwState *hwRet)
{
    struct input_event ev;
    Bool v;
    struct SynapticsHwState *hw = &(comm->hwState);

    while (SynapticsReadEvent(comm, &ev)) {
	switch (ev.type) {
	case EV_SYN:
	    switch (ev.code) {
	    case SYN_REPORT:
		if (comm->oneFinger)
		    hw->numFingers = 1;
		else if (comm->twoFingers)
		    hw->numFingers = 2;
		else if (comm->threeFingers)
		    hw->numFingers = 3;
		else
		    hw->numFingers = 0;
		*hwRet = *hw;
		return TRUE;
	    }
	case EV_KEY:
	    v = (ev.value ? TRUE : FALSE);
	    switch (ev.code) {
	    case BTN_LEFT:
		hw->left = v;
		break;
	    case BTN_RIGHT:
		hw->right = v;
		break;
	    case BTN_MIDDLE:
		hw->middle = v;
		break;
	    case BTN_FORWARD:
		hw->up = v;
		break;
	    case BTN_BACK:
		hw->down = v;
		break;
	    case BTN_0:
		hw->multi[0] = v;
		break;
	    case BTN_1:
		hw->multi[1] = v;
		break;
	    case BTN_2:
		hw->multi[2] = v;
		break;
	    case BTN_3:
		hw->multi[3] = v;
		break;
	    case BTN_4:
		hw->multi[4] = v;
		break;
	    case BTN_5:
		hw->multi[5] = v;
		break;
	    case BTN_6:
		hw->multi[6] = v;
		break;
	    case BTN_7:
		hw->multi[7] = v;
		break;
	    case BTN_TOOL_FINGER:
		comm->oneFinger = v;
		break;
	    case BTN_TOOL_DOUBLETAP:
		comm->twoFingers = v;
		break;
	    case BTN_TOOL_TRIPLETAP:
		comm->threeFingers = v;
		break;
	    }
	    break;
	case EV_ABS:
	    switch (ev.code) {
	    case ABS_X:
		hw->x = ev.value;
		break;
	    case ABS_Y:
		hw->y = ev.value;
		break;
	    case ABS_PRESSURE:
		hw->z = ev.value;
		break;
	    case ABS_TOOL_WIDTH:
		hw->fingerWidth = ev.value;
		break;
	    }
	    break;
	}
    }
    return FALSE;
}

static Bool
EventAutoDevProbe(LocalDevicePtr local)
{
    /* We are trying to find the right eventX device or fall back to
       the psaux protocol and the given device from XF86Config */
    int i;

    for (i = 0; ; i++) {
	char fname[64];
	int fd = -1;
	Bool is_synaptics;

	sprintf(fname, "%s/%s%d", DEV_INPUT_EVENT, EVENT_DEV_NAME, i);
	SYSCALL(fd = open(fname, O_RDONLY));
	if (fd < 0) {
	    if (errno == ENOENT) {
		break;
	    } else {
		continue;
	    }
	}
	is_synaptics = event_query_is_synaptics(fd);
	SYSCALL(close(fd));
	if (is_synaptics) {
	    xf86Msg(X_PROBED, "%s auto-dev sets Synaptics Device to %s\n",
		    local->name, fname);
	    xf86ReplaceStrOption(local->options, "Device", fname);
	    return TRUE;
	}
    }
    ErrorF("%s no synaptics event device found (checked %d nodes)\n",
	   local->name, i + 1);
    return FALSE;
}

struct SynapticsProtocolOperations event_proto_operations = {
    EventDeviceOnHook,
    EventDeviceOffHook,
    EventQueryHardware,
    EventReadHwState,
    EventAutoDevProbe
};
