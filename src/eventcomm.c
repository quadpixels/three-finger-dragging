/*
 * Copyright © 2004-2007 Peter Osterlund
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *      Peter Osterlund (petero2@telia.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "eventcomm.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include "synproto.h"
#include "synaptics.h"
#include "synapticsstr.h"
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

static Bool
event_query_is_touchpad(int fd)
{
    int ret;
    unsigned long evbits[NBITS(KEY_MAX)];

    /* Check for ABS_X, ABS_Y, ABS_PRESSURE and BTN_TOOL_FINGER */

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(EV_SYN, evbits) ||
	!TEST_BIT(EV_ABS, evbits) ||
	!TEST_BIT(EV_KEY, evbits))
	return FALSE;

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(evbits)), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(ABS_X, evbits) ||
	!TEST_BIT(ABS_Y, evbits) ||
	!TEST_BIT(ABS_PRESSURE, evbits))
	return FALSE;

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(evbits)), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(BTN_TOOL_FINGER, evbits))
	return FALSE;
    if (TEST_BIT(BTN_TOOL_PEN, evbits))
	return FALSE;			    /* Don't match wacom tablets */

    return TRUE;
}

/* Query device for axis ranges and store outcome in the default parameter. */
static void
event_query_axis_ranges(int fd, LocalDevicePtr local)
{
    SynapticsSHM *pars = &((SynapticsPrivate *)local->private)->synpara_default;
    struct input_absinfo abs;
    int rc;

    SYSCALL(rc = ioctl(fd, EVIOCGABS(ABS_X), &abs));
    if (rc == 0)
    {
	xf86Msg(X_INFO, "%s: x-axis range %d - %d\n", local->name,
		abs.minimum, abs.maximum);
	pars->left_edge  = abs.minimum;
	pars->right_edge = abs.maximum;
    } else
	xf86Msg(X_ERROR, "%s: failed to query axis range (%s)\n", local->name,
		strerror(errno));

    SYSCALL(rc = ioctl(fd, EVIOCGABS(ABS_Y), &abs));
    if (rc == 0)
    {
	xf86Msg(X_INFO, "%s: y-axis range %d - %d\n", local->name,
		abs.minimum, abs.maximum);
	pars->top_edge    = abs.minimum;
	pars->bottom_edge = abs.maximum;
    } else
	xf86Msg(X_ERROR, "%s: failed to query axis range (%s)\n", local->name,
		strerror(errno));
}

static Bool
EventQueryHardware(LocalDevicePtr local, struct SynapticsHwInfo *synhw)
{
    if (!event_query_is_touchpad(local->fd))
	return FALSE;

    xf86Msg(X_PROBED, "%s touchpad found\n", local->name);

    /* awful */
    if (strstr(local->name, "ALPS")) {
	SynapticsSHM *pars = ((SynapticsPrivate *)local->private)->synpara;
	void *opts = local->options;

	pars->left_edge = xf86SetIntOption(opts, "LeftEdge", 120);
	pars->right_edge = xf86SetIntOption(opts, "RightEdge", 830);
	pars->top_edge = xf86SetIntOption(opts, "TopEdge", 120);
	pars->bottom_edge = xf86SetIntOption(opts, "BottomEdge", 650);
	pars->finger_low = xf86SetIntOption(opts, "FingerLow", 14);
	pars->finger_high = xf86SetIntOption(opts, "FingerHigh", 15);
	pars->tap_move = xf86SetIntOption(opts, "MaxTapMove", 110);
	pars->scroll_dist_vert = xf86SetIntOption(opts, "VertScrollDelta", 20);
	pars->scroll_dist_horiz = xf86SetIntOption(opts, "HorizScrollDelta", 20);
	pars->min_speed = xf86SetRealOption(opts, "MinSpeed", 0.3);
	pars->max_speed = xf86SetRealOption(opts, "MaxSpeed", 0.75);
    }

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

	    event_query_axis_ranges(fd, local);
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
