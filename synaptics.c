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
#include "mipointer.h"
#ifdef XFREE_4_0_3
#include <xf86Optrec.h>  		/* needed for Options */
#endif


/*****************************************************************************
 *	Local Headers
 ****************************************************************************/
#define SYNAPTICS_PRIVATE
#include "synaptics.h"

/*****************************************************************************
 *	Variables without includable headers
 ****************************************************************************/

/*****************************************************************************
 *	Local Variables and Types
 ****************************************************************************/

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

/*****************************************************************************
 * Forward declaration
 ****************************************************************************/
static InputInfoPtr SynapticsPreInit(InputDriverPtr drv, IDevPtr dev, int flags);
static Bool DeviceControl(DeviceIntPtr, int);
static void ReadInput(LocalDevicePtr);
static int HandleState(LocalDevicePtr, struct SynapticsHwState*);
static int ControlProc(LocalDevicePtr, xDeviceCtl*);
static void CloseProc(LocalDevicePtr);
static int SwitchMode(ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc(LocalDevicePtr, int, int, int, int, int, int, int, int, int*, int*);
static Bool DeviceInit(DeviceIntPtr);
static Bool DeviceOn(DeviceIntPtr);
static Bool DeviceOff(DeviceIntPtr);
static Bool DeviceInit(DeviceIntPtr);


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
    enum SynapticsProtocol proto = SYN_PROTO_PSAUX;

    str_par = xf86FindOptionValue(local->options, "Protocol");
    if (str_par && !strcmp(str_par, "psaux")) {
	/* Already set up */
    } else if (str_par && !strcmp(str_par, "event")) {
	proto = SYN_PROTO_EVENT;
    } else if (str_par && !strcmp(str_par, "psm")) {
	proto = SYN_PROTO_PSM;
    } else if (str_par && !strcmp(str_par, "alps")) {
	proto = SYN_PROTO_ALPS;
    } else { /* default to auto-dev */
	if (event_proto_operations.AutoDevProbe(local))
	    proto = SYN_PROTO_EVENT;
    }
    switch (proto) {
    case SYN_PROTO_PSAUX:
	priv->proto_ops = &psaux_proto_operations;
	break;
    case SYN_PROTO_EVENT:
	priv->proto_ops = &event_proto_operations;
	break;
    case SYN_PROTO_PSM:
	priv->proto_ops = &psm_proto_operations;
	break;
    case SYN_PROTO_ALPS:
	priv->proto_ops = &alps_proto_operations;
	break;
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
    SynapticsSHM *pars;
    char *repeater;

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
    priv->tap_state = TS_START;
    priv->tap_button = 0;
    priv->tap_button_state = TBS_BUTTON_UP;
    priv->touch_on.millis = now;

    /* install shared memory or normal memory for parameters */
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

    /* read the parameters */
    pars = priv->synpara;
    pars->left_edge = xf86SetIntOption(local->options, "LeftEdge", 1900);
    pars->right_edge = xf86SetIntOption(local->options, "RightEdge", 5400);
    pars->top_edge = xf86SetIntOption(local->options, "TopEdge", 1900);
    pars->bottom_edge = xf86SetIntOption(local->options, "BottomEdge", 4000);
    pars->finger_low = xf86SetIntOption(local->options, "FingerLow", 25);
    pars->finger_high = xf86SetIntOption(local->options, "FingerHigh", 30);
    pars->tap_time = xf86SetIntOption(local->options, "MaxTapTime", 180);
    pars->tap_move = xf86SetIntOption(local->options, "MaxTapMove", 220);
    pars->emulate_mid_button_time = xf86SetIntOption(local->options,
							      "EmulateMidButtonTime", 75);
    pars->scroll_dist_vert = xf86SetIntOption(local->options, "VertScrollDelta", 100);
    pars->scroll_dist_horiz = xf86SetIntOption(local->options, "HorizScrollDelta", 100);
    pars->edge_motion_min_z = xf86SetIntOption(local->options, "EdgeMotionMinZ", 30);
    pars->edge_motion_max_z = xf86SetIntOption(local->options, "EdgeMotionMaxZ", 160);
    pars->edge_motion_min_speed = xf86SetIntOption(local->options, "EdgeMotionMinSpeed", 1);
    pars->edge_motion_max_speed = xf86SetIntOption(local->options, "EdgeMotionMaxSpeed", 200);
    pars->edge_motion_use_always = xf86SetBoolOption(local->options, "EdgeMotionUseAlways", FALSE);
    repeater = xf86SetStrOption(local->options, "Repeater", NULL);
    pars->updown_button_scrolling = xf86SetBoolOption(local->options, "UpDownScrolling", TRUE);
    pars->touchpad_off = xf86SetBoolOption(local->options, "TouchpadOff", FALSE);
    pars->guestmouse_off = xf86SetBoolOption(local->options, "GuestMouseOff", FALSE);
    pars->locked_drags = xf86SetBoolOption(local->options, "LockedDrags", FALSE);
    pars->tap_action[RT_TAP] = xf86SetIntOption(local->options, "RTCornerButton", 2);
    pars->tap_action[RB_TAP] = xf86SetIntOption(local->options, "RBCornerButton", 3);
    pars->tap_action[LT_TAP] = xf86SetIntOption(local->options, "LTCornerButton", 0);
    pars->tap_action[LB_TAP] = xf86SetIntOption(local->options, "LBCornerButton", 0);
    pars->tap_action[F1_TAP] = xf86SetIntOption(local->options, "TapButton1",     1);
    pars->tap_action[F2_TAP] = xf86SetIntOption(local->options, "TapButton2",     2);
    pars->tap_action[F3_TAP] = xf86SetIntOption(local->options, "TapButton3",     3);
    pars->circular_scrolling = xf86SetBoolOption(local->options, "CircularScrolling", FALSE);
    pars->circular_trigger   = xf86SetIntOption(local->options, "CircScrollTrigger", 0);

    str_par = xf86FindOptionValue(local->options, "MinSpeed");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &pars->min_speed) != 1))
	pars->min_speed=0.02;
    str_par = xf86FindOptionValue(local->options, "MaxSpeed");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &pars->max_speed) != 1))
	pars->max_speed=0.18;
    str_par = xf86FindOptionValue(local->options, "AccelFactor");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &pars->accl) != 1))
	pars->accl=0.0015;
    str_par = xf86FindOptionValue(local->options, "CircScrollDelta");
    if ((!str_par) || (xf86sscanf(str_par, "%lf", &pars->scroll_dist_circ) != 1))
	pars->scroll_dist_circ = 0.1;

    if (pars->circular_trigger < 0 || pars->circular_trigger > 8) {
	xf86Msg(X_WARNING, "Unknown circular scrolling trigger, using 0 (edges)");
	pars->circular_trigger = 0;
    }

    /* Warn about (and fix) incorrectly configured TopEdge/BottomEdge parameters */
    if (pars->top_edge > pars->bottom_edge) {
	int tmp = pars->top_edge;
	pars->top_edge = pars->bottom_edge;
	pars->bottom_edge = tmp;
	xf86Msg(X_WARNING, "%s: TopEdge is bigger than BottomEdge. Fixing.\n",
		local->name);
    }

    priv->largest_valid_x = MIN(pars->right_edge, XMAX_NOMINAL);

    priv->comm.buffer = XisbNew(local->fd, 200);
    DBG(9, XisbTrace(priv->comm.buffer, 1));

    priv->fifofd = -1;
    if (repeater) {
	/* create repeater fifo */
	if ((xf86mknod(repeater, 666, XF86_S_IFIFO) != 0) &&
	    (xf86errno != xf86_EEXIST)) {
	    xf86Msg(X_ERROR, "%s can't create repeater fifo\n", local->name);
	} else {
	    /* open the repeater fifo */
	    optList = xf86NewOption("Device", repeater);
	    if ((priv->fifofd = xf86OpenSerial(optList)) == -1) {
		xf86Msg(X_ERROR, "%s repeater device open failed\n", local->name);
	    }
	}
	xf86free(repeater);
    }

    if (!QueryHardware(local)) {
	xf86Msg(X_ERROR, "%s Unable to query/initialize Synaptics hardware.\n", local->name);
	goto SetupProc_fail;
    }

    local->history_size = xf86SetIntOption( local->options, "HistorySize", 0 );

    xf86ProcessCommonOptions(local, local->options);
    local->flags |= XI86_CONFIGURED;

    if (local->fd != -1) {
	xf86RemoveEnabledDevice(local);
	if (priv->comm.buffer) {
	    XisbFree(priv->comm.buffer);
	    priv->comm.buffer = NULL;
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

    if (priv->comm.buffer)
	XisbFree(priv->comm.buffer);
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

    priv->proto_ops->DeviceOnHook(local);

    priv->comm.buffer = XisbNew(local->fd, 64);
    if (!priv->comm.buffer) {
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
	priv->proto_ops->DeviceOffHook(local);
	if (priv->comm.buffer) {
	    XisbFree(priv->comm.buffer);
	    priv->comm.buffer = NULL;
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

static int clamp(int val, int min, int max)
{
    if (val < min)
	return min;
    else if (val < max)
	return val;
    else
	return max;
}

static Bool
SynapticsGetHwState(LocalDevicePtr local, SynapticsPrivate *priv,
		    struct SynapticsHwState *hw)
{
    if (priv->fifofd >= 0) {
	/* when there is no synaptics touchpad pipe the data to the repeater fifo */
	int count = 0;
	int c;
	while ((c = XisbRead(priv->comm.buffer)) >= 0) {
	    unsigned char u = (unsigned char)c;
	    xf86write(priv->fifofd, &u, 1);
	    if (++count >= 3)
		break;
	}
	return FALSE;
    }
    return priv->proto_ops->ReadHwState(local, &priv->synhw, priv->proto_ops,
					&priv->comm, hw);
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

    while (SynapticsGetHwState(local, priv, &hw)) {
	hw.millis = GetTimeInMillis();
	priv->hwState = hw;
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

    if (priv->palm)
	finger = FALSE;

    return finger;
}

static void
SelectTapButton(SynapticsPrivate *priv, edge_type edge)
{
    TapEvent tap;

    switch (priv->tap_max_fingers) {
    case 1:
    default:
	switch (edge) {
	case RIGHT_TOP_EDGE:
	    DBG(7, ErrorF("right top edge\n"));
	    tap = RT_TAP;
	    break;
	case RIGHT_BOTTOM_EDGE:
	    DBG(7, ErrorF("right bottom edge\n"));
	    tap = RB_TAP;
	    break;
	case LEFT_TOP_EDGE:
	    DBG(7, ErrorF("left top edge\n"));
	    tap = LT_TAP;
	    break;
	case LEFT_BOTTOM_EDGE:
	    DBG(7, ErrorF("left bottom edge\n"));
	    tap = LB_TAP;
	    break;
	default:
	    DBG(7, ErrorF("no edge\n"));
	    tap = F1_TAP;
	    break;
	}
	break;
    case 2:
	DBG(7, ErrorF("two finger tap\n"));
	tap = F2_TAP;
	break;
    case 3:
	DBG(7, ErrorF("three finger tap\n"));
	tap = F3_TAP;
	break;
    }

    priv->tap_button = priv->synpara->tap_action[tap];
    priv->tap_button = clamp(priv->tap_button, 0, 7);
}

static void
SetTapState(SynapticsPrivate *priv, enum TapState tap_state, int millis)
{
    DBG(7, ErrorF("SetTapState - %d -> %d (millis:%d)\n", priv->tap_state, tap_state, millis));
    switch (tap_state) {
    case TS_START:
	priv->tap_button_state = TBS_BUTTON_UP;
	priv->tap_max_fingers = 0;
	break;
    default:
	break;
    }
    priv->tap_state = tap_state;
}

static int
HandleTapProcessing(SynapticsPrivate *priv, struct SynapticsHwState *hw,
		    edge_type edge, Bool finger)
{
    SynapticsSHM *para = priv->synpara;
    Bool touch, release, timeout, move;
    long timeleft;
    long delay = 1000000000;

    if (priv->palm)
	return delay;

    touch = finger && !priv->finger_flag;
    release = !finger && priv->finger_flag;
    move = FALSE;
    if (touch) {
	priv->touch_on.x = hw->x;
	priv->touch_on.y = hw->y;
	priv->touch_on.millis = hw->millis;
    } else if (release) {
	priv->touch_on.millis = hw->millis;
	move = ((priv->tap_max_fingers <= 1) &&
		((abs(hw->x - priv->touch_on.x) >= para->tap_move) ||
		 (abs(hw->y - priv->touch_on.y) >= para->tap_move)));
    }
    if (priv->tap_max_fingers < hw->numFingers)
	priv->tap_max_fingers = hw->numFingers;
    timeleft = TIME_DIFF(priv->touch_on.millis + para->tap_time, hw->millis);
    if (timeleft > 0)
	delay = MIN(delay, timeleft);
    timeout = timeleft <= 0;

 restart:
    switch (priv->tap_state) {
    case TS_START:
	if (touch)
	    SetTapState(priv, TS_1, hw->millis);
	break;
    case TS_1:
	if (timeout || move) {
	    SetTapState(priv, TS_MOVE, hw->millis);
	    goto restart;
	} else if (release) {
	    SelectTapButton(priv, edge);
	    SetTapState(priv, TS_2, hw->millis);
	}
	break;
    case TS_MOVE:
	if (release)
	    SetTapState(priv, TS_START, hw->millis);
	break;
    case TS_2:
	if (touch) {
	    priv->tap_button_state = TBS_BUTTON_DOWN;
	    SetTapState(priv, TS_3, hw->millis);
	} else if (timeout) {
	    SetTapState(priv, TS_START, hw->millis);
	    priv->tap_button_state = TBS_BUTTON_DOWN_UP;
	}
	break;
    case TS_3:
	if (timeout || move) {
	    SetTapState(priv, TS_DRAG, hw->millis);
	    goto restart;
	} else if (release) {
	    priv->tap_button_state = TBS_BUTTON_UP;
	    SetTapState(priv, TS_2, hw->millis);
	}
	break;
    case TS_DRAG:
	if (release) {
	    if (para->locked_drags)
		SetTapState(priv, TS_4, hw->millis);
	    else
		SetTapState(priv, TS_START, hw->millis);
	}
	break;
    case TS_4:
	if (touch)
	    SetTapState(priv, TS_5, hw->millis);
	break;
    case TS_5:
	if (timeout || move) {
	    SetTapState(priv, TS_DRAG, hw->millis);
	    goto restart;
	} else if (release) {
	    SetTapState(priv, TS_START, hw->millis);
	}
	break;
    }
    return delay;
}

#define MOVE_HIST(a) (priv->move_hist[((priv->count_packet_finger-(a))%SYNAPTICS_MOVE_HISTORY)])

static long ComputeDeltas(SynapticsPrivate *priv, struct SynapticsHwState *hw,
			  edge_type edge, int *dxP, int *dyP)
{
    SynapticsSHM *para = priv->synpara;
    Bool moving_state;
    int dist, dx, dy;
    double speed, integral;
    long delay = 1000000000;

    dx = dy = 0;

    moving_state = FALSE;
    switch (priv->tap_state) {
    case TS_MOVE:
    case TS_DRAG:
	moving_state = TRUE;
	break;
    case TS_1:
    case TS_3:
    case TS_5:
	if (hw->numFingers == 1)
	    moving_state = TRUE;
	break;
    default:
	break;
    }
    if (moving_state && !priv->palm &&
	!priv->vert_scroll_on && !priv->horiz_scroll_on && !priv->circ_scroll_on) {
	delay = MIN(delay, 13);
	if (priv->count_packet_finger > 3) { /* min. 3 packets */
	    dx = (hw->x - MOVE_HIST(2).x) / 2;
	    dy = (hw->y - MOVE_HIST(2).y) / 2;

	    if ((priv->tap_state == TS_DRAG) || para->edge_motion_use_always) {
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
	    dist = move_distance(dx, dy);
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

    /* Add guest device movements */
    if (!para->guestmouse_off) {
	dx += hw->guest_dx;
	dy += hw->guest_dy;
    }

    *dxP = dx;
    *dyP = dy;

    /* generate a history of the absolute positions */
    MOVE_HIST(0).x = hw->x;
    MOVE_HIST(0).y = hw->y;

    return delay;
}

struct ScrollData {
    int left, right, up, down;
};

static void
HandleScrolling(SynapticsPrivate *priv, struct SynapticsHwState *hw,
		edge_type edge, Bool finger, struct ScrollData *sd)
{
    SynapticsSHM *para = priv->synpara;

    sd->left = sd->right = sd->up = sd->down = 0;

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
    if (priv->circ_scroll_on && !finger) {
	/* circular scroll locks in until finger is raised */
	DBG(7, ErrorF("cicular scroll off\n"));
	priv->circ_scroll_on = FALSE;
    }
    if (priv->vert_scroll_on && (!(edge & RIGHT_EDGE) || !finger)) {
	DBG(7, ErrorF("vert edge scroll off\n"));
	priv->vert_scroll_on = FALSE;
    }
    if (priv->horiz_scroll_on && (!(edge & BOTTOM_EDGE) || !finger)) {
	DBG(7, ErrorF("horiz edge scroll off\n"));
	priv->horiz_scroll_on = FALSE;
    }

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

    if (priv->vert_scroll_on) {
	/* + = down, - = up */
	while (hw->y - priv->scroll_y > para->scroll_dist_vert) {
	    sd->down++;
	    priv->scroll_y += para->scroll_dist_vert;
	}
	while (hw->y - priv->scroll_y < -para->scroll_dist_vert) {
	    sd->up++;
	    priv->scroll_y -= para->scroll_dist_vert;
	}
    }
    if (priv->circ_scroll_on) {
	/* + = counter clockwise, - = clockwise */
	while (diffa(priv->scroll_a, angle(priv, hw->x, hw->y)) > para->scroll_dist_circ) {
	    sd->up++;
	    if (sd->up > 1000)
		break; /* safety */
	    priv->scroll_a += para->scroll_dist_circ;
	    if (priv->scroll_a > M_PI)
		priv->scroll_a -= 2 * M_PI;
	}
	while (diffa(priv->scroll_a, angle(priv, hw->x, hw->y)) < -para->scroll_dist_circ) {
	    sd->down++;
	    if (sd->down > 1000)
		break; /* safety */
	    priv->scroll_a -= para->scroll_dist_circ;
	    if (priv->scroll_a < -M_PI)
		priv->scroll_a += 2 * M_PI;
	}
    }
    if (priv->horiz_scroll_on) {
	/* + = right, - = left */
	while (hw->x - priv->scroll_x > para->scroll_dist_horiz) {
	    sd->right++;
	    priv->scroll_x += para->scroll_dist_horiz;
	}
	while (hw->x - priv->scroll_x < -para->scroll_dist_horiz) {
	    sd->left++;
	    priv->scroll_x -= para->scroll_dist_horiz;
	}
    }
}

/*
 * React on changes in the hardware state. This function is called every time
 * the hardware state changes. The return value is used to specify how many
 * milliseconds to wait before calling the function again if no state change
 * occurs.
 */
static int
HandleState(LocalDevicePtr local, struct SynapticsHwState *hw)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) (local->private);
    SynapticsSHM *para = priv->synpara;
    Bool finger;
    int dx, dy, buttons, id;
    edge_type edge;
    Bool mid;
    int change;
    struct ScrollData scroll;
    int double_click;
    long delay = 1000000000;
    long timeleft;
    int i;

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
    timeleft = HandleTapProcessing(priv, hw, edge, finger);
    if (timeleft > 0)
	delay = MIN(delay, timeleft);

    HandleScrolling(priv, hw, edge, finger, &scroll);

    timeleft = ComputeDeltas(priv, hw, edge, &dx, &dy);
    delay = MIN(delay, timeleft);


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

    if (priv->tap_button > 0) {
	int tap_mask = 1 << (priv->tap_button - 1);
	if (priv->tap_button_state == TBS_BUTTON_DOWN_UP) {
	    if (tap_mask != (priv->lastButtons & tap_mask)) {
		xf86PostButtonEvent(local->dev, FALSE, priv->tap_button, TRUE, 0, 0);
		priv->lastButtons |= tap_mask;
	    }
	    priv->tap_button_state = TBS_BUTTON_UP;
	}
	if (priv->tap_button_state == TBS_BUTTON_DOWN)
	    buttons |= tap_mask;
    }

    /* Post events */
    if (dx || dy)
	xf86PostMotionEvent(local->dev, 0, 0, 2, dx, dy);

    change = buttons ^ priv->lastButtons;
    while (change) {
	id = ffs(change); /* number of first set bit 1..32 is returned */
	change &= ~(1 << (id - 1));
	xf86PostButtonEvent(local->dev, FALSE, id, (buttons & (1 << (id - 1))), 0, 0);
    }

    while (scroll.up-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 4, !hw->up, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 4, hw->up, 0, 0);
    }
    while (scroll.down-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 5, !hw->down, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 5, hw->down, 0, 0);
    }
    while (scroll.left-- > 0) {
	xf86PostButtonEvent(local->dev, FALSE, 6, TRUE, 0, 0);
	xf86PostButtonEvent(local->dev, FALSE, 6, FALSE, 0, 0);
    }
    while (scroll.right-- > 0) {
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

    /* Handle auto repeat buttons */
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

    /* Save old values of some state variables */
    priv->finger_flag = finger;
    priv->lastButtons = buttons;

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


Bool
QueryHardware(LocalDevicePtr local)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) local->private;
    SynapticsSHM *para = priv->synpara;

    priv->comm.protoBufTail = 0;

    if (priv->proto_ops->QueryHardware(local, &priv->synhw)) {
	para->synhw = priv->synhw;
	return TRUE;
    }

    if (priv->fifofd == -1) {
	xf86Msg(X_ERROR, "%s no synaptics touchpad detected and no repeater device\n",
		local->name);
	return FALSE;
    }
    xf86Msg(X_PROBED, "%s no synaptics touchpad, data piped to repeater fifo\n", local->name);
    priv->proto_ops->DeviceOffHook(local);
    return TRUE;
}
