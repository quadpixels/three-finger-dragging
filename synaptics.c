/*
 *   Copyright 2004 Matthias Ihmig <m.ihmig@gmx.net>
 *     patch for pressure dependent EdgeMotion speed
 *
 *   Copyright 2004 Alexei Gilchrist <alexei@physics.uq.edu.au>
 *     patch for circular scrolling
 *
 *   Copyright 2003 Jörg Bösner <ich@joerg-boesner.de>
 *     patch for switching the touchpad off (for example, when a
 *     USB mouse is connected)
 *
 *   Copyright 2003 Hartwig Felger <hgfelger@hgfelger.de>
 *     patch to make the horizontal wheel replacement buttons work.
 *
 *   Copyright 2002 Peter Osterlund <petero2@telia.com>
 *     patches for fast scrolling, palm detection, edge motion,
 *     horizontal scrolling
 *
 *   Copyright 2002 S. Lehner <sam_x@bluemail.ch>
 *     for newer Firmware (5.8) protocol changes for 3rd to 6th button
 *
 *   Copyright (C) 2001 Stefan Gmeiner <riddlebox@freesurf.ch>
 *     start merging tpconfig and gpm code to an xfree input module
 *     adding some changes and extensions (ex. 3rd and 4th button)
 *
 *   Copyright (c) 1999 Henry Davies <hdavies@ameritech.net> for the
 *     absolute to relative translation code (from the gpm source)
 *     and some other ideas
 *
 *   Synaptics Passthrough Support
 *   Copyright (c) 2002 Linuxcare Inc. David Kennedy <dkennedy@linuxcare.com>
 *   adapted to version 0.12.1
 *   Copyright (c) 2003 Fred Hucht <fred@thp.Uni-Duisburg.de>
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
 *
 * Trademarks are the property of their respective owners.
 *
 */


/*****************************************************************************
 *	Standard Headers
 ****************************************************************************/

#include <sys/ioctl.h>
#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <xisb.h>
#include "mipointer.h"
#ifdef XFREE_4_0_3
#include <xf86Optrec.h>  		/* needed for Options */
#endif


/*****************************************************************************
 *	Local Headers
 ****************************************************************************/
#define SYNAPTICS_PRIVATE
#include "synaptics.h"
#include "ps2comm.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables and Types
 ****************************************************************************/

/*
 * The x/y limits are taken from the Synaptics TouchPad interfacing Guide,
 * section 2.3.2, which says that they should be valid regardless of the
 * actual size of the sensor.
 */
#define XMIN_NOMINAL 1472
#define XMAX_NOMINAL 5472
#define YMIN_NOMINAL 1408
#define YMAX_NOMINAL 4448

#define XMAX_VALID 6143

#define MAX_UNSYNC_PACKETS 10				/* i.e. 10 to 60 bytes */

typedef enum {
    BOTTOM_EDGE = 1,
    TOP_EDGE = 2,
    LEFT_EDGE = 4,
    RIGHT_EDGE = 8,
    LEFT_BOTTOM_EDGE = BOTTOM_EDGE | LEFT_EDGE,
    RIGHT_BOTTOM_EDGE = BOTTOM_EDGE | RIGHT_EDGE,
    RIGHT_TOP_EDGE = TOP_EDGE | RIGHT_EDGE,
    LEFT_TOP_EDGE = TOP_EDGE | LEFT_EDGE
} edge_type;

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define TIME_DIFF(a, b) ((long)((a)-(b)))
#define SYSCALL(call) while (((call) == -1) && (errno == EINTR))

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* for auto-dev: */
#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

/*****************************************************************************
 * Forward declaration
 ****************************************************************************/
static InputInfoPtr
SynapticsPreInit(InputDriverPtr drv, IDevPtr dev, int flags);


InputDriverRec SYNAPTICS = {
    1,
    "synaptics",
    NULL,
    SynapticsPreInit,
    /*SynapticsUnInit*/ NULL,
    NULL,
    0
};

#ifdef XFree86LOADER

static XF86ModuleVersionInfo VersionRec = {
    "synaptics",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XF86_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}				/* signature, to be patched into the file by
						 * a tool */
};


static pointer
SetupProc(pointer module, pointer options, int *errmaj, int *errmin)
{
    xf86AddInputDriver(&SYNAPTICS, module, 0);
    return module;
}

XF86ModuleData synapticsModuleData = {&VersionRec, &SetupProc, NULL };

#endif /* XFree86LOADER */


/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

static void
SetDeviceAndProtocol(LocalDevicePtr local)
{
    char *str_par;
    SynapticsPrivate *priv = local->private;

    priv->proto = SYN_PROTO_PSAUX;
    str_par = xf86FindOptionValue(local->options, "Protocol");
    if (str_par && !strcmp(str_par, "event")) {
	priv->proto = SYN_PROTO_EVENT;
    } else if (str_par && !strcmp(str_par, "psaux")) {
	/* Already set up */
    } else { /* default to auto-dev */
	/* We are trying to find the right eventX Device, or fall back to
	   the psaux Protocol and the given Device from XF86Config */
	int fd = -1;
	int i;
	for (i = 0; ; i++) {
	    char fname[64];
	    struct input_id id;
	    int ret;

	    sprintf(fname, "%s/%s%d", DEV_INPUT_EVENT, EVENT_DEV_NAME, i);
	    SYSCALL(fd = open(fname, O_RDONLY));
	    if (fd < 0) {
		if (errno == ENOENT) {
		    ErrorF("%s no synaptics event device found (checked %d nodes)\n",
			   local->name, i + 1);
		    break;
		} else {
		    continue;
		}
	    }
	    SYSCALL(ret = ioctl(fd, EVIOCGID, &id));
	    SYSCALL(close(fd));
	    if (ret >= 0) {
		if ((id.bustype == BUS_I8042) &&
		    (id.vendor == 0x0002) &&
		    (id.product == PSMOUSE_SYNAPTICS)) {
		    priv->proto = SYN_PROTO_EVENT;
		    xf86Msg(X_PROBED, "%s auto-dev sets Synaptics Device to %s\n",
			    local->name, fname);
		    xf86ReplaceStrOption(local->options, "Device", fname);
		    break;
		}
	    }
	}
    }
}

/*
 *  called by the module loader for initialization
 */
static InputInfoPtr
SynapticsPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
    LocalDevicePtr local;
    SynapticsPrivate *priv;
#ifdef XFREE_4_0_3
    XF86OptionPtr optList;
#else
    pointer optList;
