#ifndef	_SYNAPTICS_H_
#define _SYNAPTICS_H_

#include "linux_input.h"

/******************************************************************************
 *		Public definitions.
 *			Used by driver and the shared memory configurator
 *****************************************************************************/
#define SHM_SYNAPTICS 23947
typedef struct _SynapticsSHM
{
    /* Current device state */
    int x, y;				    /* actual x, y Coordinates */
    int z;				    /* pressure value */
    int w;				    /* finger width value */
    int left, right, up, down;		    /* left/right/up/down buttons */

    /* Probed hardware properties */
    unsigned long int model_id;		    /* Model-ID */
    unsigned long int capabilities;	    /* Capabilities */
    unsigned long int ext_cap;		    /* Extended Capabilities */
    unsigned long int identity;		    /* Identification */
    Bool isSynaptics;			    /* Synaptics touchpad active */

    /* Parameter data */
    int	left_edge, right_edge, top_edge, bottom_edge;
    /* edge coordinates absolute */
    int	finger_low, finger_high;	    /* finger detection values in Z-values */
    unsigned long tap_time;
    int tap_move;			    /* max. tapping-time and movement in packets and coord. */
    int emulate_mid_button_time;	    /* Max time between left and right button presses to
					       emulate a middle button press. */
    int	scroll_dist_vert;		    /* Scrolling distance in absolute coordinates */
    int	scroll_dist_horiz;		    /* Scrolling distance in absolute coordinates */
    double min_speed, max_speed, accl;	    /* movement parameters */
    int edge_motion_speed;		    /* Edge motion speed when dragging */
    char* repeater;			    /* Repeater on or off */
    Bool updown_button_scrolling;	    /* Up/Down-Button scrolling or middle/double-click */
} SynapticsSHM, *SynapticsSHMPtr;

#ifdef SYNAPTICS_PRIVATE
/******************************************************************************
 *		Definitions
 *					structs, typedefs, #defines, enums
 *****************************************************************************/
#define SYNAPTICS_MOVE_HISTORY	5

/*
 * A structure to describe the state of the touchpad hardware (buttons and pad)
 */
struct SynapticsHwState {
    int millis;			/* Timestamp in milliseconds */
    int x;			/* X position of finger */
    int y;			/* Y position of finger */
    int z;			/* Finger pressure */
    int w;			/* Finger width/finger count */
    Bool left;
    Bool right;
    Bool up;
    Bool down;
    Bool cbLeft;
    Bool cbRight;
};

typedef struct _SynapticsTapRec
{
    int x, y;
    unsigned int millis;
} SynapticsTapRec;

typedef struct _SynapticsMoveHist
{
    int x, y;
} SynapticsMoveHistRec;

enum MidButtonEmulation {
    MBE_OFF,			/* No button pressed */
    MBE_LEFT,			/* Left button pressed, waiting for right button or timeout */
    MBE_RIGHT,			/* Right button pressed, waiting for left button or timeout */
    MBE_MID,			/* Left and right buttons pressed, waiting for both buttons
				   to be released */
    MBE_TIMEOUT			/* Waiting for both buttons to be released. */
};

enum SynapticsProtocol {
    SYN_PROTO_PSAUX,		/* Raw psaux device */
    SYN_PROTO_EVENT		/* Linux kernel event interface */
};

typedef struct _SynapticsPrivateRec
{
    /* shared memory pointer */
    SynapticsSHMPtr synpara;

    enum SynapticsProtocol proto;

    struct SynapticsHwState hwState;

    /* Data read from the touchpad */
    unsigned long int model_id;		/* Model-ID */
    unsigned long int capabilities; 	/* Capabilities */
    unsigned long int ext_cap;		/* Extended Capabilities */
    unsigned long int identity;		/* Identification */
    Bool isSynaptics;			/* Synaptics touchpad active */
    Bool shm_config;			/* True when shared memory area allocated */

    OsTimerPtr timer;			/* for up/down-button repeat, tap processing, etc */

    /* Data for normal processing */
    XISBuffer *buffer;
    unsigned char protoBuf[6];		/* Buffer for Packet */
    unsigned char lastByte;		/* letztes gelesene byte */
    int outOfSync;			/* How many consecutive incorrect packets we
					   have received */
    int protoBufTail;
    int fifofd;		 		/* fd for fifo */
    SynapticsTapRec touch_on;		/* data when the touchpad is touched */
    SynapticsMoveHistRec move_hist[SYNAPTICS_MOVE_HISTORY]; /* movement history */

    int scroll_y;			/* last y-scroll position */
    int scroll_x;			/* last x-scroll position */
    unsigned long count_packet_finger;	/* packet counter with finger on the touchpad */
    unsigned int tapping_millis;	/* packet counter for tapping */
    unsigned int button_delay_millis;	/* button delay for 3rd button emulation */
    unsigned int prev_up;		/* Previous up button value, for double click emulation */
    Bool finger_flag;			/* previous finger */
    Bool tap, drag, doubletap;		/* feature flags */
    Bool tap_left, tap_mid, tap_right;	/* tapping buttons */
    Bool vert_scroll_on;		/* scrolling flag */
    Bool horiz_scroll_on;		/* scrolling flag */
    double frac_x, frac_y;		/* absoulte -> relative fraction */
    enum MidButtonEmulation mid_emu_state;	/* emulated 3rd button */
    int repeatButtons;			/* buttons for repeat */
    unsigned long nextRepeat;		/* Time when to trigger next auto repeat event */
    int lastButtons;			/* last State of the buttons */
    int finger_count;			/* tap counter for fingers */
    int palm;				/* Set to true when palm detected, reset to false when
					   palm/finger contact disappears */
    int prev_z;				/* previous z value, for palm detection */
    int avg_w;				/* weighted average of previous w values */
}
SynapticsPrivateRec, *SynapticsPrivatePtr;


static Bool DeviceControl(DeviceIntPtr, int);
static void ReadInput(LocalDevicePtr);
static int HandleState(LocalDevicePtr, struct SynapticsHwState*);
static int ControlProc(LocalDevicePtr, xDeviceCtl*);
static void CloseProc(LocalDevicePtr);
static int SwitchMode(ClientPtr, DeviceIntPtr, int);
static Bool ConvertProc(LocalDevicePtr, int, int, int, int, int, int, int, int, int*, int*);
static Bool QueryHardware(LocalDevicePtr);
static Bool DeviceInit(DeviceIntPtr);
static Bool DeviceOn(DeviceIntPtr);
static Bool DeviceOff(DeviceIntPtr);
static Bool DeviceInit(DeviceIntPtr);
static Bool SynapticsGetHwState(LocalDevicePtr, SynapticsPrivatePtr, struct SynapticsHwState*);
static Bool SynapticsParseEventData(LocalDevicePtr, SynapticsPrivatePtr,
				    struct SynapticsHwState*);
static Bool SynapticsReadEvent(SynapticsPrivatePtr, struct input_event*);
static Bool SynapticsParseRawPacket(LocalDevicePtr, SynapticsPrivatePtr,
				    struct SynapticsHwState*);
static Bool SynapticsGetPacket(LocalDevicePtr, SynapticsPrivatePtr);
static void PrintIdent(SynapticsPrivatePtr);

#endif


/*
 *    DO NOT PUT ANYTHING AFTER THIS ENDIF
 */
#endif /* _SYNAPTICS_H_ */
