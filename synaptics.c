/* 
 *   2002 Peter Osterlund <petero2@telia.com>
 *     patches for fast scrolling, plam detection, dege motion, 
 *     hozizontal scrolling
 *
 *   2002 S. Lehner <sam_x@bluemail.ch>
 *     for newer Firmware (5.8) protocol changes for 3rd to 6th button 
 *
 *	 Copyright (C) 2001 Stefan Gmeiner <riddlebox@freesurf.ch>
 *     start merging tpconfig and gpm code to a xfree-input modul
 *     adding some changes and extensions (ex. 3rd and 4th button)
 * 
 *   Copyright (c) 1999 Henry Davies <hdavies@ameritech.net> for the 
 *     absolut to relative translation code (from the gpm-source)
 *     and some other ideas
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

#include <stdio.h>
#include <misc.h>
#include <xf86.h>
#define NEED_XF86_TYPES
#include <xf86_ansic.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <xisb.h>
#include <exevents.h>			/* Needed for InitValuator/Proximity stuff	*/
#include "mipointer.h"
#include <xf86Optrec.h>  		/* needed for Options */ 


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
#define DIFF_TIME(a, b) (((a)>(b))?(a)-(b):(b)-(a))

#define VERSION "0.11.3p1"

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

static XF86ModuleVersionInfo VersionRec =
{
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
SetupProc(pointer module, pointer options, int *errmaj, int *errmin ) 
{
	xf86AddInputDriver(&SYNAPTICS, module, 0);
	return module;
}

XF86ModuleData synapticsModuleData = {&VersionRec, &SetupProc, NULL };

#endif /* XFree86LOADER */


/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/

/*
 *  called by the module loader for initialtion       
 */
static InputInfoPtr
SynapticsPreInit(InputDriverPtr drv, IDevPtr dev, int flags)
{
	LocalDevicePtr local;
	SynapticsPrivatePtr priv;
	XF86OptionPtr optList; 
	char *str_par;
	int shmid;

	/* allocate memory for SynaticsPrivateRec */
	priv = xcalloc (1, sizeof (SynapticsPrivateRec));
	if (!priv)
		return NULL;

	/* Allocate a new InputInfoRec and add it to the head xf86InputDevs. */
	local = xf86AllocateInput(drv, 0);
	if (!local) 
	{
		xfree(priv);
		return NULL;
	}

	/* initialize the InputInfoRec */
	local->name                    = dev->identifier;
	local->type_name               = XI_MOUSE; /* XI_TOUCHPAD and KDE killed the X-Server at startup ? */
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

	xf86CollectInputOptions(local, NULL, NULL);

	xf86OptionListReport(local->options);

	/* open the touchpad device */
	local->fd = xf86OpenSerial (local->options);
	if (local->fd == -1)
	{
		ErrorF ("Synaptics driver unable to open device\n");
		goto SetupProc_fail;
	}
	xf86ErrorFVerb( 6, "port opened successfully\n" );

	/* initialize variables */
	priv->repeat_timer = NULL;
	priv->repeatButtons = 0;

	/* install shared memory or normal memory for parameter */
	priv->shm_config = FALSE;
	if(xf86SetBoolOption(local->options, "SHMConfig", FALSE)) 
	{
		if ((shmid = xf86shmget(SHM_SYNAPTICS, 0, 0)) != -1)
			xf86shmctl(shmid, XF86IPC_RMID, NULL);
		if((shmid = xf86shmget(SHM_SYNAPTICS, sizeof(SynapticsSHM), 0777 | XF86IPC_CREAT)) == -1) 
		{
			xf86Msg(X_ERROR, "%s error shmget\n", local->name);
			goto SetupProc_fail;
		} 
		else if((priv->synpara = (SynapticsSHM*) xf86shmat(shmid, NULL, 0)) == NULL) 
		{
			xf86Msg(X_ERROR, "%s error shmat\n", local->name);
			goto SetupProc_fail;
		}
		priv->shm_config = TRUE;
	} 
	else 
	{
		priv->synpara = xcalloc (1, sizeof (SynapticsSHM));
		if(!priv->synpara)
			goto SetupProc_fail;
	}

	/* read the parameter */
	priv->synpara->left_edge = xf86SetIntOption(local->options, "LeftEdge", 1900);
	priv->synpara->right_edge = xf86SetIntOption(local->options, "RightEdge", 5400);
	priv->synpara->top_edge = xf86SetIntOption(local->options, "TopEdge", 3900);
	priv->synpara->bottom_edge = xf86SetIntOption(local->options, "BottomEdge", 1800);
	priv->synpara->finger_low = xf86SetIntOption(local->options, "FingerLow", 25);
	priv->synpara->finger_high = xf86SetIntOption(local->options, "FingerHigh", 30);
	priv->synpara->tap_time = xf86SetIntOption(local->options, "MaxTapTime", 20);
	priv->synpara->tap_move = xf86SetIntOption(local->options, "MaxTapMove", 220);
	priv->synpara->emulate_mid_button_time = xf86SetIntOption(local->options,
															  "EmulateMidButtonTime", 6);
	priv->synpara->scroll_dist_vert = xf86SetIntOption(local->options, "VertScrollDelta", 100);
	priv->synpara->scroll_dist_horiz = xf86SetIntOption(local->options, "HorizScrollDelta", 100);
	priv->synpara->edge_motion_speed = xf86SetIntOption(local->options, "EdgeMotionSpeed", 40);
	priv->synpara->repeater = xf86SetStrOption(local->options, "Repeater", NULL);
	priv->synpara->updown_button_scrolling = xf86SetBoolOption(local->options, "UpDownScrolling", TRUE);

	str_par = xf86FindOptionValue(local->options, "MinSpeed");
	if((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->min_speed) != 1))
		priv->synpara->min_speed=0.02;
	str_par = xf86FindOptionValue(local->options, "MaxSpeed");
	if((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->max_speed) != 1))
		priv->synpara->max_speed=0.18;
	str_par = xf86FindOptionValue(local->options, "AccelFactor");
	if((!str_par) || (xf86sscanf(str_par, "%lf", &priv->synpara->accl) != 1))
		priv->synpara->accl=0.0015;

	priv->buffer = XisbNew(local->fd, 200);
	DBG(9, XisbTrace (priv->buffer, 1));

	if(priv->synpara->repeater) 
	{
		/* create repeater fifo */
		if((xf86mknod(priv->synpara->repeater, 666, XF86_S_IFIFO) != 0) &&
		   (xf86errno != xf86_EEXIST)) 
		{
				xf86Msg(X_ERROR, "%s can't create repeater fifo\n", local->name);
				xf86free(priv->synpara->repeater);
				priv->synpara->repeater = NULL;
				priv->fifofd = -1;
		} 
		else 
		{
			/* open the repeater fifo */
			optList = xf86NewOption("Device", priv->synpara->repeater);
			if((priv->fifofd = xf86OpenSerial(optList)) == -1) 
			{
				xf86Msg(X_ERROR, "%s repeater device open failed\n", local->name);
				xf86free(priv->synpara->repeater);
				priv->synpara->repeater = NULL;
				priv->fifofd = -1;
			}
		}
	}

	if(QueryHardware(local) != Success) 
	{
		xf86Msg(X_ERROR, "%s Unable to query/initialize Synaptics hardware.\n", local->name);
		goto SetupProc_fail;
	}

	local->history_size = xf86SetIntOption( local->options, "HistorySize", 0 );

	/* this results in an xstrdup that must be freed later */
	/*local->name = xf86SetStrOption( local->options, "DeviceName", "Synaptics-Touchpad" );*/
	xf86ProcessCommonOptions(local, local->options);
	local->flags |= XI86_CONFIGURED;

	if (local->fd != -1) 
	{ 
		RemoveEnabledDevice (local->fd);
		if (priv->buffer) 
		{
			XisbFree(priv->buffer);
			priv->buffer = NULL;
		}
		xf86CloseSerial(local->fd);
	}
	RemoveEnabledDevice (local->fd);
	local->fd = -1;
	return (local);

  SetupProc_fail:
	if ((local) && (local->fd))
		xf86CloseSerial (local->fd);

	/* If it fails, the name will be printed 
	if ((local) && (local->name))
		xfree (local->name);
	*/

	if ((priv) && (priv->buffer))
		XisbFree (priv->buffer);
	if ((priv) && (priv->synpara))
		xfree (priv->synpara);
	if (priv)
		xfree (priv);
	return (local);
}

