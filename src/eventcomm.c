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

#include <xorg-server.h>
#include "eventcomm.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
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
EventDeviceOnHook(LocalDevicePtr local, SynapticsParameters *para)
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

static Bool
event_query_is_touchpad(int fd)
{
    int ret;
    unsigned long evbits[NBITS(EV_MAX)] = {0};
    unsigned long absbits[NBITS(ABS_MAX)] = {0};
    unsigned long keybits[NBITS(KEY_MAX)] = {0};

    /* Check for ABS_X, ABS_Y, ABS_PRESSURE and BTN_TOOL_FINGER */

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(0, sizeof(evbits)), evbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(EV_SYN, evbits) ||
	!TEST_BIT(EV_ABS, evbits) ||
	!TEST_BIT(EV_KEY, evbits))
	return FALSE;

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits));
    if (ret < 0)
	return FALSE;
    if (!TEST_BIT(ABS_X, absbits) ||
	!TEST_BIT(ABS_Y, absbits))
	return FALSE;

    SYSCALL(ret = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits));
    if (ret < 0)
	return FALSE;

    /* we expect touchpad either report raw pressure or touches */
    if (!TEST_BIT(ABS_PRESSURE, absbits) && !TEST_BIT(BTN_TOUCH, keybits))
	return FALSE;
    /* all Synaptics-like touchpad report BTN_TOOL_FINGER */
    if (!TEST_BIT(BTN_TOOL_FINGER, keybits))
	return FALSE;
    if (TEST_BIT(BTN_TOOL_PEN, keybits))
	return FALSE;			    /* Don't match wacom tablets */

    return TRUE;
}

static void
event_query_info(LocalDevicePtr local)
{
    SynapticsPrivate *priv = (SynapticsPrivate *)local->private;
    short id[4];
    int rc;

    SYSCALL(rc = ioctl(local->fd, EVIOCGID, id));
    if (rc < 0)
        return;

    if (id[ID_VENDOR] == 0x2 && id[ID_PRODUCT] == 0x7)
        priv->model = MODEL_SYNAPTICS;
    else if (id[ID_VENDOR] == 0x2 && id[ID_PRODUCT] == 0x8)
        priv->model = MODEL_ALPS;

}

/* Query device for axis ranges */
static void
event_query_axis_ranges(LocalDevicePtr local)
{
    SynapticsPrivate *priv = (SynapticsPrivate *)local->private;
    struct input_absinfo abs;
    unsigned long absbits[NBITS(ABS_MAX)] = {0};
    unsigned long keybits[NBITS(KEY_MAX)] = {0};
    char buf[256];
    int rc;

    SYSCALL(rc = ioctl(local->fd, EVIOCGABS(ABS_X), &abs));
    if (rc >= 0)
    {
	xf86Msg(X_INFO, "%s: x-axis range %d - %d\n", local->name,
		abs.minimum, abs.maximum);
	priv->minx = abs.minimum;
	priv->maxx = abs.maximum;
    } else
	xf86Msg(X_ERROR, "%s: failed to query axis range (%s)\n", local->name,
		strerror(errno));

    SYSCALL(rc = ioctl(local->fd, EVIOCGABS(ABS_Y), &abs));
    if (rc >= 0)
    {
	xf86Msg(X_INFO, "%s: y-axis range %d - %d\n", local->name,
		abs.minimum, abs.maximum);
	priv->miny = abs.minimum;
	priv->maxy = abs.maximum;
    } else
	xf86Msg(X_ERROR, "%s: failed to query axis range (%s)\n", local->name,
		strerror(errno));

    priv->has_pressure = FALSE;
    SYSCALL(rc = ioctl(local->fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits));
    if (rc >= 0)
	priv->has_pressure = TEST_BIT(ABS_PRESSURE, absbits);
    else
	xf86Msg(X_ERROR, "%s: failed to query ABS bits (%s)\n", local->name,
		strerror(errno));

    if (priv->has_pressure)
    {
	SYSCALL(rc = ioctl(local->fd, EVIOCGABS(ABS_PRESSURE), &abs));
	if (rc >= 0)
	{
	    xf86Msg(X_INFO, "%s: pressure range %d - %d\n", local->name,
		    abs.minimum, abs.maximum);
	    priv->minp = abs.minimum;
	    priv->maxp = abs.maximum;
	}
    } else
	xf86Msg(X_INFO,
		"%s: device does not report pressure, will use touch data.\n",
		local->name);

    SYSCALL(rc = ioctl(local->fd, EVIOCGABS(ABS_TOOL_WIDTH), &abs));
    if (rc >= 0)
    {
	xf86Msg(X_INFO, "%s: finger width range %d - %d\n", local->name,
		abs.minimum, abs.maximum);
	priv->minw = abs.minimum;
	priv->maxw = abs.maximum;
    }

    SYSCALL(rc = ioctl(local->fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits));
    if (rc >= 0)
    {
	buf[0] = 0;
	if ((priv->has_left = TEST_BIT(BTN_LEFT, keybits)))
	   strcat(buf, " left");
	if ((priv->has_right = TEST_BIT(BTN_RIGHT, keybits)))
	   strcat(buf, " right");
	if ((priv->has_middle = TEST_BIT(BTN_MIDDLE, keybits)))
	   strcat(buf, " middle");
	if ((priv->has_double = TEST_BIT(BTN_TOOL_DOUBLETAP, keybits)))
	   strcat(buf, " double");
	if ((priv->has_triple = TEST_BIT(BTN_TOOL_TRIPLETAP, keybits)))
	   strcat(buf, " triple");
	xf86Msg(X_INFO, "%s: buttons:%s\n", local->name, buf);
    }
}