#endif
    char *str_par;
    int shmid;
    unsigned long now;

    /* allocate memory for SynaticsPrivateRec */
    priv = xcalloc(1, sizeof(SynapticsPrivate));
    if (!priv)
	return NULL;

    /* Allocate a new InputInfoRec and add it to the head xf86InputDevs. */
    local = xf86AllocateInput(drv, 0);
    if (!local) {
	xfree(priv);
	return NULL;
    }

    /* initialize the InputInfoRec */
    local->name                    = dev->identifier;
    local->type_name               = XI_MOUSE; /* XI_TOUCHPAD and KDE killed the X Server at startup ? */
    local->device_control          = DeviceControl;
    local->read_input              = ReadInput;
    local->control_proc            = ControlProc;
    local->close_proc              = CloseProc;
    local->switch_mode             = SwitchMode;
    local->conversion_proc         = ConvertProc;
    local->reverse_conversion_proc = NULL;
    local->dev                     = NULL;
    local->private                 = priv;
    local->private_flags           = 0;
    local->flags                   = XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
    local->conf_idev               = dev;
    local->motion_history_proc     = xf86GetMotionEvents;
    local->history_size            = 0;
    local->always_core_feedback    = 0;

    xf86Msg(X_INFO, "Synaptics touchpad driver version %s\n", VERSION);

    xf86CollectInputOptions(local, NULL, NULL);

    xf86OptionListReport(local->options);

    SetDeviceAndProtocol(local);

    /* open the touchpad device */
    local->fd = xf86OpenSerial(local->options);
    if (local->fd == -1) {
	ErrorF("Synaptics driver unable to open device\n");
	goto SetupProc_fail;
    }
    xf86ErrorFVerb( 6, "port opened successfully\n" );

    /* initialize variables */
    priv->timer = NULL;
    priv->repeatButtons = 0;
    priv->nextRepeat = 0;
    now = GetTimeInMillis();
    priv->count_packet_finger = 0;
    priv->tapping_millis = now;
    priv->button_delay_millis = now;
    priv->touch_on.millis = now;
    priv->hasGuest = FALSE;

    /* install shared memory or normal memory for parameter */
    priv->shm_config = FALSE;
    if (xf86SetBoolOption(local->options, "SHMConfig", FALSE)) {
	if ((shmid = xf86shmget(SHM_SYNAPTICS, 0, 0)) != -1)
	    xf86shmctl(shmid, XF86IPC_RMID, NULL);
	if ((shmid = xf86shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM),
				0777 | XF86IPC_CREAT)) == -1) {
	    xf86Msg(X_ERROR, "%s error shmget\n", local->name);
	    goto SetupProc_fail;
	}
	else if ((priv->synpara = (SynapticsSHM*) xf86shmat(shmid, NULL, 0)) == NULL) {
	    xf86Msg(X_ERROR, "%s error shmat\n", local->name);
	    goto SetupProc_fail;
	}
	priv->shm_config = TRUE;
    } else {
	priv->synpara = xcalloc(1, sizeof(SynapticsSHM));
	if (!priv->synpara)
	    goto SetupProc_fail;
    }

    /* read the parameter */
    priv->synpara->left_edge = xf86SetIntOption(local->options, "LeftEdge", 1900);
    priv->synpara->right_edge = xf86SetIntOption(local->options, "RightEdge", 5400);
    priv->synpara->top_edge = xf86SetIntOption(local->options, "TopEdge", 1900);
    priv->synpara->bottom_edge = xf86SetIntOption(local->options, "BottomEdge", 4000);
    priv->synpara->finger_low = xf86SetIntOption(local->options, "FingerLow", 25);
    priv->synpara->finger_high = xf86SetIntOption(local->options, "FingerHigh", 30);
    priv->synpara->tap_time = xf86SetIntOption(local->options, "MaxTapTime", 180);
    priv->synpara->tap_move = xf86SetIntOption(local->options, "MaxTapMove", 220);
    priv->synpara->emulate_mid_button_time = xf86SetIntOption(local->options,
							      "EmulateMidButtonTime", 75);
    priv->synpara->scroll_dist_vert = xf86SetIntOption(local->options, "VertScrollDelta", 100);
    priv->synpara->scroll_dist_horiz = xf86SetIntOption(local->options, "HorizScrollDelta", 100);
    priv->synpara->edge_motion_min_z = xf86SetIntOption(local->options, "EdgeMotionMinZ", 30);
    priv->synpara->edge_motion_max_z = xf86SetIntOption(local->options, "EdgeMotionMaxZ", 160);
    priv->synpara->edge_motion_min_speed = xf86SetIntOption(local->options, "EdgeMotionMinSpeed", 1);
    priv->synpara->edge_motion_max_speed = xf86SetIntOption(local->options, "EdgeMotionMaxSpeed", 200);
    priv->synpara->edge_motion_use_always = xf86SetBoolOption(local->options, "EdgeMotionUseAlways", FALSE);
    priv->synpara->repeater = xf86SetStrOption(local->options, "Repeater", NULL);
    priv->synpara->updown_button_scrolling = xf86SetBoolOption(local->options, "UpDownScrolling", TRUE);
    priv->synpara->touchpad_off = xf86SetBoolOption(local->options, "TouchpadOff", FALSE);
    priv->synpara->locked_drags = xf86SetBoolOption(local->options, "LockedDrags", FALSE);
    priv->synpara->tap_action[RT_TAP] = xf86SetIntOption(local->options, "RTCornerButton", 2);
    priv->synpara->tap_action[RB_TAP] = xf86SetIntOption(local->options, "RBCornerButton", 3);
    priv->synpara->tap_action[LT_TAP] = xf86SetIntOption(local->options, "LTCornerButton", 0);
    priv->synpara->tap_action[LB_TAP] = xf86SetIntOption(local->options, "LBCornerButton", 0);
    priv->synpara->tap_action[F1_TAP] = xf86SetIntOption(local->options, "TapButton1",     1);
    priv->synpara->tap_action[F2_TAP] = xf86SetIntOption(local->options, "TapButton2",     2);
    priv->synpara->tap_action[F3_TAP] = xf86SetIntOption(local->options, "TapButton3",     3);
    priv->synpara->circular_scrolling = xf86SetBoolOption(local->options, "CircularScrolling", FALSE);
    priv->synpara->circular_trigger   = xf86SetIntOption(local->options, "CircScrollTrigger", 0);

    str_par = xf86FindOptionValue(local->options, "MinSpeed");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->min_speed) != 1))
	priv->synpara->min_speed=0.02;
    str_par = xf86FindOptionValue(local->options, "MaxSpeed");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->max_speed) != 1))
	priv->synpara->max_speed=0.18;
    str_par = xf86FindOptionValue(local->options, "AccelFactor");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->accl) != 1))
	priv->synpara->accl=0.0015;
    str_par = xf86FindOptionValue(local->options, "CircScrollDelta");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->scroll_dist_circ) != 1))
	priv->synpara->scroll_dist_circ = 0.1;

    if (priv->synpara->circular_trigger < 0 || priv->synpara->circular_trigger > 8) {
	xf86Msg(X_WARNING, "Unknown circular scrolling trigger, using 0 (edges)");
	priv->synpara->circular_trigger = 0;
    }

    /* Warn about (and fix) incorrectly configured TopEdge/BottomEdge parameters */
    if (priv->synpara->top_edge > priv->synpara->bottom_edge) {
	int tmp = priv->synpara->top_edge;
	priv->synpara->top_edge = priv->synpara->bottom_edge;
	priv->synpara->bottom_edge = tmp;
	xf86Msg(X_WARNING, "%s: TopEdge is bigger than BottomEdge. Fixing.\n",
		local->name);
    }

    priv->largest_valid_x = MIN(priv->synpara->right_edge, XMAX_NOMINAL);

    priv->buffer = XisbNew(local->fd, 200);
    DBG(9, XisbTrace(priv->buffer, 1));

    if (priv->synpara->repeater) {
	/* create repeater fifo */
	if ((xf86mknod(priv->synpara->repeater, 666, XF86_S_IFIFO) != 0) &&
	    (xf86errno != xf86_EEXIST)) {
	    xf86Msg(X_ERROR, "%s can't create repeater fifo\n", local->name);
	    xf86free(priv->synpara->repeater);
	    priv->synpara->repeater = NULL;
	    priv->fifofd = -1;
	} else {
	    /* open the repeater fifo */
	    optList = xf86NewOption("Device", priv->synpara->repeater);
	    if ((priv->fifofd = xf86OpenSerial(optList)) == -1) {
		xf86Msg(X_ERROR, "%s repeater device open failed\n", local->name);
		xf86free(priv->synpara->repeater);
		priv->synpara->repeater = NULL;
		priv->fifofd = -1;
	    }
	}
    }

    if (QueryHardware(local) != Success) {
	xf86Msg(X_ERROR, "%s Unable to query/initialize Synaptics hardware.\n", local->name);
	goto SetupProc_fail;
    }

    local->history_size = xf86SetIntOption( local->options, "HistorySize", 0 );

    xf86ProcessCommonOptions(local, local->options);
    local->flags |= XI86_CONFIGURED;

    if (local->fd != -1) {
	xf86RemoveEnabledDevice(local);
	if (priv->buffer) {
	    XisbFree(priv->buffer);
	    priv->buffer = NULL;
	}
	xf86CloseSerial(local->fd);
    }
    local->fd = -1;
    return local;

 SetupProc_fail:
    if (local->fd >= 0) {
	RemoveEnabledDevice(local->fd);
	xf86CloseSerial(local->fd);
	local->fd = -1;
    }

    if (priv->buffer)
	XisbFree(priv->buffer);
    if (priv->synpara) {
	if (priv->shm_config) {
	    if ((shmid = xf86shmget(SHM_SYNAPTICS, 0, 0)) != -1)
		xf86shmctl(shmid, XF86IPC_RMID, NULL);
	} else {
	    xfree(priv->synpara);
	}
    }
    /* Freeing priv makes the X server crash. Don't know why.
       xfree(priv);
    */
    return local;
}