/*
 *  Alter the control parameters for the mouse. Note that all special
 *  protocol values are handled by dix.
 */
static void
SynapticsCtrl(DeviceIntPtr device, PtrCtrl *ctrl)
{
	ErrorF("SynapticsCtrl called.\n");
/*
    pInfo = device->public.devicePrivate;
    pMse = pInfo->private;

    pMse->num       = ctrl->num;
    pMse->den       = ctrl->den;
    pMse->threshold = ctrl->threshold;
*/
}

static Bool
DeviceControl (DeviceIntPtr dev, int mode)
{
	Bool	RetValue;

	switch (mode)
	{
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
			SynapticsPrivatePtr priv = (SynapticsPrivatePtr) (local->private);
			RetValue = DeviceOff( dev );
			if (priv->shm_config)
				if ((shmid = xf86shmget(SHM_SYNAPTICS, 0, 0)) != -1)
					xf86shmctl(shmid, XF86IPC_RMID, NULL);
		}
		break;
	default:
		RetValue = BadValue;
	}

	return( RetValue );
}

static Bool
DeviceOn (DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	SynapticsPrivatePtr priv = (SynapticsPrivatePtr) (local->private);

	ErrorF("Synaptics DeviceOn called\n");

	local->fd = xf86OpenSerial(local->options);
	if (local->fd == -1) {
		xf86Msg(X_WARNING, "%s: cannot open input device\n", local->name);
		return (!Success);
	}

	priv->buffer = XisbNew(local->fd, 64);
	if (!priv->buffer) {
			xf86CloseSerial(local->fd);
			local->fd = -1;
			return (!Success);
		}

	xf86FlushInput(local->fd);
	/*xf86AddEnabledDevice (local);*/
	dev->public.on = TRUE;

	/* reinit the pad */
	QueryHardware(local);
	xf86AddEnabledDevice (local);

	return (Success);
}

