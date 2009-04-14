/*
 * Copyright © 2002-2005,2007 Peter Osterlund
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#include <X11/Xdefs.h>
#include <X11/Xatom.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include "synaptics.h"
#include "synaptics-properties.h"
#ifdef HAVE_PROPERTIES
#include <xserver-properties.h>

#ifndef XATOM_FLOAT
#define XATOM_FLOAT "FLOAT"
#endif
#endif

enum ParaType {
    PT_INT,
    PT_BOOL,
    PT_DOUBLE
};

struct Parameter {
    char *name;				    /* Name of parameter */
    int offset;				    /* Offset in shared memory area */
    enum ParaType type;			    /* Type of parameter */
    double min_val;			    /* Minimum allowed value */
    double max_val;			    /* Maximum allowed value */
    char *prop_name;			    /* Property name */
    int prop_format;			    /* Property format (0 for floats) */
    int prop_offset;			    /* Offset inside property */
};

#define DEFINE_PAR(name, memb, type, min_val, max_val, pname, pformat, poffset) \
{ name, offsetof(SynapticsSHM, memb), (type), (min_val), (max_val), \
  (pname), (pformat), (poffset) }

static struct Parameter params[] = {
    DEFINE_PAR("LeftEdge",             left_edge,               PT_INT,    0, 10000,
		SYNAPTICS_PROP_EDGES,		32,	0),
    DEFINE_PAR("RightEdge",            right_edge,              PT_INT,    0, 10000,
		SYNAPTICS_PROP_EDGES,		32,	1),
    DEFINE_PAR("TopEdge",              top_edge,                PT_INT,    0, 10000,
		SYNAPTICS_PROP_EDGES,		32,	2),
    DEFINE_PAR("BottomEdge",           bottom_edge,             PT_INT,    0, 10000,
		SYNAPTICS_PROP_EDGES,		32,	3),
    DEFINE_PAR("FingerLow",            finger_low,              PT_INT,    0, 255,
		SYNAPTICS_PROP_FINGER,		32,	0),
    DEFINE_PAR("FingerHigh",           finger_high,             PT_INT,    0, 255,
		SYNAPTICS_PROP_FINGER,		32,	1),
    DEFINE_PAR("FingerPress",          finger_press,            PT_INT,    0, 256,
		SYNAPTICS_PROP_FINGER,		32,	2),
    DEFINE_PAR("MaxTapTime",           tap_time,                PT_INT,    0, 1000,
		SYNAPTICS_PROP_TAP_TIME,	32,	0),
    DEFINE_PAR("MaxTapMove",           tap_move,                PT_INT,    0, 2000,
		SYNAPTICS_PROP_TAP_MOVE,	32,	0),
    DEFINE_PAR("MaxDoubleTapTime",     tap_time_2,              PT_INT,    0, 1000,
		SYNAPTICS_PROP_TAP_DURATIONS,	32,	1),
    DEFINE_PAR("SingleTapTimeout",     single_tap_timeout,      PT_INT,    0, 1000,
		SYNAPTICS_PROP_TAP_DURATIONS,	32,	0),
    DEFINE_PAR("ClickTime",            click_time,              PT_INT,    0, 1000,
		SYNAPTICS_PROP_TAP_DURATIONS,	32,	2),
    DEFINE_PAR("FastTaps",             fast_taps,               PT_BOOL,   0, 1,
		SYNAPTICS_PROP_TAP_FAST,	8,	0),
    DEFINE_PAR("EmulateMidButtonTime", emulate_mid_button_time, PT_INT,    0, 1000,
		SYNAPTICS_PROP_MIDDLE_TIMEOUT,	32,	0),
    DEFINE_PAR("EmulateTwoFingerMinZ", emulate_twofinger_z,     PT_INT,    0, 1000,
		SYNAPTICS_PROP_TWOFINGER_PRESSURE,	32,	0),
    DEFINE_PAR("EmulateTwoFingerMinW", emulate_twofinger_w,     PT_INT,    0, 15,
		SYNAPTICS_PROP_TWOFINGER_WIDTH,	32,	0),
    DEFINE_PAR("VertScrollDelta",      scroll_dist_vert,        PT_INT,    0, 1000,
		SYNAPTICS_PROP_SCROLL_DISTANCE,	32,	0),
    DEFINE_PAR("HorizScrollDelta",     scroll_dist_horiz,       PT_INT,    0, 1000,
		SYNAPTICS_PROP_SCROLL_DISTANCE,	32,	1),
    DEFINE_PAR("VertEdgeScroll",       scroll_edge_vert,        PT_BOOL,   0, 1,
		SYNAPTICS_PROP_SCROLL_EDGE,	8,	0),
    DEFINE_PAR("HorizEdgeScroll",      scroll_edge_horiz,       PT_BOOL,   0, 1,
		SYNAPTICS_PROP_SCROLL_EDGE,	8,	1),
    DEFINE_PAR("CornerCoasting",       scroll_edge_corner,      PT_BOOL,   0, 1,
		SYNAPTICS_PROP_SCROLL_EDGE,	8,	2),
    DEFINE_PAR("VertTwoFingerScroll",  scroll_twofinger_vert,   PT_BOOL,   0, 1,
		SYNAPTICS_PROP_SCROLL_TWOFINGER,	8,	0),
    DEFINE_PAR("HorizTwoFingerScroll", scroll_twofinger_horiz,  PT_BOOL,   0, 1,
		SYNAPTICS_PROP_SCROLL_TWOFINGER,	8,	1),
    DEFINE_PAR("MinSpeed",             min_speed,               PT_DOUBLE, 0, 1.0,
		SYNAPTICS_PROP_SPEED,		0, /*float */	0),
    DEFINE_PAR("MaxSpeed",             max_speed,               PT_DOUBLE, 0, 1.0,
		SYNAPTICS_PROP_SPEED,		0, /*float */	1),
    DEFINE_PAR("AccelFactor",          accl,                    PT_DOUBLE, 0, 0.2,
		SYNAPTICS_PROP_SPEED,		0, /*float */	2),
    DEFINE_PAR("TrackstickSpeed",      trackstick_speed,        PT_DOUBLE, 0, 200.0,
		SYNAPTICS_PROP_SPEED,		0, /*float */ 3),
    DEFINE_PAR("EdgeMotionMinZ",       edge_motion_min_z,       PT_INT,    1, 255,
		SYNAPTICS_PROP_EDGEMOTION_PRESSURE, 32,	0),
    DEFINE_PAR("EdgeMotionMaxZ",       edge_motion_max_z,       PT_INT,    1, 255,
		SYNAPTICS_PROP_EDGEMOTION_PRESSURE, 32,	1),
    DEFINE_PAR("EdgeMotionMinSpeed",   edge_motion_min_speed,   PT_INT,    0, 1000,
		SYNAPTICS_PROP_EDGEMOTION_SPEED, 32,	0),
    DEFINE_PAR("EdgeMotionMaxSpeed",   edge_motion_max_speed,   PT_INT,    0, 1000,
		SYNAPTICS_PROP_EDGEMOTION_SPEED, 32,	1),
    DEFINE_PAR("EdgeMotionUseAlways",  edge_motion_use_always,  PT_BOOL,   0, 1,
		SYNAPTICS_PROP_EDGEMOTION,	 8,	0),
    DEFINE_PAR("UpDownScrolling",      updown_button_scrolling, PT_BOOL,   0, 1,
		SYNAPTICS_PROP_BUTTONSCROLLING,	 8,	0),
    DEFINE_PAR("LeftRightScrolling",   leftright_button_scrolling, PT_BOOL,   0, 1,
		SYNAPTICS_PROP_BUTTONSCROLLING,	 8,	1),
    DEFINE_PAR("UpDownScrollRepeat",   updown_button_repeat,    PT_BOOL,   0, 1,
		SYNAPTICS_PROP_BUTTONSCROLLING_REPEAT, 8,	0),
    DEFINE_PAR("LeftRightScrollRepeat",leftright_button_repeat, PT_BOOL,   0, 1,
		SYNAPTICS_PROP_BUTTONSCROLLING_REPEAT, 8,	1),
    DEFINE_PAR("ScrollButtonRepeat",   scroll_button_repeat,    PT_INT,    SBR_MIN , SBR_MAX,
		SYNAPTICS_PROP_BUTTONSCROLLING_TIME, 32,	0),
    DEFINE_PAR("TouchpadOff",          touchpad_off,            PT_INT,    0, 2,
		SYNAPTICS_PROP_OFF,		8,	0),
    DEFINE_PAR("GuestMouseOff",        guestmouse_off,          PT_BOOL,   0, 1,
		SYNAPTICS_PROP_GUESTMOUSE,	8,	0),
    DEFINE_PAR("LockedDrags",          locked_drags,            PT_BOOL,   0, 1,
		SYNAPTICS_PROP_LOCKED_DRAGS,	8,	0),
    DEFINE_PAR("LockedDragTimeout",    locked_drag_time,        PT_INT,    0, 30000,
		SYNAPTICS_PROP_LOCKED_DRAGS_TIMEOUT,	32,	0),
    DEFINE_PAR("RTCornerButton",       tap_action[RT_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	0),
    DEFINE_PAR("RBCornerButton",       tap_action[RB_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	1),
    DEFINE_PAR("LTCornerButton",       tap_action[LT_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	2),
    DEFINE_PAR("LBCornerButton",       tap_action[LB_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	3),
    DEFINE_PAR("TapButton1",           tap_action[F1_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	4),
    DEFINE_PAR("TapButton2",           tap_action[F2_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	5),
    DEFINE_PAR("TapButton3",           tap_action[F3_TAP],      PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_TAP_ACTION,	8,	6),
    DEFINE_PAR("ClickFinger1",         click_action[F1_CLICK1], PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_CLICK_ACTION,	8,	0),
    DEFINE_PAR("ClickFinger2",         click_action[F2_CLICK1], PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_CLICK_ACTION,	8,	1),
    DEFINE_PAR("ClickFinger3",         click_action[F3_CLICK1], PT_INT,    0, SYN_MAX_BUTTONS,
		SYNAPTICS_PROP_CLICK_ACTION,	8,	2),
    DEFINE_PAR("CircularScrolling",    circular_scrolling,      PT_BOOL,   0, 1,
		SYNAPTICS_PROP_CIRCULAR_SCROLLING,	8,	0),
    DEFINE_PAR("CircScrollDelta",      scroll_dist_circ,        PT_DOUBLE, .01, 3,
		SYNAPTICS_PROP_CIRCULAR_SCROLLING_DIST,	0 /* float */,	0),
    DEFINE_PAR("CircScrollTrigger",    circular_trigger,        PT_INT,    0, 8,
		SYNAPTICS_PROP_CIRCULAR_SCROLLING_TRIGGER,	8,	0),
    DEFINE_PAR("CircularPad",          circular_pad,            PT_BOOL,   0, 1,
		SYNAPTICS_PROP_CIRCULAR_PAD,	8,	0),
    DEFINE_PAR("PalmDetect",           palm_detect,             PT_BOOL,   0, 1,
		SYNAPTICS_PROP_PALM_DETECT,	8,	0),
    DEFINE_PAR("PalmMinWidth",         palm_min_width,          PT_INT,    0, 15,
		SYNAPTICS_PROP_PALM_DIMENSIONS,	32,	0),
    DEFINE_PAR("PalmMinZ",             palm_min_z,              PT_INT,    0, 255,
		SYNAPTICS_PROP_PALM_DIMENSIONS,	32,	1),
    DEFINE_PAR("CoastingSpeed",        coasting_speed,          PT_DOUBLE, 0, 20,
		SYNAPTICS_PROP_COASTING_SPEED,	0 /* float*/,	0),
    DEFINE_PAR("PressureMotionMinZ",   press_motion_min_z,      PT_INT,    1, 255,
		SYNAPTICS_PROP_PRESSURE_MOTION,	32,	0),
    DEFINE_PAR("PressureMotionMaxZ",   press_motion_max_z,      PT_INT,    1, 255,
		SYNAPTICS_PROP_PRESSURE_MOTION,	32,	1),
    DEFINE_PAR("PressureMotionMinFactor", press_motion_min_factor, PT_DOUBLE, 0, 10.0,
		SYNAPTICS_PROP_PRESSURE_MOTION_FACTOR,	0 /*float*/,	0),
    DEFINE_PAR("PressureMotionMaxFactor", press_motion_max_factor, PT_DOUBLE, 0, 10.0,
		SYNAPTICS_PROP_PRESSURE_MOTION_FACTOR,	0 /*float*/,	1),
    DEFINE_PAR("GrabEventDevice",      grab_event_device,       PT_BOOL,   0, 1,
		SYNAPTICS_PROP_GRAB,	8,	0),
    { NULL, 0, 0, 0, 0 }
};

static void
shm_show_hw_info(SynapticsSHM *synshm)
{
    printf("Hardware properties:\n");
    if (synshm->synhw.model_id) {
	printf("    Model Id     = %08x\n", synshm->synhw.model_id);
	printf("    Capabilities = %08x\n", synshm->synhw.capabilities);
	printf("    Identity     = %08x\n", synshm->synhw.identity);
    } else {
	printf("    Can't detect hardware properties.\n");
	printf("    This is normal if you are running linux kernel 2.6.\n");
	printf("    Check the kernel log for touchpad hardware information.\n");
    }
}

static void
shm_show_settings(SynapticsSHM *synshm)
{
    int i;

    printf("Parameter settings:\n");
    for (i = 0; params[i].name; i++) {
	struct Parameter* par = &params[i];
	switch (par->type) {
	case PT_INT:
	    printf("    %-23s = %d\n", par->name, *(int*)((char*)synshm + par->offset));
	    break;
	case PT_BOOL:
	    printf("    %-23s = %d\n", par->name, *(Bool*)((char*)synshm + par->offset));
	    break;
	case PT_DOUBLE:
	    printf("    %-23s = %g\n", par->name, *(double*)((char*)synshm + par->offset));
	    break;
	}
    }
}

static double
parse_cmd(char* cmd, struct Parameter** par)
{
    char *eqp = index(cmd, '=');
    *par = NULL;

    if (eqp) {
	int j;
	int found = 0;
	*eqp = 0;
	for (j = 0; params[j].name; j++) {
	    if (strcasecmp(cmd, params[j].name) == 0) {
		found = 1;
		break;
	    }
	}
	if (found) {
	    double val = atof(&eqp[1]);
	    *par = &params[j];

	    if (val < (*par)->min_val)
		val = (*par)->min_val;
	    if (val > (*par)->max_val)
		val = (*par)->max_val;

	    return val;
	} else {
	    printf("Unknown parameter %s\n", cmd);
	}
    } else {
	printf("Invalid command: %s\n", cmd);
    }

    return 0;
}

static void
shm_set_variables(SynapticsSHM *synshm, int argc, char *argv[], int first_cmd)
{
    int i;
    struct Parameter *par;
    double val;

    for (i = first_cmd; i < argc; i++) {
	val = parse_cmd(argv[i], &par);

	if (!par)
	    continue;

	switch (par->type) {
	    case PT_INT:
		*(int*)((char*)synshm + par->offset) = (int)rint(val);
		break;
	    case PT_BOOL:
		*(Bool*)((char*)synshm + par->offset) = (Bool)rint(val);
		break;
	    case PT_DOUBLE:
		*(double*)((char*)synshm + par->offset) = val;
		break;
	}
    }
}

static int
is_equal(SynapticsSHM *s1, SynapticsSHM *s2)
{
    int i;

    if ((s1->x           != s2->x) ||
	(s1->y           != s2->y) ||
	(s1->z           != s2->z) ||
	(s1->numFingers  != s2->numFingers) ||
	(s1->fingerWidth != s2->fingerWidth) ||
	(s1->left        != s2->left) ||
	(s1->right       != s2->right) ||
	(s1->up          != s2->up) ||
	(s1->down        != s2->down) ||
	(s1->middle      != s2->middle) ||
	(s1->guest_left  != s2->guest_left) ||
	(s1->guest_mid   != s2->guest_mid) ||
	(s1->guest_right != s2->guest_right) ||
	(s1->guest_dx    != s2->guest_dx) ||
	(s1->guest_dy    != s2->guest_dy))
	return 0;

    for (i = 0; i < 8; i++)
	if (s1->multi[i] != s2->multi[i])
	    return 0;

    return 1;
}

static double
get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static void
shm_monitor(SynapticsSHM *synshm, int delay)
{
    int header = 0;
    SynapticsSHM old;
    double t0 = get_time();

    memset(&old, 0, sizeof(SynapticsSHM));
    old.x = -1;				    /* Force first equality test to fail */

    while (1) {
	SynapticsSHM cur = *synshm;
	if (!is_equal(&old, &cur)) {
	    if (!header) {
		printf("%8s  %4s %4s %3s %s %2s %2s %s %s %s %s  %8s  "
		       "%2s %2s %2s %3s %3s\n",
		       "time", "x", "y", "z", "f", "w", "l", "r", "u", "d", "m",
		       "multi", "gl", "gm", "gr", "gdx", "gdy");
		header = 20;
	    }
	    header--;
	    printf("%8.3f  %4d %4d %3d %d %2d %2d %d %d %d %d  %d%d%d%d%d%d%d%d  "
		   "%2d %2d %2d %3d %3d\n",
		   get_time() - t0,
		   cur.x, cur.y, cur.z, cur.numFingers, cur.fingerWidth,
		   cur.left, cur.right, cur.up, cur.down, cur.middle,
		   cur.multi[0], cur.multi[1], cur.multi[2], cur.multi[3],
		   cur.multi[4], cur.multi[5], cur.multi[6], cur.multi[7],
		   cur.guest_left, cur.guest_mid, cur.guest_right,
		   cur.guest_dx, cur.guest_dy);
	    fflush(stdout);
	    old = cur;
	}
	usleep(delay * 1000);
    }
}

/** Init and return SHM area or NULL on error */
static  SynapticsSHM*
shm_init()
{
    SynapticsSHM *synshm = NULL;
    int shmid = 0;

    if ((shmid = shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0)) == -1) {
	if ((shmid = shmget(SHM_SYNAPTICS, 0, 0)) == -1)
	    fprintf(stderr, "Can't access shared memory area. SHMConfig disabled?\n");
	else
	    fprintf(stderr, "Incorrect size of shared memory area. Incompatible driver version?\n");
    } else if ((synshm = (SynapticsSHM*) shmat(shmid, NULL, 0)) == NULL)
	perror("shmat");

    return synshm;
}


#ifdef HAVE_PROPERTIES
/** Init display connection or NULL on error */
static Display*
dp_init()
{
    Display *dpy		= NULL;
    XExtensionVersion *v	= NULL;
    Atom touchpad_type		= 0;
    Atom synaptics_property	= 0;
    int error			= 0;

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
	fprintf(stderr, "Failed to connect to X Server.\n");
	error = 1;
	goto unwind;
    }

    v = XGetExtensionVersion(dpy, INAME);
    if (!v->present ||
	(v->major_version * 1000 + v->minor_version) < (XI_Add_DeviceProperties_Major * 1000
	    + XI_Add_DeviceProperties_Minor)) {
	fprintf(stderr, "X server supports X Input %d.%d. I need %d.%d.\n",
		v->major_version, v->minor_version,
		XI_Add_DeviceProperties_Major,
		XI_Add_DeviceProperties_Minor);
	error = 1;
	goto unwind;
    }

    /* We know synaptics sets XI_TOUCHPAD for all the devices. */
    touchpad_type = XInternAtom(dpy, XI_TOUCHPAD, True);
    if (!touchpad_type) {
	fprintf(stderr, "XI_TOUCHPAD not initialised.\n");
	error = 1;
	goto unwind;
    }

    synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_EDGES, True);
    if (!synaptics_property) {
	fprintf(stderr, "Couldn't find synaptics properties. No synaptics "
		"driver loaded?\n");
	error = 1;
	goto unwind;
    }

unwind:
    XFree(v);
    if (error && dpy)
    {
	XCloseDisplay(dpy);
	dpy = NULL;
    }
    return dpy;
}

static XDevice *
dp_get_device(Display *dpy)
{
    XDevice* dev		= NULL;
    XDeviceInfo *info		= NULL;
    int ndevices		= 0;
    Atom touchpad_type		= 0;
    Atom synaptics_property	= 0;
    Atom *properties		= NULL;
    int nprops			= 0;
    int error			= 0;

    touchpad_type = XInternAtom(dpy, XI_TOUCHPAD, True);
    synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_EDGES, True);
    info = XListInputDevices(dpy, &ndevices);

    while(ndevices--) {
	if (info[ndevices].type == touchpad_type) {
	    dev = XOpenDevice(dpy, info[ndevices].id);
	    if (!dev) {
		fprintf(stderr, "Failed to open device '%s'.\n",
			info[ndevices].name);
		error = 1;
		goto unwind;
	    }

	    properties = XListDeviceProperties(dpy, dev, &nprops);
	    if (!properties || !nprops)
	    {
		fprintf(stderr, "No properties on device '%s'.\n",
			info[ndevices].name);
		error = 1;
		goto unwind;
	    }

	    while(nprops--)
	    {
		if (properties[nprops] == synaptics_property)
		    break;
	    }
	    if (!nprops)
	    {
		fprintf(stderr, "No synaptics properties on device '%s'.\n",
			info[ndevices].name);
		error = 1;
		goto unwind;
	    }

	    break; /* Yay, device is suitable */
	}
    }

unwind:
    XFree(properties);
    XFreeDeviceList(info);
    if (!dev)
        fprintf(stderr, "Unable to find a synaptics device.\n");
    else if (error && dev)
    {
	XCloseDevice(dpy, dev);
	dev = NULL;
    }
    return dev;
}

static void
dp_set_variables(Display *dpy, XDevice* dev, int argc, char *argv[], int first_cmd)
{
    int i;
    double val;
    struct Parameter *par;
    Atom prop, type, float_type;
    int format;
    unsigned char* data;
    unsigned long nitems, bytes_after;

    float *f;
    long *n;
    char *b;

    float_type = XInternAtom(dpy, XATOM_FLOAT, True);
    if (!float_type)
	fprintf(stderr, "Float properties not available.\n");

    for (i = first_cmd; i < argc; i++) {
	val = parse_cmd(argv[i], &par);
	if (!par)
	    continue;

	prop = XInternAtom(dpy, par->prop_name, True);
	if (!prop)
	{
	    fprintf(stderr, "Property for '%s' not available. Skipping.\n",
		    par->name);
	    continue;

	}

	XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
				&type, &format, &nitems, &bytes_after, &data);

	switch(par->prop_format)
	{
	    case 8:
		if (format != par->prop_format || type != XA_INTEGER) {
		    fprintf(stderr, "   %-23s = format mismatch (%d)\n",
			    par->name, format);
		    break;
		}
		b = (char*)data;
		b[par->prop_offset] = rint(val);
		break;
	    case 32:
		if (format != par->prop_format || type != XA_INTEGER) {
		    fprintf(stderr, "   %-23s = format mismatch (%d)\n",
			    par->name, format);
		    break;
		}
		n = (long*)data;
		n[par->prop_offset] = rint(val);
		break;
	    case 0: /* float */
		if (!float_type)
		    continue;
		if (format != 32 || type != float_type) {
		    fprintf(stderr, "   %-23s = format mismatch (%d)\n",
			    par->name, format);
		    break;
		}
		f = (float*)data;
		f[par->prop_offset] = val;
		break;
	}

	XChangeDeviceProperty(dpy, dev, prop, type, format,
				PropModeReplace, data, nitems);
	XFlush(dpy);
    }
}

/* FIXME: horribly inefficient. */
static void
dp_show_settings(Display *dpy, XDevice *dev)
{
    int j;
    Atom a, type, float_type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char* data;
    int len;

    float *f;
    long *i;
    char *b;

    float_type = XInternAtom(dpy, XATOM_FLOAT, True);
    if (!float_type)
	fprintf(stderr, "Float properties not available.\n");

    printf("Parameter settings:\n");
    for (j = 0; params[j].name; j++) {
	struct Parameter *par = &params[j];
	a = XInternAtom(dpy, par->prop_name, True);
	if (!a) {
	    fprintf(stderr, "    %-23s = missing\n",
		    par->name);
	    continue;
	}

	len = 1 + ((par->prop_offset * (par->prop_format ? par->prop_format : 32)/8))/4;

	XGetDeviceProperty(dpy, dev, a, 0, len, False,
				AnyPropertyType, &type, &format,
				&nitems, &bytes_after, &data);

	switch(par->prop_format) {
	    case 8:
		if (format != par->prop_format || type != XA_INTEGER) {
		    fprintf(stderr, "   %-23s = format mismatch (%d)\n",
			    par->name, format);
		    break;
		}

		b = (char*)data;
		printf("    %-23s = %d\n", par->name, b[par->prop_offset]);
		break;
	    case 32:
		if (format != par->prop_format || type != XA_INTEGER) {
		    fprintf(stderr, "   %-23s = format mismatch (%d)\n",
			    par->name, format);
		    break;
		}

		i = (long*)data;
		printf("    %-23s = %ld\n", par->name, i[par->prop_offset]);
		break;
	    case 0: /* Float */
		if (!float_type)
		    continue;
		if (format != 32 || type != float_type) {
		    fprintf(stderr, "   %-23s = format mismatch (%d)\n",
			    par->name, format);
		    break;
		}

		f = (float*)data;
		printf("    %-23s = %g\n", par->name, f[par->prop_offset]);
		break;
	}

	XFree(data);
    }
}
#endif

static void
usage(void)
{
    fprintf(stderr, "Usage: synclient [-s] [-m interval] [-h] [-l] [-V] [-?] [var1=value1 [var2=value2] ...]\n");
#ifdef HAVE_PROPERTIES
    fprintf(stderr, "  -s Use SHM area instead of device properties.\n");
#endif
    fprintf(stderr, "  -m monitor changes to the touchpad state (implies -s)\n"
	    "     interval specifies how often (in ms) to poll the touchpad state\n");
    fprintf(stderr, "  -h Show detected hardware properties (implies -s)\n");
    fprintf(stderr, "  -l List current user settings\n");
    fprintf(stderr, "  -V Print synclient version string and exit\n");
    fprintf(stderr, "  -? Show this help message\n");
    fprintf(stderr, "  var=value  Set user parameter 'var' to 'value'.\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    int c;
    int delay = -1;
    int do_monitor = 0;
    int dump_hw = 0;
    int dump_settings = 0;
    int use_shm = 1;
    int first_cmd;

#ifdef HAVE_PROPERTIES
    use_shm = 0;
#endif

    /* Parse command line parameters */
    while ((c = getopt(argc, argv, "sm:hlV")) != -1) {
	switch (c) {
	case 's':
	    use_shm = 1;
	    break;
	case 'm':
	    use_shm = 1;
	    do_monitor = 1;
	    if ((delay = atoi(optarg)) < 0)
		usage();
	    break;
	case 'h':
	    use_shm = 1;
	    dump_hw = 1;
	    break;
	case 'l':
	    dump_settings = 1;
	    break;
	case 'V':
	    printf("%s\n", VERSION);
	    exit(0);
	default:
	    usage();
	}
    }

    first_cmd = optind;
    if (!do_monitor && !dump_hw && !dump_settings && first_cmd == argc)
	usage();

    /* Connect to the shared memory area */
    if (use_shm)
    {
	SynapticsSHM *synshm = NULL;

	synshm = shm_init();
	if (!synshm)
	    return 1;

	/* Perform requested actions */
	if (dump_hw)
	    shm_show_hw_info(synshm);

	shm_set_variables(synshm, argc, argv, first_cmd);

	if (dump_settings)
	    shm_show_settings(synshm);
	if (do_monitor)
	    shm_monitor(synshm, delay);
    }
#ifdef HAVE_PROPERTIES
    else /* Device properties */
    {
	Display *dpy;
	XDevice *dev;

	dpy = dp_init();
	if (!dpy || !(dev = dp_get_device(dpy)))
	    return 1;

	dp_set_variables(dpy, dev, argc, argv, first_cmd);
	if (dump_settings)
	    dp_show_settings(dpy, dev);

	XCloseDevice(dpy, dev);
	XCloseDisplay(dpy);
    }
#endif

    return 0;
}