/*
 *  Alter the control parameters for the mouse. Note that all special
 *  protocol values are handled by dix.
 */
static void
SynapticsCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
    DBG(3, ErrorF("SynapticsCtrl called.\n"));
    /*
      pInfo = device->public.devicePrivate;
      pMse = pInfo->private;

      pMse->num       = ctrl->num;
      pMse->den       = ctrl->den;
      pMse->threshold = ctrl->threshold;
    */
}

static Bool
DeviceControl(DeviceIntPtr dev, int mode)
{
    Bool RetValue;

    switch (mode) {
    case DEVICE_INIT:
	DeviceInit(dev);
	RetValue = Success;
	break;
    case DEVICE_ON:
	RetValue = DeviceOn( dev );
	break;
    case DEVICE_OFF:
	RetValue = DeviceOff( dev );
	break;
    case DEVICE_CLOSE:
	{
	    int shmid;
	    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);
	    RetValue = DeviceOff( dev );
	    if (priv->shm_config)
		if ((shmid = xf86shmget(SHM_SYNAPTICS, 0, 0)) != -1)
		    xf86shmctl(shmid, XF86IPC_RMID, NULL);
	}
	break;
    default:
	RetValue = BadValue;
    }

    return RetValue;
}

static Bool
DeviceOn(DeviceIntPtr dev)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);

    DBG(3, ErrorF("Synaptics DeviceOn called\n"));

    local->fd = xf86OpenSerial(local->options);
    if (local->fd == -1) {
	xf86Msg(X_WARNING, "%s: cannot open input device\n", local->name);
	return !Success;
    }

    /* Try to grab the event device so that data doesn't leak to /dev/input/mice */
    if (priv->proto == SYN_PROTO_EVENT) {
	int ret;
	SYSCALL(ret = ioctl(local->fd, EVIOCGRAB, (pointer)1));
	if (ret < 0) {
	    xf86Msg(X_WARNING, "%s can't grab event device\n",
		    local->name, errno);
	}
    }

    priv->buffer = XisbNew(local->fd, 64);
    if (!priv->buffer) {
	xf86CloseSerial(local->fd);
	local->fd = -1;
	return !Success;
    }

    xf86FlushInput(local->fd);

    /* reinit the pad */
    QueryHardware(local);
    xf86AddEnabledDevice(local);
    dev->public.on = TRUE;

    return Success;
}

static Bool
DeviceOff(DeviceIntPtr dev)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);

    DBG(3, ErrorF("Synaptics DeviceOff called\n"));

    if (local->fd != -1) {
	xf86RemoveEnabledDevice(local);
	if (priv->proto == SYN_PROTO_PSAUX)
	    synaptics_set_mode(local->fd, 0);
	if (priv->buffer) {
	    XisbFree(priv->buffer);
	    priv->buffer = NULL;
	}
	xf86CloseSerial(local->fd);
    }
    dev->public.on = FALSE;
    return Success;
}

static Bool
DeviceInit(DeviceIntPtr dev)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    unsigned char map[] = {0, 1, 2, 3, 4, 5, 6, 7};

    DBG(3, ErrorF("Synaptics DeviceInit called\n"));

    dev->public.on = FALSE;

    InitPointerDeviceStruct((DevicePtr)dev, map,
			    7,
			    miPointerGetMotionEvents, SynapticsCtrl,
			    miPointerGetMotionBufferSize());

    /* X valuator */
    xf86InitValuatorAxisStruct(dev, 0, 0, -1, 1, 0, 1);
    xf86InitValuatorDefaults(dev, 0);
    /* Y valuator */
    xf86InitValuatorAxisStruct(dev, 1, 0, -1, 1, 0, 1);
    xf86InitValuatorDefaults(dev, 1);

    xf86MotionHistoryAllocate(local);

    return Success;
}

static int
move_distance(int dx, int dy)
{
    return xf86sqrt((dx * dx) + (dy * dy));
}

/* return angle of point relative to center */
static double
angle(SynapticsPrivate *priv, int x, int y)
{
    double xCenter = (priv->synpara->left_edge + priv->synpara->right_edge) / 2.0;
    double yCenter = (priv->synpara->top_edge + priv->synpara->bottom_edge) / 2.0;

    return xf86atan2(-(y - yCenter), x - xCenter);
}

/* return angle difference */
static double
diffa(double a1, double a2)
{
    double da = xf86fmod(a2 - a1, 2 * M_PI);
    if (da < 0)
	da += 2 * M_PI;
    if (da > M_PI)
	da -= 2 * M_PI;
    return da;
}

static edge_type
edge_detection(SynapticsPrivate *priv, int x, int y)
{
    edge_type edge = 0;

    if (x > priv->synpara->right_edge)
	edge |= RIGHT_EDGE;
    else if (x < priv->synpara->left_edge)
	edge |= LEFT_EDGE;

    if (y < priv->synpara->top_edge)
	edge |= TOP_EDGE;
    else if (y > priv->synpara->bottom_edge)
	edge |= BOTTOM_EDGE;

    return edge;
}

static CARD32
timerFunc(OsTimerPtr timer, CARD32 now, pointer arg)
{
    LocalDevicePtr local = (LocalDevicePtr) (arg);
    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);
    struct SynapticsHwState hw;
    int delay;
    int sigstate;
    CARD32 wakeUpTime;

    sigstate = xf86BlockSIGIO();

    hw = priv->hwState;
    hw.guest_dx = hw.guest_dy = 0;
    hw.millis = now;
    delay = HandleState(local, &hw);

    /*
     * Workaround for wraparound bug in the TimerSet function. This bug is already
     * fixed in CVS, but this driver needs to work with XFree86 versions 4.2.x and
     * 4.3.x too.
     */
    wakeUpTime = now + delay;
    if (wakeUpTime <= now)
	wakeUpTime = 0xffffffffL;

    priv->timer = TimerSet(priv->timer, TimerAbsolute, wakeUpTime, timerFunc, local);

    xf86UnblockSIGIO(sigstate);

    return 0;
}


#define MOVE_HIST(a) (priv->move_hist[((priv->count_packet_finger-(a))%SYNAPTICS_MOVE_HISTORY)])

static int clamp(int val, int min, int max)
{
    if (val < min)
	return min;
    else if (val < max)
	return val;
    else
	return max;
}


/*
 *  called for each full received packet from the touchpad
 */
static void
ReadInput(LocalDevicePtr local)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);
    struct SynapticsHwState hw;
    int delay = 0;
    Bool newDelay = FALSE;

    while (SynapticsGetHwState(local, priv, &hw) == Success) {
	hw.millis = GetTimeInMillis();
	delay = HandleState(local, &hw);
	newDelay = TRUE;
    }

    if (newDelay)
	priv->timer = TimerSet(priv->timer, 0, delay, timerFunc, local);
}