static Bool
DeviceOff(DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	SynapticsPrivatePtr priv = (SynapticsPrivatePtr) (local->private);

	ErrorF("Synaptics DeviceOff called\n");

	if (local->fd != -1) { 
	xf86RemoveEnabledDevice (local);
		if (priv->buffer) {
			XisbFree(priv->buffer);
			priv->buffer = NULL;
		}
		xf86CloseSerial(local->fd);
	}

	RemoveEnabledDevice (local->fd);
	dev->public.on = FALSE;
	return (Success);
}

static Bool
DeviceInit(DeviceIntPtr dev)
{
	LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
	unsigned char map[] = {0, 1, 2, 3, 4, 5, 6, 7};

	ErrorF("Synaptics DeviceInit called\n");

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
	return(xf86sqrt((dx * dx) + (dy * dy)));
}

static edge_type 
edge_detection( SynapticsPrivatePtr priv, int x, int y )
{
	edge_type edge = 0;

	if(x > priv->synpara->right_edge)
		edge |= RIGHT_EDGE;
	else if(x < priv->synpara->left_edge)
		edge |= LEFT_EDGE;

	if(y > priv->synpara->top_edge)
		edge |= TOP_EDGE;
	else if(y < priv->synpara->bottom_edge)
		edge |= BOTTOM_EDGE;

	return( edge );
}

static CARD32
updownTimer(OsTimerPtr timer, CARD32 now, pointer arg)
{
    LocalDevicePtr local = (LocalDevicePtr) (arg);
	SynapticsPrivatePtr priv = (SynapticsPrivatePtr) (local->private);
    int	sigstate, change, id;

    sigstate = xf86BlockSIGIO ();
	
	change = priv->repeatButtons;
	while(change) {
		id = ffs(change);
		change &= ~(1 << (id - 1));
		xf86PostButtonEvent(local->dev, FALSE, id, FALSE, 0, 0);
		xf86PostButtonEvent(local->dev, FALSE, id, TRUE, 0, 0);
	}

    xf86UnblockSIGIO (sigstate);

	priv->repeat_timer = TimerSet(priv->repeat_timer, 0, 100, updownTimer, local);

    return 0;
}