static Bool
EventQueryHardware(LocalDevicePtr local, struct SynapticsHwInfo *synhw)
{
    if (!event_query_is_touchpad(local->fd))
	return FALSE;

    xf86Msg(X_PROBED, "%s: touchpad found\n", local->name);

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
    SynapticsPrivate *priv = (SynapticsPrivate *)local->private;
    SynapticsParameters *para = &priv->synpara;

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
	    case BTN_TOUCH:
		if (!priv->has_pressure)
			hw->z = v ? para->finger_high + 1 : 0;
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

/* filter for the AutoDevProbe scandir on /dev/input */
static int EventDevOnly(const struct dirent *dir) {
	return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

/**
 * Probe the open device for dimensions.
 */
static void
EventReadDevDimensions(LocalDevicePtr local)
{
    if (event_query_is_touchpad(local->fd))
	event_query_axis_ranges(local);
    event_query_info(local);
}

static Bool
EventAutoDevProbe(LocalDevicePtr local)
{
    /* We are trying to find the right eventX device or fall back to
       the psaux protocol and the given device from XF86Config */
    int i;
    Bool touchpad_found = FALSE;
    struct dirent **namelist;

    i = scandir(DEV_INPUT_EVENT, &namelist, EventDevOnly, alphasort);
    if (i < 0) {
		ErrorF("Couldn't open %s\n", DEV_INPUT_EVENT);
		return FALSE;
    }
    else if (i == 0) {
		ErrorF("%s The /dev/input/event* device nodes seem to be missing\n",
				local->name);
		free(namelist);
		return FALSE;
    }

    while (i--) {
		char fname[64];
		int fd = -1;

		if (!touchpad_found) {
			sprintf(fname, "%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
			SYSCALL(fd = open(fname, O_RDONLY));
			if (fd < 0)
				continue;

			if (event_query_is_touchpad(fd)) {
				touchpad_found = TRUE;
			    xf86Msg(X_PROBED, "%s auto-dev sets device to %s\n",
				    local->name, fname);
			    local->options =
			    	xf86ReplaceStrOption(local->options, "Device", fname);
			}
			SYSCALL(close(fd));
		}
		free(namelist[i]);
    }
	free(namelist);

	if (!touchpad_found) {
		ErrorF("%s no synaptics event device found\n", local->name);
		return FALSE;
	}
    return TRUE;
}

struct SynapticsProtocolOperations event_proto_operations = {
    EventDeviceOnHook,
    NULL,
    EventQueryHardware,
    EventReadHwState,
    EventAutoDevProbe,
    EventReadDevDimensions
};