static int
HandleMidButtonEmulation(SynapticsPrivate *priv, struct SynapticsHwState *hw, long *delay)
{
    SynapticsSHM *para = priv->synpara;
    Bool done = FALSE;
    long timeleft;
    int mid = 0;

    while (!done) {
	switch (priv->mid_emu_state) {
	case MBE_OFF:
	    priv->button_delay_millis = hw->millis;
	    if (hw->left) {
		priv->mid_emu_state = MBE_LEFT;
	    } else if (hw->right) {
		priv->mid_emu_state = MBE_RIGHT;
	    } else {
		done = TRUE;
	    }
	    break;
	case MBE_LEFT:
	    timeleft = TIME_DIFF(priv->button_delay_millis + para->emulate_mid_button_time,
				 hw->millis);
	    if (timeleft > 0)
		*delay = MIN(*delay, timeleft);
	    if (!hw->left || (timeleft <= 0)) {
		hw->left = TRUE;
		priv->mid_emu_state = MBE_TIMEOUT;
		done = TRUE;
	    } else if (hw->right) {
		priv->mid_emu_state = MBE_MID;
	    } else {
		hw->left = FALSE;
		done = TRUE;
	    }
	    break;
	case MBE_RIGHT:
	    timeleft = TIME_DIFF(priv->button_delay_millis + para->emulate_mid_button_time,
				 hw->millis);
	    if (timeleft > 0)
		*delay = MIN(*delay, timeleft);
	    if (!hw->right || (timeleft <= 0)) {
		hw->right = TRUE;
		priv->mid_emu_state = MBE_TIMEOUT;
		done = TRUE;
	    } else if (hw->left) {
		priv->mid_emu_state = MBE_MID;
	    } else {
		hw->right = FALSE;
		done = TRUE;
	    }
	    break;
	case MBE_MID:
	    if (!hw->left && !hw->right) {
		priv->mid_emu_state = MBE_OFF;
	    } else {
		mid = TRUE;
		hw->left = hw->right = FALSE;
		done = TRUE;
	    }
	    break;
	case MBE_TIMEOUT:
	    if (!hw->left && !hw->right) {
		priv->mid_emu_state = MBE_OFF;
	    } else {
		done = TRUE;
	    }
	}
    }
    return mid;
}

static int
SynapticsDetectFinger(SynapticsPrivate *priv, struct SynapticsHwState *hw)
{
    SynapticsSHM *para = priv->synpara;
    int finger;

    /* finger detection thru pressure and threshold */
    finger = (((hw->z > para->finger_high) && !priv->finger_flag) ||
	      ((hw->z > para->finger_low)  &&  priv->finger_flag));

    /* palm detection */
    if (finger) {
	if ((hw->z > 200) && (hw->fingerWidth > 10))
	    priv->palm = TRUE;
    } else {
	priv->palm = FALSE;
    }
    if (hw->x == 0)
	priv->avg_width = 0;
    else
	priv->avg_width += (hw->fingerWidth - priv->avg_width + 1) / 2;
    if (finger && !priv->finger_flag) {
	int safe_width = MAX(hw->fingerWidth, priv->avg_width);
	if (hw->numFingers > 1)
	    finger = TRUE;			/* more than one finger -> not a palm */
	else if ((safe_width < 6) && (priv->prev_z < para->finger_high))
	    finger = TRUE;			/* thin finger, distinct touch -> not a palm */
	else if ((safe_width < 7) && (priv->prev_z < para->finger_high / 2))
	    finger = TRUE;			/* thin finger, distinct touch -> not a palm */
	else if (hw->z > priv->prev_z + 1)	/* z not stable, may be a palm */
	    finger = FALSE;
	else if (hw->z < priv->prev_z - 5)	/* z not stable, may be a palm */
	    finger = FALSE;
	else if (hw->z > 200)			/* z too large -> probably palm */
	    finger = FALSE;
	else if (hw->fingerWidth > 10)		/* finger width too large -> probably palm */
	    finger = FALSE;
    }
    priv->prev_z = hw->z;

    return finger;
}

static void
ReportTap(SynapticsPrivate *priv, TapEvent tap)
{
    int button = priv->synpara->tap_action[tap];
    switch (button) {
    case 1:
	priv->tap_left = TRUE;
	break;
    case 2:
	priv->tap_mid = TRUE;
	break;
    case 3:
	priv->tap_right = TRUE;
	break;
    }
}

/*
 * React on changes in the hardware state. This function is called every time
 * the hardware state changes. The return value is used to specify how many
 * milliseconds to wait before calling the function again if no state change
 * occurs.
 */