#define MOVE_HIST(a) ((priv->count_packet_finger-(a))%SYNAPTICS_MOVE_HISTORY)

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
	SynapticsPrivatePtr priv = (SynapticsPrivatePtr) (local->private);
	SynapticsSHMPtr para = priv->synpara;
	Bool finger;
	int x, y, z, w, dist, dx, dy, buttons, id;
	edge_type edge;
	Bool left, mid, right, up, down, cbLeft, cbRight;
	double speed, integral;
	int change;
	int scroll_up, scroll_down, scroll_left, scroll_right;
	int double_click;
	Bool done;

	/* 
	 * set blocking to -1 on the first call because we know there is data to
	 * read. Xisb automatically clears it after one successful read so that
	 * succeeding reads are preceeded buy a select with a 0 timeout to prevent
	 * read from blocking indefinately.
	 */
	XisbBlockDuration(priv->buffer, -1);
	while(SynapticsGetPacket(local, priv) == Success)
	{
		/* process input data */
		x = ((priv->protoBuf[3] & 0x10) << 8) | 
		    ((priv->protoBuf[1] & 0x0f) << 8) | 
		      priv->protoBuf[4];
		y = ((priv->protoBuf[3] & 0x20) << 7) | 
		    ((priv->protoBuf[1] & 0xf0) << 4) | 
		      priv->protoBuf[5];

		/* pressure */
		z = priv->protoBuf[2];
		w = ((priv->protoBuf[0] & 0x30) >> 2) | 
		    ((priv->protoBuf[0] & 0x04) >> 1) | 
		    ((priv->protoBuf[3] & 0x04) >> 2);

		left  = (priv->protoBuf[0] & 0x01) ? TRUE : FALSE;
		mid   = FALSE;
		right = (priv->protoBuf[0] & 0x2) ? TRUE : FALSE;
		up    = FALSE;
		down  = FALSE;
		if (!priv->six_buttons)
		{
			if(SYN_CAP_EXTENDED(priv->capabilities) &&
			   (SYN_CAP_FOUR_BUTTON(priv->capabilities))) {
				up = ((priv->protoBuf[3] & 0x01)) ? TRUE : FALSE;
				if (left)
					up = !up;
				down = ((priv->protoBuf[3] & 0x02)) ? TRUE : FALSE;
				if (right)
					down = !down;
			}
		}
		else
		{ /* type with 6 buttons */
			if (priv->protoBuf[3] == 0xC2) 
			{
				cbLeft  = (priv->protoBuf[4] & 0x02) ? TRUE : FALSE;
				cbRight = (priv->protoBuf[5] & 0x02) ? TRUE : FALSE;
				up      = (priv->protoBuf[4] & 0x01) ? TRUE : FALSE;
				down    = (priv->protoBuf[5] & 0x01) ? TRUE : FALSE;
			}
			else
			{
				cbLeft = cbRight = up = down = FALSE;
			}
		}

		edge = edge_detection(priv, x, y);

		dx = dy = 0;

		/* update finger position in shared memory */
		para->x = x;
		para->y = y;
		para->z = z;
		para->w = w;
		para->left = left;
		para->right = right;
		para->up = up;
		para->down = down;

		/* 3rd button emulation */
		done = FALSE;
		while (!done) {
			Bool timeout;
			switch (priv->mid_emu_state) {
			case MBE_OFF:
				if (left) {
					priv->mid_emu_state = MBE_LEFT;
				} else if (right) {
					priv->mid_emu_state = MBE_RIGHT;
				} else {
					priv->count_button_delay = priv->count_packet;
					done = TRUE;
				}
				break;
			case MBE_LEFT:
				timeout = DIFF_TIME(priv->count_packet, priv->count_button_delay) >=
					para->emulate_mid_button_time;
				if (!left || timeout) {
					left = TRUE;
					priv->mid_emu_state = MBE_OFF;
					done = TRUE;
				} else if (right) {
					priv->mid_emu_state = MBE_MID;
				} else {
					left = FALSE;
					done = TRUE;
				}
				break;
			case MBE_RIGHT:
				timeout = DIFF_TIME(priv->count_packet, priv->count_button_delay) >=
					para->emulate_mid_button_time;
				if (!right || timeout) {
					right = TRUE;
					priv->mid_emu_state = MBE_OFF;
					done = TRUE;
				} else if (left) {
					priv->mid_emu_state = MBE_MID;
				} else {
					right = FALSE;
					done = TRUE;
				}
				break;
			case MBE_MID:
				if (!left && !right) {
					priv->mid_emu_state = MBE_OFF;
				} else {
					mid = TRUE;
					left = right = FALSE;
					done = TRUE;
				}
				break;
			}
		}

		/* Up/Down-button scrolling or middle/double-click */
		double_click = FALSE;
		if (!para->updown_button_scrolling)
		{
			if (down)
			{ /* map down-button to middle-button */
				mid = TRUE;
			}

			if (up)
			{ /* up-button generates double-click */
				if (!priv->prev_up)
					double_click = TRUE;
			}
			priv->prev_up = up;

			/* reset up/down button events */
			up = down = FALSE;
		}

		/* finger detection thru pressure an threshold */
		finger = (((z > para->finger_high) && !priv->finger_flag) ||
		          ((z > para->finger_low)  &&  priv->finger_flag));

 		/* palm detection */
 		if(SYN_CAP_EXTENDED(priv->capabilities) && SYN_CAP_PALMDETECT(priv->capabilities)) {
 			if(finger) {
 				if((z > 200) && (w > 10))
 					priv->palm = TRUE;
 			} else {
 				priv->palm = FALSE;
 			}
 			if(x == 0)
 				priv->avg_w = 0;
 			else
 				priv->avg_w += (w - priv->avg_w + 1) / 2;
 			if(finger && !priv->finger_flag) {
 				int safe_w = MAX(w, priv->avg_w);
 				if(w < 2)
 					finger = TRUE;				/* more than one finger -> not a palm */
 				else if((safe_w < 6) && (priv->prev_z < para->finger_high))
 					finger = TRUE;				/* thin finger, distinct touch -> not a palm */
 				else if((safe_w < 7) && (priv->prev_z < para->finger_high / 2))
 					finger = TRUE;				/* thin finger, distinct touch -> not a palm */
 				else if(z > priv->prev_z + 1)	/* z not stable, may be a palm */
 					finger = FALSE;
 				else if(z < priv->prev_z - 5)	/* z not stable, may be a palm */
 					finger = FALSE;
 				else if(z > 200)				/* z too large -> probably palm */
 					finger = FALSE;
 				else if(w > 10)					/* w too large -> probably palm */
 					finger = FALSE;
 			}
 			priv->prev_z = z;
 		}
 
  		/* tap and drag detection */
 		if(priv->palm) {
 			/* Palm detected, skip tap/drag processing */
 		} else if(finger && !priv->finger_flag) 
		{ /* touched */
			DBG(7, ErrorF("touched - x:%d, y:%d packet:%lu\n", x, y, priv->count_packet));
			if(priv->tap) 
			{
				DBG(7, ErrorF("drag detected - tap time:%lu\n", priv->count_packet_tapping));
				priv->drag = TRUE; /* drag gesture */
			}
			priv->touch_on.x = x;
			priv->touch_on.y = y;
			priv->touch_on.packet = priv->count_packet;
		} 
		else if(!finger && priv->finger_flag) 
		{ /* untouched */
			DBG(7, ErrorF("untouched - x:%d, y:%d packet:%lu finger:%d\n", 
			              x, y, priv->count_packet, priv->finger_count));
			/* check if
			   1. the tap is in tap_time
			   2. the max movement is in tap_move or more than one finger are tapped */
			if((DIFF_TIME(priv->count_packet, priv->touch_on.packet) < para->tap_time) && 
				(((abs(x - priv->touch_on.x) < para->tap_move) &&     
				  (abs(y - priv->touch_on.y) < para->tap_move)) || 
				   priv->finger_count)) 
			{
					if(priv->drag) 
					{
						DBG(7, ErrorF("double tapping detected\n"));
						priv->doubletap = TRUE;
						priv->tap = FALSE;
					} 
					else 
					{
						DBG(7, ErrorF("tapping detected @ "));
						priv->count_packet_tapping = priv->count_packet;
						priv->tap = TRUE;
						if(priv->finger_count == 0) 
						{
							switch(edge) 
							{
								case RIGHT_TOP_EDGE:
									DBG(7, ErrorF("right top edge\n"));
									priv->tap_mid = TRUE;
									break;
								case RIGHT_BOTTOM_EDGE:
									DBG(7, ErrorF("right bottom edge\n"));
									priv->tap_right = TRUE;
									break;
								case LEFT_TOP_EDGE: 
									DBG(7, ErrorF("left top edge\n"));
									break;
								case LEFT_BOTTOM_EDGE: 
									DBG(7, ErrorF("left bottom edge\n"));
									break;
								default:
									DBG(7, ErrorF("no edge\n"));
									priv->tap_left = TRUE;
							}
						} 
						else 
						{
							switch(priv->finger_count) 
							{
								case 2: 	
									DBG(7, ErrorF("two finger tap\n"));
									priv->tap_mid = TRUE;
									break;
								case 3: 	
									DBG(7, ErrorF("three finger tap\n"));
									priv->tap_right = TRUE;
									break;
								default:
									DBG(7, ErrorF("one finger\n"));
									priv->tap_left = TRUE;
							}
						}
					}
			} /* tap detection */ 
			priv->drag = FALSE;
		} /* finger lost */

		/* detecting 2 and 3 fingers */
		if(finger && /* finger is on the surface */
		   (DIFF_TIME(priv->count_packet, priv->touch_on.packet) < para->tap_time) && /* tap time is not succeeded */
		   SYN_CAP_MULTIFINGER(priv->capabilities)) /* touchpad has multifinger capabilities */
		{ /* count fingers when reported */
			if((w == 0) && (priv->finger_count == 0))
				priv->finger_count = 2;
			if(w == 1)
				priv->finger_count = 3;
		} 
		else
		{ /* reset finger counts */	
			priv->finger_count = 0;
		}

		/* reset tapping button flags */
		if(!priv->tap && !priv->drag && !priv->doubletap) 
		{
			priv->tap_left = priv->tap_mid = priv->tap_right = FALSE;
		}

		/* tap processing */
		if(priv->tap && 
		   (DIFF_TIME(priv->count_packet, priv->count_packet_tapping) < para->tap_time)) 
		{
			left  |= priv->tap_left;
			mid   |= priv->tap_mid;
			right |= priv->tap_right;
		} 
		else 
		{
			priv->tap = FALSE;
		}

		/* drag processing */
		if(priv->drag) 
		{
			left  |= priv->tap_left;
			mid   |= priv->tap_mid;
			right |= priv->tap_right;
		}

		/* double tap processing */
		if(priv->doubletap && !priv->finger_flag) 
		{
			left  |= priv->tap_left;
			mid   |= priv->tap_mid;
			right |= priv->tap_right;
			priv->doubletap = FALSE;
		}

		/* scroll detection */
		if(finger && !priv->finger_flag) 
		{
			if(edge & RIGHT_EDGE) 
			{
				priv->vert_scroll_on = TRUE;
				priv->scroll_y = y;
				DBG(7, ErrorF("vert edge scroll detected on right edge\n"));
			}
 			if(edge & BOTTOM_EDGE) {
 				priv->horiz_scroll_on = TRUE;
 				priv->scroll_x = x;
 				DBG(7, ErrorF("horiz edge scroll detected on bottom edge\n"));
 			}
		}
 		if(priv->vert_scroll_on && (!(edge & RIGHT_EDGE) || !finger || priv->palm)) 
		{
			DBG(7, ErrorF("vert edge scroll off\n"));
			priv->vert_scroll_on = FALSE;	
		}
 		if(priv->horiz_scroll_on && (!(edge & BOTTOM_EDGE) || !finger || priv->palm))
		{	
 			DBG(7, ErrorF("horiz edge scroll off\n"));
 			priv->horiz_scroll_on = FALSE;
 		}

		/* scroll processing */
 		scroll_up = 0;
 		scroll_down = 0;
		if(priv->vert_scroll_on) 
		{
 			/* + = up, - = down */
 			while(y - priv->scroll_y > para->scroll_dist_vert) {
 				scroll_up++;
				priv->scroll_y += para->scroll_dist_vert;
			}
 			while(y - priv->scroll_y < -para->scroll_dist_vert) {
 				scroll_down++;
				priv->scroll_y -= para->scroll_dist_vert;
			}
		}
 		scroll_left = 0;
 		scroll_right = 0;
 		if(priv->horiz_scroll_on) {
 			/* + = right, - = left */
 			while(x - priv->scroll_x > para->scroll_dist_horiz) {
 				scroll_right++;
 				priv->scroll_x += para->scroll_dist_horiz;
 			}
 			while(x - priv->scroll_x < -para->scroll_dist_horiz) {
 				scroll_left++;
 				priv->scroll_x -= para->scroll_dist_horiz;
 			}
 		}

		/* movement */
 		if(finger && !priv->vert_scroll_on && !priv->horiz_scroll_on &&
 		   !priv->finger_count && !priv->palm) 
		{
			if(priv->count_packet_finger > 3) 
			{ /* min. 3 packets */
				dy = (1 * 
				   (((priv->move_hist[MOVE_HIST(1)].y + priv->move_hist[MOVE_HIST(2)].y) / 2) - 
					((y 		                      + priv->move_hist[MOVE_HIST(1)].y) / 2)));
				dx = (-1 * 
				   (((priv->move_hist[MOVE_HIST(1)].x + priv->move_hist[MOVE_HIST(2)].x) / 2) - 
					((x                           	  + priv->move_hist[MOVE_HIST(1)].x) / 2)));

 				if (priv->drag) {
 					if (edge & RIGHT_EDGE) {
 						dx += clamp(para->edge_motion_speed - dx, 0, para->edge_motion_speed);
 					} else if (edge & LEFT_EDGE) {
 						dx -= clamp(para->edge_motion_speed + dx, 0, para->edge_motion_speed);
 					}
 					if (edge & TOP_EDGE) {
 						dy -= clamp(para->edge_motion_speed + dy, 0, para->edge_motion_speed);
 					} else if (edge & BOTTOM_EDGE) {
 						dy += clamp(para->edge_motion_speed - dy, 0, para->edge_motion_speed);
 					}
 				}
				
				/* speed in depence of distance/packet */
				dist = move_distance( dx, dy );
				speed = dist * para->accl; 
				if(speed > para->max_speed)
				{ /* set max speed factor */
					speed = para->max_speed;
				}
				else if(speed < para->min_speed)
				{ /* set min speed factor */
					speed = para->min_speed;
				}

				/* save the fraction for adding to the next priv->count_packet */
				priv->frac_x = xf86modf((dx * speed) + priv->frac_x, &integral);
				dx = integral;
				priv->frac_y = xf86modf((dy * speed) + priv->frac_y, &integral);
				dy = integral;
			}

			priv->count_packet_finger++;
		} 
		else 
		{ /* reset packet counter */
			priv->count_packet_finger = 0;
		}


		buttons = ((left  ? 0x01 : 0) |
		           (mid   ? 0x02 : 0) |
		           (right ? 0x04 : 0) |
		           (up    ? 0x08 : 0) |
		           (down  ? 0x10 : 0));		

		priv->count_packet++;

		/* Flags */
		priv->finger_flag = finger;

		/* generate a history of the absolute positions */
		priv->move_hist[MOVE_HIST(0)].y = y;
		priv->move_hist[MOVE_HIST(0)].x = x;

		/* repeat timer for up/down buttons */
		/* when you press a button the packets will only send for a second, so
		   we have to use a timer for repeating */
		if((up || down) && para->updown_button_scrolling) 
		{
			if(!priv->repeat_timer) 
			{
				priv->repeatButtons = buttons & 0x18;
				priv->repeat_timer = TimerSet(priv->repeat_timer, 
				                              0, 200, updownTimer, local);
			}
		} 
		else if(priv->repeat_timer) 
		{
			TimerFree(priv->repeat_timer);
			priv->repeat_timer = NULL;
			priv->repeatButtons = 0;
		}

		/* Post events */
		if(dx || dy)
			xf86PostMotionEvent(local->dev, 0, 0, 2, dx, dy);
		
		change = buttons ^ priv->lastButtons;
		while(change) 
		{
			id = ffs(change);
			change &= ~(1 << (id - 1));
			xf86PostButtonEvent(local->dev, FALSE, id, (buttons & (1 << (id - 1))), 0, 0);
		}
		priv->lastButtons = buttons;
 
 		while(scroll_up-- > 0) {
 			xf86PostButtonEvent(local->dev, FALSE, 4, !up, 0, 0);
 			xf86PostButtonEvent(local->dev, FALSE, 4, up, 0, 0);
 		}
 		while(scroll_down-- > 0) {
 			xf86PostButtonEvent(local->dev, FALSE, 5, !down, 0, 0);
 			xf86PostButtonEvent(local->dev, FALSE, 5, down, 0, 0);
 		}
 		while(scroll_left-- > 0) {
 			xf86PostButtonEvent(local->dev, FALSE, 6, TRUE, 0, 0);
 			xf86PostButtonEvent(local->dev, FALSE, 6, FALSE, 0, 0);
 		}
 		while(scroll_right-- > 0) {
 			xf86PostButtonEvent(local->dev, FALSE, 7, TRUE, 0, 0);
 			xf86PostButtonEvent(local->dev, FALSE, 7, FALSE, 0, 0);
 		}
		if (double_click) {
			int i;
			for (i = 0; i < 2; i++) {
				xf86PostButtonEvent(local->dev, FALSE, 1, !left, 0, 0);
				xf86PostButtonEvent(local->dev, FALSE, 1, left, 0, 0);
			}
		}
	}
}

