/*
 *   Copyright 2004-2007 Peter Osterlund <petero2@telia.com>
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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "synproto.h"
#define SYNAPTICS_PRIVATE
#include "synaptics.h"
#include <xf86.h>


#define SYSCALL(call) while (((call) == -1) && (errno == EINTR))

#define LONG_BITS (sizeof(long) * 8)
#define NBITS(x) (((x) + LONG_BITS - 1) / LONG_BITS)
#define OFF(x)   ((x) % LONG_BITS)
#define LONG(x)  ((x) / LONG_BITS)
#define TEST_BIT(bit, array) (array[LONG(bit)] & (1 << OFF(bit)))

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static void
EventDeviceOnHook(LocalDevicePtr local, SynapticsSHM *para)
{
    if (para->grab_event_device) {
	/* Try to grab the event device so that data don't leak to /dev/input/mice */
	int ret;
	SYSCALL(ret = ioctl(local->fd, EVIOCGRAB, (pointer)1));
	if (ret < 0) {
	    xf86Msg(X_WARNING, "%s can't grab event device, errno=%d\n",
		    local->name, errno);
	}
    }
}

static void
EventDeviceOffHook(LocalDevicePtr local)
{
}

static void
event_query_abs_params(LocalDevicePtr local, int fd)
{
        int ret;
        SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);
        struct input_absinfo absinfo;
        SYSCALL(ret = ioctl(fd, EVIOCGABS(ABS_X), &absinfo));
        if (ret < 0)
                return;

        priv->minx = absinfo.minimum;
        priv->maxx = absinfo.maximum;

        SYSCALL(ret = ioctl(fd, EVIOCGABS(ABS_Y), &absinfo));
        if (ret < 0)
                return;

        priv->miny = absinfo.minimum;
        priv->maxy = absinfo.maximum;
}

static Bool
event_query_is_touchpad(int fd)
{
    int ret;
    unsigned long evbits[NBITS(KEY_MAX)];

    /* Check for ABS_X, ABS_Y, ABS_PRESSURE and BTN_TOOL_FINGER */

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(0, EV_MAX), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(EV_SYN, evbits) ||
	!TEST_BIT(EV_ABS, evbits) ||
	!TEST_BIT(EV_KEY, evbits))
	return FALSE;

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(EV_ABS, KEY_MAX), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(ABS_X, evbits) ||
	!TEST_BIT(ABS_Y, evbits) ||
	!TEST_BIT(ABS_PRESSURE, evbits))
	return FALSE;

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(BTN_TOOL_FINGER, evbits))
	return FALSE;
    if (TEST_BIT(BTN_TOOL_PEN, evbits))
	return FALSE;			    /* Don't match wacom tablets */

    return TRUE;
}

static Bool
EventQueryHardware(LocalDevicePtr local, struct SynapticsHwInfo *synhw)
{
    if (event_query_is_touchpad(local->fd)) {
	xf86Msg(X_PROBED, "%s touchpad found\n", local->name);
	return TRUE;
    }

    return FALSE;
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
		hw->guest_dx = hw->guest_dy = 0;
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
	    case BTN_A:
		hw->guest_left = v;
		break;
	    case BTN_B:
		hw->guest_right = v;
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
	case EV_REL:
	    switch (ev.code) {
	    case REL_X:
		hw->guest_dx = ev.value;
		break;
	    case REL_Y:
		hw->guest_dy = ev.value;
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
    Bool have_evdev = FALSE;
    int noent_cnt = 0;
    const int max_skip = 10;

    for (i = 0; ; i++) {
	char fname[64];
	int fd = -1;
	Bool is_touchpad;

	sprintf(fname, "%s/%s%d", DEV_INPUT_EVENT, EVENT_DEV_NAME, i);
	SYSCALL(fd = open(fname, O_RDONLY));
	if (fd < 0) {
	    if (errno == ENOENT) {
		if (++noent_cnt >= max_skip)
		    break;
		else
		    continue;
	    } else {
		continue;
	    }
	}
	noent_cnt = 0;
	have_evdev = TRUE;
	is_touchpad = event_query_is_touchpad(fd);
	if (is_touchpad) {
	    xf86Msg(X_PROBED, "%s auto-dev sets device to %s\n",
		    local->name, fname);
	    xf86ReplaceStrOption(local->options, "Device", fname);
	    event_query_abs_params(local, fd);
	    SYSCALL(close(fd));
	    return TRUE;
	}
	SYSCALL(close(fd));
    }
    ErrorF("%s no synaptics event device found (checked %d nodes)\n",
	   local->name, i + 1);
    if (i <= max_skip)
	ErrorF("%s The /dev/input/event* device nodes seem to be missing\n",
	       local->name);
    if (i > max_skip && !have_evdev)
	ErrorF("%s The evdev kernel module seems to be missing\n", local->name);
    return FALSE;
}

struct SynapticsProtocolOperations event_proto_operations = {
    EventDeviceOnHook,
    EventDeviceOffHook,
    EventQueryHardware,
    EventReadHwState,
    EventAutoDevProbe
};