static int
HandleState(LocalDevicePtr local, struct SynapticsHwState* hw)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);
    SynapticsSHM *para = priv->synpara;
    Bool finger;
    int dist, dx, dy, buttons, id;
    edge_type edge;
    Bool mid;
    double speed, integral;
    int change;
    int scroll_up, scroll_down, scroll_left, scroll_right;
    int double_click;
    long delay = 1000000000;
    long timeleft;
    int i;

    dx = dy = 0;

    /* update hardware state in shared memory */
    para->x = hw->x;
    para->y = hw->y;
    para->z = hw->z;
    para->numFingers = hw->numFingers;
    para->fingerWidth = hw->fingerWidth;
    para->left = hw->left;
    para->right = hw->right;
    para->up = hw->up;
    para->down = hw->down;
    for (i = 0; i < 8; i++)
	para->multi[i] = hw->multi[i];
    para->middle = hw->middle;
    para->guest_left = hw->guest_left;
    para->guest_mid = hw->guest_mid;
    para->guest_right = hw->guest_right;
    para->guest_dx = hw->guest_dx;
    para->guest_dy = hw->guest_dy;

    /* If touchpad is switched off, we skip the whole thing and return delay */
    if (para->touchpad_off == TRUE)
	return delay;

    /* Treat the first two multi buttons as up/down for now. */
    hw->up |= hw->multi[0];
    hw->down |= hw->multi[1];

    /* 3rd button emulation */
    mid = HandleMidButtonEmulation(priv, hw, &delay);
    mid |= hw->middle;

    /* Up/Down button scrolling or middle/double click */
    double_click = FALSE;
    if (!para->updown_button_scrolling) {
	if (hw->down) {		/* map down button to middle button */
	    mid = TRUE;
	}

	if (hw->up) {		/* up button generates double click */
	    if (!priv->prev_up)
		double_click = TRUE;
	}
	priv->prev_up = hw->up;

	/* reset up/down button events */
	hw->up = hw->down = FALSE;
    }

    /*
     * Some touchpads have a scroll wheel region where a very large X
     * coordinate is reported. For such touchpads, we adjust the X
     * coordinate to eliminate the discontinuity.
     */
    if (hw->x <= XMAX_VALID) {
	if (priv->largest_valid_x < hw->x)
	    priv->largest_valid_x = hw->x;
    } else {
	hw->x = priv->largest_valid_x;
    }

    edge = edge_detection(priv, hw->x, hw->y);

    finger = SynapticsDetectFinger(priv, hw);

    /* tap and drag detection */
    if (priv->palm) {
	/* Palm detected, skip tap/drag processing */
    } else if (finger && !priv->finger_flag) { /* touched */
	DBG(7, ErrorF("touched - x:%d, y:%d millis:%lu\n", hw->x, hw->y, hw->millis));
	if (priv->tap) {
	    DBG(7, ErrorF("drag detected - tap time:%lu\n", priv->tapping_millis));
	    priv->drag = TRUE; /* drag gesture */
	}
	priv->touch_on.x = hw->x;
	priv->touch_on.y = hw->y;
	priv->touch_on.millis = hw->millis;
    } else if (!finger && priv->finger_flag) { /* untouched */
	DBG(7, ErrorF("untouched - x:%d, y:%d millis:%lu finger:%d\n",
		      hw->x, hw->y, hw->millis, priv->finger_count));
	/* check if
	   1. the tap is in tap_time
	   2. the max movement is in tap_move or more than one finger are tapped */
	timeleft = TIME_DIFF(priv->touch_on.millis + para->tap_time, hw->millis);
	if (timeleft > 0 &&
	    (((abs(hw->x - priv->touch_on.x) < para->tap_move) &&
	      (abs(hw->y - priv->touch_on.y) < para->tap_move)) ||
	     priv->finger_count)) {
	    if (priv->drag) {
		DBG(7, ErrorF("double tapping detected\n"));
		priv->doubletap = TRUE;
		priv->tap = FALSE;
	    } else {
		DBG(7, ErrorF("tapping detected @ "));
		priv->tapping_millis = hw->millis;
		priv->tap = TRUE;
		if (priv->finger_count == 0) {
		    switch (edge) {
		    case RIGHT_TOP_EDGE:
			DBG(7, ErrorF("right top edge\n"));
			ReportTap(priv, RT_TAP);
			break;
		    case RIGHT_BOTTOM_EDGE:
			DBG(7, ErrorF("right bottom edge\n"));
			ReportTap(priv, RB_TAP);
			break;
		    case LEFT_TOP_EDGE:
			DBG(7, ErrorF("left top edge\n"));
			ReportTap(priv, LT_TAP);
			break;
		    case LEFT_BOTTOM_EDGE:
			DBG(7, ErrorF("left bottom edge\n"));
			ReportTap(priv, LB_TAP);
			break;
		    default:
			DBG(7, ErrorF("no edge\n"));
			ReportTap(priv, F1_TAP);
		    }
		} else {
		    switch (priv->finger_count) {
		    case 2:
			DBG(7, ErrorF("two finger tap\n"));
			ReportTap(priv, F2_TAP);
			break;
		    case 3:
			DBG(7, ErrorF("three finger tap\n"));
			ReportTap(priv, F3_TAP);
			break;
		    default:
			DBG(7, ErrorF("one finger\n"));
			ReportTap(priv, F1_TAP);
		    }
		}
	    }
	} /* tap detection */
	if ((timeleft <= 0) && priv->drag && para->locked_drags)
	    priv->draglock = TRUE;
	priv->drag = FALSE;
    } /* finger lost */

    /* detecting 2 and 3 fingers */
    timeleft = TIME_DIFF(priv->touch_on.millis + para->tap_time, hw->millis);
    if (timeleft > 0)
	delay = MIN(delay, timeleft);
    if (finger &&			/* finger is on the surface */
	(timeleft > 0)) {		/* tap time is not succeeded */
	/* count fingers when reported */
	if ((hw->numFingers == 2) && (priv->finger_count == 0))
	    priv->finger_count = 2;
	if (hw->numFingers == 3)
	    priv->finger_count = 3;
    } else { /* reset finger counts */
	priv->finger_count = 0;
    }

    /* reset tapping button flags */
    if (!priv->tap && !priv->drag && !priv->doubletap && !priv->draglock) {
	priv->tap_left = priv->tap_mid = priv->tap_right = FALSE;
    }

    /* tap processing */
    timeleft = TIME_DIFF(priv->tapping_millis + para->tap_time, hw->millis);
    if (timeleft > 0)
	delay = MIN(delay, timeleft);
    if (priv->tap && (timeleft > 0)) {
	hw->left  |= priv->tap_left;
	mid       |= priv->tap_mid;
	hw->right |= priv->tap_right;
    } else {
	if (priv->tap)
	    priv->draglock = FALSE;
	priv->tap = FALSE;
    }

    /* drag processing */
    if (priv->drag || priv->draglock) {
	hw->left  |= priv->tap_left;
	mid       |= priv->tap_mid;
	hw->right |= priv->tap_right;
    }

    /* double tap processing */
    if (priv->doubletap && !priv->finger_flag) {
	hw->left  |= priv->tap_left;
	mid       |= priv->tap_mid;
	hw->right |= priv->tap_right;
	priv->doubletap = FALSE;
    }

    /* scroll detection */
    if (finger && !priv->finger_flag) {
	if (para->circular_scrolling) {
	    if ((para->circular_trigger == 0 && edge) ||
		(para->circular_trigger == 1 && edge & TOP_EDGE) ||
		(para->circular_trigger == 2 && edge & TOP_EDGE && edge & RIGHT_EDGE) ||
		(para->circular_trigger == 3 && edge & RIGHT_EDGE) ||
		(para->circular_trigger == 4 && edge & RIGHT_EDGE && edge & BOTTOM_EDGE) ||
		(para->circular_trigger == 5 && edge & BOTTOM_EDGE) ||
		(para->circular_trigger == 6 && edge & BOTTOM_EDGE && edge & LEFT_EDGE) ||
		(para->circular_trigger == 7 && edge & LEFT_EDGE) ||
		(para->circular_trigger == 8 && edge & LEFT_EDGE && edge & TOP_EDGE)) {
		priv->circ_scroll_on = TRUE;
		priv->scroll_a = angle(priv, hw->x, hw->y);
		DBG(7, ErrorF("circular scroll detected on edge\n"));
	    }
	}
	if (!priv->circ_scroll_on) {
	    if ((para->scroll_dist_vert != 0) && (edge & RIGHT_EDGE)) {
		priv->vert_scroll_on = TRUE;
		priv->scroll_y = hw->y;
		DBG(7, ErrorF("vert edge scroll detected on right edge\n"));
	    }
	    if ((para->scroll_dist_horiz != 0) && (edge & BOTTOM_EDGE)) {
		priv->horiz_scroll_on = TRUE;
		priv->scroll_x = hw->x;
		DBG(7, ErrorF("horiz edge scroll detected on bottom edge\n"));
	    }
	}
    }
    if (priv->circ_scroll_on && (!finger || priv->palm)) {
	/* circular scroll locks in until finger is raised */
	DBG(7, ErrorF("cicular scroll off\n"));
	priv->circ_scroll_on = FALSE;
    }
    if (priv->vert_scroll_on && (!(edge & RIGHT_EDGE) || !finger || priv->palm)) {
	DBG(7, ErrorF("vert edge scroll off\n"));
	priv->vert_scroll_on = FALSE;
    }
    if (priv->horiz_scroll_on && (!(edge & BOTTOM_EDGE) || !finger || priv->palm)) {
	DBG(7, ErrorF("horiz edge scroll off\n"));
	priv->horiz_scroll_on = FALSE;
    }

    /* scroll processing */

    /* if hitting a corner (top right or bottom right) while vertical scrolling is active,
       switch over to circular scrolling smoothly */
    if (priv->vert_scroll_on && !priv->horiz_scroll_on && para->circular_scrolling) {
	if ((edge & RIGHT_EDGE) && (edge & (TOP_EDGE | BOTTOM_EDGE))) {
	    priv->vert_scroll_on = FALSE;
	    priv->circ_scroll_on = TRUE;
	    priv->scroll_a = angle(priv, hw->x, hw->y);
	    DBG(7, ErrorF("switching to circular scrolling\n"));
	}
    }

    scroll_up = 0;
    scroll_down = 0;
    if (priv->vert_scroll_on) {
	/* + = down, - = up */
	while (hw->y - priv->scroll_y > para->scroll_dist_vert) {
	    scroll_down++;
	    priv->scroll_y += para->scroll_dist_vert;
	}
	while (hw->y - priv->scroll_y < -para->scroll_dist_vert) {
	    scroll_up++;
	    priv->scroll_y -= para->scroll_dist_vert;
	}
    }
    if (priv->circ_scroll_on) {
	/* + = counter clockwise, - = clockwise */
	while (diffa(priv->scroll_a, angle(priv, hw->x, hw->y)) > para->scroll_dist_circ) {
	    scroll_up++;
	    if (scroll_up > 1000)
		break; /* safety */
	    priv->scroll_a += para->scroll_dist_circ;
	    if (priv->scroll_a > M_PI)
		priv->scroll_a -= 2 * M_PI;
	}
	while (diffa(priv->scroll_a, angle(priv, hw->x, hw->y)) < -para->scroll_dist_circ) {
	    scroll_down++;
	    if (scroll_down > 1000)
		break; /* safety */
	    priv->scroll_a -= para->scroll_dist_circ;
	    if (priv->scroll_a < -M_PI)
		priv->scroll_a += 2 * M_PI;
	}
    }
    scroll_left = 0;
    scroll_right = 0;
    if (priv->horiz_scroll_on) {
	/* + = right, - = left */
	while (hw->x - priv->scroll_x > para->scroll_dist_horiz) {
	    scroll_right++;
	    priv->scroll_x += para->scroll_dist_horiz;
	}
	while (hw->x - priv->scroll_x < -para->scroll_dist_horiz) {
	    scroll_left++;
	    priv->scroll_x -= para->scroll_dist_horiz;
	}
    }

    /* movement */
    if (finger && !priv->vert_scroll_on && !priv->horiz_scroll_on && !priv->circ_scroll_on &&
	!priv->finger_count && !priv->palm) {
	delay = MIN(delay, 13);
	if (priv->count_packet_finger > 3) { /* min. 3 packets */
	    dx = (hw->x - MOVE_HIST(2).x) / 2;
	    dy = (hw->y - MOVE_HIST(2).y) / 2;

	    if (priv->drag || priv->draglock || para->edge_motion_use_always) {
		int minZ = para->edge_motion_min_z;
		int maxZ = para->edge_motion_max_z;
		int minSpd = para->edge_motion_min_speed;
		int maxSpd = para->edge_motion_max_speed;
		int edge_speed;

		if (hw->z <= minZ) {
		    edge_speed = minSpd;
		} else if (hw->z >= maxZ) {
		    edge_speed = maxSpd;
		} else {
		    edge_speed = minSpd + (hw->z - minZ) * (maxSpd - minSpd) / (maxZ - minZ);
		}
		if (edge & RIGHT_EDGE) {
		    dx += clamp(edge_speed - dx, 0, edge_speed);
		} else if (edge & LEFT_EDGE) {
		    dx -= clamp(edge_speed + dx, 0, edge_speed);
		}
		if (edge & TOP_EDGE) {
		    dy -= clamp(edge_speed + dy, 0, edge_speed);
		} else if (edge & BOTTOM_EDGE) {
		    dy += clamp(edge_speed - dy, 0, edge_speed);
		}
	    }

	    /* speed depending on distance/packet */
	    dist = move_distance( dx, dy );
	    speed = dist * para->accl;
	    if (speed > para->max_speed) {  /* set max speed factor */
		speed = para->max_speed;
	    } else if (speed < para->min_speed) { /* set min speed factor */
		speed = para->min_speed;
	    }

	    /* save the fraction for adding to the next priv->count_packet */
	    priv->frac_x = xf86modf((dx * speed) + priv->frac_x, &integral);
	    dx = integral;
	    priv->frac_y = xf86modf((dy * speed) + priv->frac_y, &integral);
	    dy = integral;
	}

	priv->count_packet_finger++;
    } else {				    /* reset packet counter */
	priv->count_packet_finger = 0;
    }


    buttons = ((hw->left     ? 0x01 : 0) |
	       (mid          ? 0x02 : 0) |
	       (hw->right    ? 0x04 : 0) |
	       (hw->guest_left  ? 0x01 : 0) |
	       (hw->guest_mid   ? 0x02 : 0) |
	       (hw->guest_right ? 0x04 : 0) |
	       (hw->up       ? 0x08 : 0) |
	       (hw->down     ? 0x10 : 0) |
	       (hw->multi[2] ? 0x20 : 0) |
	       (hw->multi[3] ? 0x40 : 0));

    /* Flags */
    priv->finger_flag = finger;

    /* generate a history of the absolute positions */
    MOVE_HIST(0).x = hw->x;
    MOVE_HIST(0).y = hw->y;

    /* Add guest device movements */
    dx += hw->guest_dx;
    dy += hw->guest_dy;

    /* Post events */
    if (dx || dy)
	xf86PostMotionEvent(local->dev, 0, 0, 2, dx, dy);

    change = buttons ^ priv->lastButtons;
    while (change) {
	id = ffs(change); /* number of first set bit 1..32 is returned */
	change &= ~(1 << (id - 1));
	xf86PostButtonEvent(local->dev, FALSE, id, (buttons & (1 << (id - 1))), 0, 0);
    }
    priv->lastButtons = buttons;

    while (scroll_up-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 4, !hw->up, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 4, hw->up, 0, 0);
    }
    while (scroll_down-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 5, !hw->down, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 5, hw->down, 0, 0);
    }
    while (scroll_left-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 6, TRUE, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 6, FALSE, 0, 0);
    }
    while (scroll_right-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 7, TRUE, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 7, FALSE, 0, 0);
    }
    if (double_click) {
	int i;
	for (i = 0; i < 2; i++) {
	    xf86PostButtonEvent(local->dev, FALSE, 1, !hw->left, 0, 0);
	    xf86PostButtonEvent(local->dev, FALSE, 1, hw->left, 0, 0);
	}
    }


    /*
     * Handle auto repeat buttons
     */
    if ((hw->up || hw->down || hw->multi[2] || hw->multi[3]) &&
	para->updown_button_scrolling) {
	priv->repeatButtons = buttons & 0x78;
	if (!priv->nextRepeat) {
	    priv->nextRepeat = hw->millis + 200;
	}
    } else {
	priv->repeatButtons = 0;
	priv->nextRepeat = 0;
    }

    if (priv->repeatButtons) {
	timeleft = TIME_DIFF(priv->nextRepeat, hw->millis);
	if (timeleft > 0)
	    delay = MIN(delay, timeleft);
	if (timeleft <= 0) {
	    int change, id;
	    change = priv->repeatButtons;
	    while (change) {
		id = ffs(change);
		change &= ~(1 << (id - 1));
		xf86PostButtonEvent(local->dev, FALSE, id, FALSE, 0, 0);
		xf86PostButtonEvent(local->dev, FALSE, id, TRUE, 0, 0);
	    }

	    priv->nextRepeat = hw->millis + 100;
	    delay = MIN(delay, 100);
	}
    }

    return delay;
}