static int
ControlProc(LocalDevicePtr local, xDeviceCtl * control)
{
	ErrorF("Control Proc called\n");
	return (Success);
}


static void
CloseProc (LocalDevicePtr local)
{
	ErrorF("Close Proc called\n");
}

static int
SwitchMode (ClientPtr client, DeviceIntPtr dev, int mode)
{
	ErrorF("SwitchMode called\n");
	return (Success);
}

static Bool
ConvertProc (LocalDevicePtr local,
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
	if(first != 0 || num != 2)
		return FALSE;

	*x = v0;
	*y = v1;

	return TRUE;
}


static Bool
QueryHardware (LocalDevicePtr local)
{
	SynapticsPrivatePtr priv = (SynapticsPrivatePtr) local->private;
	SynapticsSHMPtr para = priv->synpara;
	int retries;
	
	xf86Msg(X_INFO, "xfree driver for the synaptics touchpad %s\n", VERSION);

	/* is the synaptics touchpad active? */
	priv->isSynaptics = QueryIsSynaptics(local->fd);

	if((!priv->isSynaptics) && (!para->repeater || (priv->fifofd == -1))) {
		xf86Msg(X_ERROR, "%s no synaptics touchpad detected and no repeater device\n", 
		        local->name);		
		priv->isSynaptics = TRUE;
		return(!Success);
	}
	para->isSynaptics = priv->isSynaptics;

	if(!priv->isSynaptics) {
		xf86Msg(X_PROBED, "%s no synaptics touchpad, data piped to repeater fifo\n", local->name);
		synaptics_reset(local->fd);
		SynapticsEnableDevice(local->fd);
		return(Success);
	}

	xf86Msg(X_PROBED, "%s synaptics touchpad found\n", local->name);

	retries = 3;
	while((retries++ <= 3) && (synaptics_reset(local->fd) != Success)) 
		xf86Msg(X_ERROR, "%s reset failed\n", local->name);

	if(synaptics_identify(local->fd, &priv->identity) != Success) 
		return !Success;
	para->identity = priv->identity;

	if(synaptics_model_id(local->fd, &priv->model_id) != Success) 	
		return !Success;
	para->model_id = priv->model_id;

	if(synaptics_capability(local->fd, &priv->capabilities) != Success)
		return !Success;
	para->capabilities = priv->capabilities;

	if(synaptics_set_mode(local->fd, SYN_BIT_ABSOLUTE_MODE |
	                                 SYN_BIT_HIGH_RATE |
	                                 SYN_BIT_DISABLE_GESTURE |
	                                 SYN_BIT_W_MODE) != Success)
		return !Success;

	priv->six_buttons = FALSE;
	if ((SYN_ID_MAJOR(priv->identity) > 5) ||
		((SYN_ID_MAJOR(priv->identity) == 5) && (SYN_ID_MINOR(priv->identity) >= 8)))
	{
		xf86Msg(X_PROBED, "Using 6 button protocol\n");
		priv->six_buttons = TRUE;
	}

	SynapticsEnableDevice(local->fd);

	PrintIdent(priv);

	return Success;	
}