static int
ControlProc(LocalDevicePtr local, xDeviceCtl * control)
{
    DBG(3, ErrorF("Control Proc called\n"));
    return Success;
}


static void
CloseProc(LocalDevicePtr local)
{
    DBG(3, ErrorF("Close Proc called\n"));
}

static int
SwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
    ErrorF("SwitchMode called\n");
    return Success;
}

static Bool
ConvertProc(LocalDevicePtr local,
	    int first,
	    int num,
	    int v0,
	    int v1,
	    int v2,
	    int v3,
	    int v4,
	    int v5,
	    int *x,
	    int *y)
{
    if (first != 0 || num != 2)
	return FALSE;

    *x = v0;
    *y = v1;

    return TRUE;
}


static Bool
QueryHardware(LocalDevicePtr local)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) local->private;
    SynapticsSHM *para = priv->synpara;
    int retries;
    int mode;

    if (priv->proto == SYN_PROTO_EVENT)
	return Success;

    /* is the synaptics touchpad active? */
    priv->isSynaptics = QueryIsSynaptics(local->fd);

    if ((!priv->isSynaptics) && (!para->repeater || (priv->fifofd == -1))) {
	xf86Msg(X_ERROR, "%s no synaptics touchpad detected and no repeater device\n",
		local->name);
	priv->isSynaptics = TRUE;
	return !Success;
    }
    para->isSynaptics = priv->isSynaptics;

    priv->protoBufTail = 0;
    if (!priv->isSynaptics) {
	xf86Msg(X_PROBED, "%s no synaptics touchpad, data piped to repeater fifo\n", local->name);
	synaptics_reset(local->fd);
	SynapticsEnableDevice(local->fd);
	return Success;
    }

    xf86Msg(X_PROBED, "%s synaptics touchpad found\n", local->name);

    retries = 3;
    while ((retries++ <= 3) && (synaptics_reset(local->fd) != Success))
	xf86Msg(X_ERROR, "%s reset failed\n", local->name);

    if (synaptics_identify(local->fd, &priv->identity) != Success)
	return !Success;
    para->identity = priv->identity;

    if (synaptics_model_id(local->fd, &priv->model_id) != Success)
	return !Success;
    para->model_id = priv->model_id;

    if (synaptics_capability(local->fd, &priv->capabilities, &priv->ext_cap) != Success)
	return !Success;
    para->capabilities = priv->capabilities;
    para->ext_cap = priv->ext_cap;

    mode = SYN_BIT_ABSOLUTE_MODE | SYN_BIT_HIGH_RATE;
    if (SYN_ID_MAJOR(priv->identity) >= 4)
	mode |= SYN_BIT_DISABLE_GESTURE;
    if (SYN_CAP_EXTENDED(priv->capabilities))
	mode |= SYN_BIT_W_MODE;
    if (synaptics_set_mode(local->fd, mode) != Success)
	return !Success;

    /* Check to see if the host mouse supports a guest */
    if (SYN_CAP_PASSTHROUGH(priv->capabilities)) {
        priv->hasGuest = TRUE;

	/* Enable the guest mouse.  Set it to relative mode, three byte
	 * packets */

	/* Disable the host to talk to the guest */
	SynapticsDisableDevice(local->fd);
	/* Reset it, set defaults, streaming and enable it */
	if ((SynapticsResetPassthrough(local->fd)) != Success) {
	    priv->hasGuest = FALSE;
	}
    }

    SynapticsEnableDevice(local->fd);

    PrintIdent(priv);

    return Success;
}

static Bool
SynapticsGetHwState(LocalDevicePtr local, SynapticsPrivate *priv,
		    struct SynapticsHwState *hw)
{
    if (priv->proto == SYN_PROTO_PSAUX) {
	return SynapticsParseRawPacket(local, priv, hw);
    } else if (priv->proto == SYN_PROTO_EVENT) {
	return SynapticsParseEventData(local, priv, hw);
    } else {
	return !Success;
    }
}

static Bool
SynapticsParseEventData(LocalDevicePtr local, SynapticsPrivate *priv,
			struct SynapticsHwState *hwRet)
{
    struct input_event ev;
    Bool v;
    struct SynapticsHwState *hw = &(priv->hwState);

    while (SynapticsReadEvent(priv, &ev) == Success) {
	switch (ev.type) {
	case EV_SYN:
	    switch (ev.code) {
	    case SYN_REPORT:
		if (priv->oneFinger)
		    hw->numFingers = 1;
		else if (priv->twoFingers)
		    hw->numFingers = 2;
		else if (priv->threeFingers)
		    hw->numFingers = 3;
		else
		    hw->numFingers = 0;
		*hwRet = *hw;
		return Success;
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
		priv->oneFinger = v;
		break;
	    case BTN_TOOL_DOUBLETAP:
		priv->twoFingers = v;
		break;
	    case BTN_TOOL_TRIPLETAP:
		priv->threeFingers = v;
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
    return !Success;
}

static Bool
SynapticsReadEvent(SynapticsPrivate *priv, struct input_event *ev)
{
    int i, c;
    unsigned char *pBuf, u;

    for (i = 0; i < sizeof(struct input_event); i++) {
	if ((c = XisbRead(priv->buffer)) < 0)
	    return !Success;
	u = (unsigned char)c;
	pBuf = (unsigned char *)ev;
	pBuf[i] = u;
    }
    return Success;
}

static Bool
SynapticsParseRawPacket(LocalDevicePtr local, SynapticsPrivate *priv,
			struct SynapticsHwState *hwRet)
{
    Bool ret = SynapticsGetPacket(local, priv);
    int newabs = SYN_MODEL_NEWABS(priv->model_id);
    unsigned char *buf = priv->protoBuf;
    struct SynapticsHwState *hw = &(priv->hwState);
    int w, i;

    if (ret != Success)
	return ret;

    /* Handle guest packets */
    hw->guest_dx = hw->guest_dy = 0;
    if (newabs && priv->hasGuest) {
	w = (((buf[0] & 0x30) >> 2) |
	     ((buf[0] & 0x04) >> 1) |
	     ((buf[3] & 0x04) >> 2));
	if (w == 3) {	       /* If w is 3, this is a guest packet */
	    if (buf[4] != 0)
		hw->guest_dx =   buf[4] - ((buf[1] & 0x10) ? 256 : 0);
	    if (buf[5] != 0)
		hw->guest_dy = -(buf[5] - ((buf[1] & 0x20) ? 256 : 0));
	    hw->guest_left  = (buf[1] & 0x01) ? TRUE : FALSE;
	    hw->guest_mid   = (buf[1] & 0x04) ? TRUE : FALSE;
	    hw->guest_right = (buf[1] & 0x02) ? TRUE : FALSE;
	    *hwRet = *hw;
	    return Success;
	}
    }

    /* Handle normal packets */
    hw->x = hw->y = hw->z = hw->numFingers = hw->fingerWidth = 0;
    hw->left = hw->right = hw->up = hw->down = hw->middle = FALSE;
    for (i = 0; i < 8; i++)
	hw->multi[i] = FALSE;

    if (newabs) {			    /* newer protos...*/
	DBG(7, ErrorF("using new protocols\n"));
	hw->x = (((buf[3] & 0x10) << 8) |
		 ((buf[1] & 0x0f) << 8) |
		 buf[4]);
	hw->y = (((buf[3] & 0x20) << 7) |
		 ((buf[1] & 0xf0) << 4) |
		 buf[5]);

	hw->z = buf[2];
	w = (((buf[0] & 0x30) >> 2) |
	     ((buf[0] & 0x04) >> 1) |
	     ((buf[3] & 0x04) >> 2));

	hw->left  = (buf[0] & 0x01) ? 1 : 0;
	hw->right = (buf[0] & 0x02) ? 1 : 0;

	if (SYN_CAP_EXTENDED(priv->capabilities)) {
	    if (SYN_CAP_FOUR_BUTTON(priv->capabilities)) {
		hw->up = ((buf[3] & 0x01)) ? 1 : 0;
		if (hw->left)
		    hw->up = !hw->up;
		hw->down = ((buf[3] & 0x02)) ? 1 : 0;
		if (hw->right)
		    hw->down = !hw->down;
	    }
	    if (SYN_CAP_MULTI_BUTTON_NO(priv->ext_cap)) {
		if ((buf[3] & 2) ? !hw->right : hw->right) {
		    switch (SYN_CAP_MULTI_BUTTON_NO(priv->ext_cap) & ~0x01) {
		    default:
			break;
		    case 8:
			hw->multi[7] = ((buf[5] & 0x08)) ? 1 : 0;
			hw->multi[6] = ((buf[4] & 0x08)) ? 1 : 0;
		    case 6:
			hw->multi[5] = ((buf[5] & 0x04)) ? 1 : 0;
			hw->multi[4] = ((buf[4] & 0x04)) ? 1 : 0;
		    case 4:
			hw->multi[3] = ((buf[5] & 0x02)) ? 1 : 0;
			hw->multi[2] = ((buf[4] & 0x02)) ? 1 : 0;
		    case 2:
			hw->multi[1] = ((buf[5] & 0x01)) ? 1 : 0;
			hw->multi[0] = ((buf[4] & 0x01)) ? 1 : 0;
		    }
		}
	    }
	}
    } else {			    /* old proto...*/
	DBG(7, ErrorF("using old protocol\n"));
	hw->x = (((buf[1] & 0x1F) << 8) |
		 buf[2]);
	hw->y = (((buf[4] & 0x1F) << 8) |
		 buf[5]);

	hw->z = (((buf[0] & 0x30) << 2) |
		 (buf[3] & 0x3F));
	w = (((buf[1] & 0x80) >> 4) |
	     ((buf[0] & 0x04) >> 1));

	hw->left  = (buf[0] & 0x01) ? 1 : 0;
	hw->right = (buf[0] & 0x02) ? 1 : 0;
    }

    hw->y = YMAX_NOMINAL + YMIN_NOMINAL - hw->y;

    if (hw->z > 0) {
	int w_ok = 0;
	/*
	 * Use capability bits to decide if the w value is valid.
	 * If not, set it to 5, which corresponds to a finger of
	 * normal width.
	 */
	if (SYN_CAP_EXTENDED(priv->capabilities)) {
	    if ((w >= 0) && (w <= 1)) {
		w_ok = SYN_CAP_MULTIFINGER(priv->capabilities);
	    } else if (w == 2) {
		w_ok = SYN_MODEL_PEN(priv->model_id);
	    } else if ((w >= 4) && (w <= 15)) {
		w_ok = SYN_CAP_PALMDETECT(priv->capabilities);
	    }
	}
	if (!w_ok)
	    w = 5;

	switch (w) {
	case 0:
	    hw->numFingers = 2;
	    hw->fingerWidth = 5;
	    break;
	case 1:
	    hw->numFingers = 3;
	    hw->fingerWidth = 5;
	    break;
	default:
	    hw->numFingers = 1;
	    hw->fingerWidth = w;
	    break;
	}
    }

    *hwRet = *hw;
    return Success;
}

/*
 * Decide if the current packet stored in priv->protoBuf is valid.
 */
static Bool
PacketOk(SynapticsPrivate *priv)
{
    unsigned char *buf = priv->protoBuf;
    int newabs = SYN_MODEL_NEWABS(priv->model_id);

    if (newabs ? ((buf[0] & 0xC0) != 0x80) : ((buf[0] & 0xC0) != 0xC0)) {
	DBG(4, ErrorF("Synaptics driver lost sync at 1st byte\n"));
	return FALSE;
    }

    if (!newabs && ((buf[1] & 0x60) != 0x00)) {
	DBG(4, ErrorF("Synaptics driver lost sync at 2nd byte\n"));
	return FALSE;
    }

    if ((newabs ? ((buf[3] & 0xC0) != 0xC0) : ((buf[3] & 0xC0) != 0x80))) {
	DBG(4, ErrorF("Synaptics driver lost sync at 4th byte\n"));
	return FALSE;
    }

    if (!newabs && ((buf[4] & 0x60) != 0x00)) {
	DBG(4, ErrorF("Synaptics driver lost sync at 5th byte\n"));
	return FALSE;
    }

    return TRUE;
}

static Bool
SynapticsGetPacket(LocalDevicePtr local, SynapticsPrivate *priv)
{
    int count = 0;
    int c;
    unsigned char u;

    while ((c = XisbRead(priv->buffer)) >= 0) {
	u = (unsigned char)c;

	/* test if there is a reset sequence received */
	if ((c == 0x00) && (priv->lastByte == 0xAA)) {
	    if (xf86WaitForInput(local->fd, 50000) == 0) {
		DBG(7, ErrorF("Reset received\n"));
		QueryHardware(local);
	    } else
		DBG(3, ErrorF("faked reset received\n"));
	}
	priv->lastByte = u;

	/* when there is no synaptics touchpad pipe the data to the repeater fifo */
	if (!priv->isSynaptics) {
	    xf86write(priv->fifofd, &u, 1);
	    if (++count >= 3)
		return !Success;
	    continue;
	}

	/* to avoid endless loops */
	if (count++ > 30) {
	    ErrorF("Synaptics driver lost sync... got gigantic packet!\n");
	    return !Success;
	}

	priv->protoBuf[priv->protoBufTail++] = u;

	/* Check that we have a valid packet. If not, we are out of sync,
	   so we throw away the first byte in the packet.*/
	if (priv->protoBufTail >= 6) {
	    if (!PacketOk(priv)) {
		int i;
		for (i = 0; i < priv->protoBufTail - 1; i++)
		    priv->protoBuf[i] = priv->protoBuf[i + 1];
		priv->protoBufTail--;
		priv->outOfSync++;
		if (priv->outOfSync > MAX_UNSYNC_PACKETS) {
		    priv->outOfSync = 0;
		    DBG(3, ErrorF("Synaptics synchronization lost too long -> reset touchpad.\n"));
		    QueryHardware(local); /* including a reset */
		    continue;
		}
	    }
	}

	if (priv->protoBufTail >= 6) { /* Full packet received */
	    if (priv->outOfSync > 0) {
		priv->outOfSync = 0;
		DBG(4, ErrorF("Synaptics driver resynced.\n"));
	    }
	    priv->protoBufTail = 0;
	    return Success;
	}
    }

    return !Success;
}

static void
PrintIdent(SynapticsPrivate *priv)
{
    xf86Msg(X_PROBED, " Synaptics Touchpad, model: %d\n", SYN_ID_MODEL(priv->identity));
    xf86Msg(X_PROBED, " Firmware: %d.%d\n", SYN_ID_MAJOR(priv->identity),
	    SYN_ID_MINOR(priv->identity));

    if (SYN_MODEL_ROT180(priv->model_id))
	xf86Msg(X_PROBED, " 180 degree mounted touchpad\n");
    if (SYN_MODEL_PORTRAIT(priv->model_id))
	xf86Msg(X_PROBED, " portrait touchpad\n");
    xf86Msg(X_PROBED, " Sensor: %d\n", SYN_MODEL_SENSOR(priv->model_id));
    if (SYN_MODEL_NEWABS(priv->model_id))
	xf86Msg(X_PROBED, " new absolute packet format\n");
    if (SYN_MODEL_PEN(priv->model_id))
	xf86Msg(X_PROBED, " pen detection\n");

    if (SYN_CAP_EXTENDED(priv->capabilities)) {
	xf86Msg(X_PROBED, " Touchpad has extended capability bits\n");
	if (SYN_CAP_MULTI_BUTTON_NO(priv->ext_cap))
	    xf86Msg(X_PROBED, " -> %d multi buttons, i.e. besides standard buttons\n",
		    (int)(SYN_CAP_MULTI_BUTTON_NO(priv->ext_cap)));
	else if (SYN_CAP_FOUR_BUTTON(priv->capabilities))
	    xf86Msg(X_PROBED, " -> four buttons\n");
	if (SYN_CAP_MULTIFINGER(priv->capabilities))
	    xf86Msg(X_PROBED, " -> multifinger detection\n");
	if (SYN_CAP_PALMDETECT(priv->capabilities))
	    xf86Msg(X_PROBED, " -> palm detection\n");
	if (SYN_CAP_PASSTHROUGH(priv->capabilities))
	    xf86Msg(X_PROBED, " -> pass-through port\n");
    }
}