static Bool
SynapticsGetPacket(LocalDevicePtr local, SynapticsPrivatePtr priv)
{
	int count = 0;
	int c;
	unsigned char *pBuf, u;

	pBuf = priv->protoBuf;

	while((c = XisbRead(priv->buffer)) >= 0) {
		u = (unsigned char)c;

		/* test if there is a reset sequence received */
		if((c == 0x00) && (priv->lastByte == 0xAA)) 
		{
			if(xf86WaitForInput(local->fd, 50000) == 0 ) 
			{
				DBG(7, ErrorF("Reset received\n"));
				QueryHardware(local);
			} else
				ErrorF("faked reset received\n");
		}
		priv->lastByte = u;

		/* when there is no synaptics touchpad pipe the data to the repeater fifo */
		if(!priv->isSynaptics) 
		{
			xf86write(priv->fifofd, &u, 1);
			if(++count >= 3)
				return(!Success);
			continue;
		}

		/* to avoid endless loops */
		if(count++ > 100) 
		{
			return (!Success);
		}

		pBuf[priv->protoBufTail++] = u;

		/* check first byte */
		if((priv->protoBufTail == 1) && ((u & 0xC8) != 0x80)) 
		{
			priv->inSync = FALSE;
			priv->protoBufTail = 0;
			DBG(4, ErrorF("Synaptics driver lost sync at 1st byte\n"));
			continue;
		}

		/* check 4th byte */
		if((priv->protoBufTail == 4) && ((u & 0xc8) != 0xc0)) 
		{
			priv->inSync = FALSE;
			priv->protoBufTail = 0;
			DBG(4, ErrorF("Synaptics driver lost sync at 4th byte\n"));
			continue;
		}

		if(priv->protoBufTail >= 6) 
		{ /* Full packet received */
			if(!priv->inSync) {
				priv->inSync = TRUE;
				DBG(4, ErrorF("Synaptics driver resynced.\n"));
			}
			priv->protoBufTail = 0;
			return Success;	
		}
	}

	return !Success;
}

static void
PrintIdent(SynapticsPrivatePtr priv)
{
	xf86Msg(X_PROBED, " Synaptics Touchpad, model: %d\n", SYN_ID_MODEL(priv->identity));
	xf86Msg(X_PROBED, " Firware: %d.%d\n", SYN_ID_MAJOR(priv->identity), SYN_ID_MINOR(priv->identity));

	if(SYN_MODEL_ROT180(priv->model_id))
		xf86Msg(X_PROBED, " 180 degree mounted touchpad\n");
	if(SYN_MODEL_PORTRAIT(priv->model_id))
		xf86Msg(X_PROBED, " portrait touchpad\n");
	xf86Msg(X_PROBED, " Sensor: %d\n", SYN_MODEL_SENSOR(priv->model_id));
	if(SYN_MODEL_NEWABS(priv->model_id))
		xf86Msg(X_PROBED, " new absolute packet format\n");
	if(SYN_MODEL_PEN(priv->model_id))
		xf86Msg(X_PROBED, " pen detection\n");

	if(SYN_CAP_EXTENDED(priv->capabilities)) {
		xf86Msg(X_PROBED, " Touchpad has extended capability bits\n");
		if(SYN_CAP_FOUR_BUTTON(priv->capabilities))
			xf86Msg(X_PROBED, " -> four buttons\n");
		if(SYN_CAP_MULTIFINGER(priv->capabilities))
			xf86Msg(X_PROBED, " -> multifinger detection\n");
		if(SYN_CAP_PALMDETECT(priv->capabilities))
			xf86Msg(X_PROBED, " -> palm detection\n");
	}
}

/* Local Variables: */
/* tab-width: 4 */
/* End: */
/* vim:ts=2:sw=2:cindent:
*/
