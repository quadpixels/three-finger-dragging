#ifndef	_SYNAPTICS_H_
#define _SYNAPTICS_H_

#include "linux_input.h"
#include <X11/Xdefs.h>

/******************************************************************************
 *		Public definitions.
 *			Used by driver and the shared memory configurator
 *****************************************************************************/
typedef enum {
    RT_TAP = 0,				    /* Right top corner */
    RB_TAP,				    /* Right bottom corner */
    LT_TAP,				    /* Left top corner */
    LB_TAP,				    /* Left bottom corner */
    F1_TAP,				    /* Non-corner tap, one finger */
    F2_TAP,				    /* Non-corner tap, two fingers */
    F3_TAP,				    /* Non-corner tap, three fingers */
    MAX_TAP
} TapEvent;

typedef struct synapticshw {
    unsigned long int model_id;		    /* Model-ID */
    unsigned long int capabilities;	    /* Capabilities */
    unsigned long int ext_cap;		    /* Extended Capabilities */
    unsigned long int identity;		    /* Identification */
} synapticshw_t;


#define SHM_SYNAPTICS 23947
typedef struct _SynapticsSHM
{
    /* Current device state */
    int x, y;				    /* actual x, y coordinates */
    int z;				    /* pressure value */
    int numFingers;			    /* number of fingers */
    int fingerWidth;			    /* finger width value */
    int left, right, up, down;		    /* left/right/up/down buttons */
    Bool multi[8];
    Bool middle;
    int guest_left, guest_mid, guest_right; /* guest device buttons */
    int guest_dx, guest_dy; 		    /* guest device movement */

    /* Probed hardware properties */
    synapticshw_t synhw;

    Bool isSynaptics;			    /* Synaptics touchpad active */

    /* Parameter data */
    int	left_edge, right_edge, top_edge, bottom_edge; /* edge coordinates absolute */
    int	finger_low, finger_high;	    /* finger detection values in Z-values */
    unsigned long tap_time;
    int tap_move;			    /* max. tapping-time and movement in packets and coord. */
    int emulate_mid_button_time;	    /* Max time between left and right button presses to
					       emulate a middle button press. */
    int	scroll_dist_vert;		    /* Scrolling distance in absolute coordinates */
    int	scroll_dist_horiz;		    /* Scrolling distance in absolute coordinates */
    double min_speed, max_speed, accl;	    /* movement parameters */
    int edge_motion_min_z;		    /* finger pressure at which minimum edge motion speed is set */
    int edge_motion_max_z;		    /* finger pressure at which maximum edge motion speed is set */
    int edge_motion_min_speed;		    /* slowest setting for edge motion speed */
    int edge_motion_max_speed;		    /* fastest setting for edge motion speed */
    Bool edge_motion_use_always;	    /* If false, egde motion is used only when dragging */

    char* repeater;			    /* Repeater on or off */
    Bool updown_button_scrolling;	    /* Up/Down-Button scrolling or middle/double-click */
    Bool touchpad_off;			    /* Switches the Touchpad off*/
    Bool locked_drags;			    /* Enable locked drags */
    int tap_action[MAX_TAP];		    /* Button to report on tap events */
    Bool circular_scrolling;		    /* Enable circular scrolling */
    double scroll_dist_circ;		    /* Scrolling angle radians */
    int circular_trigger;		    /* Trigger area for circular scrolling */
} SynapticsSHM;

#ifdef SYNAPTICS_PRIVATE

#include "synproto.h"

/******************************************************************************
 *		Definitions
 *					structs, typedefs, #defines, enums
 *****************************************************************************/
#define SYNAPTICS_MOVE_HISTORY	5

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

/* See docs/tapndrag.dia for a state machine diagram */
enum TapState {
    TS_START,			/* No tap/drag in progress */
    TS_1,			/* After first touch */
    TS_MOVE,			/* Pointer movement enabled */
    TS_2,			/* After first release */
    TS_3,			/* After second touch */
    TS_DRAG,			/* Pointer drag enabled */
    TS_4,			/* After release when "locked drags" enabled */
    TS_5			/* After touch when "locked drags" enabled */
};

enum TapButtonState {
    TBS_BUTTON_UP,		/* "Virtual tap button" is up */
    TBS_BUTTON_DOWN,		/* "Virtual tap button" is down */
    TBS_BUTTON_UP_DOWN		/* Send button up event + set down state */
};

typedef struct _SynapticsPrivateRec
{
    /* shared memory pointer */
    SynapticsSHM *synpara;

    enum SynapticsProtocol proto;
    struct SynapticsProtocolOperations* proto_ops;

    struct SynapticsHwState hwState;

    /* Data read from the touchpad */
    synapticshw_t synhw;
    Bool isSynaptics;			/* Synaptics touchpad active */
    Bool hasGuest;			/* Has a guest mouse */
    Bool shm_config;			/* True when shared memory area allocated */

    OsTimerPtr timer;			/* for up/down-button repeat, tap processing, etc */

    /* Data for normal processing */
    XISBuffer *buffer;
    unsigned char protoBuf[6];		/* Buffer for Packet */
    unsigned char lastByte;		/* Last read byte. Use for reset sequence detection. */
    int outOfSync;			/* How many consecutive incorrect packets we
					   have received */
    int protoBufTail;
    int fifofd;		 		/* fd for fifo */
    SynapticsMoveHistRec move_hist[SYNAPTICS_MOVE_HISTORY]; /* movement history */

    int largest_valid_x;		/* Largest valid X coordinate seen so far */
    int scroll_y;			/* last y-scroll position */
    int scroll_x;			/* last x-scroll position */
    double scroll_a;			/* last angle-scroll position */
    unsigned long count_packet_finger;	/* packet counter with finger on the touchpad */
    unsigned int button_delay_millis;	/* button delay for 3rd button emulation */
    unsigned int prev_up;		/* Previous up button value, for double click emulation */
    Bool finger_flag;			/* previous finger */

    enum TapState tap_state;		/* State of tap processing */
    int tap_max_fingers;		/* Max number of fingers seen since entering start state */
    int tap_button;			/* Which button started the tap processing */
    enum TapButtonState tap_button_state; /* Current tap action */
    SynapticsTapRec touch_on;		/* data when the touchpad is touched/released */

    Bool vert_scroll_on;		/* scrolling flag */
    Bool horiz_scroll_on;		/* scrolling flag */
    Bool circ_scroll_on;		/* scrolling flag */
    double frac_x, frac_y;		/* absolute -> relative fraction */
    enum MidButtonEmulation mid_emu_state;	/* emulated 3rd button */
    int repeatButtons;			/* buttons for repeat */
    unsigned long nextRepeat;		/* Time when to trigger next auto repeat event */
    int lastButtons;			/* last state of the buttons */
    int palm;				/* Set to true when palm detected, reset to false when
					   palm/finger contact disappears */
    int prev_z;				/* previous z value, for palm detection */
    int avg_width;			/* weighted average of previous fingerWidth values */

    Bool oneFinger;			/* Used by SynapticsParseEventData to */
    Bool twoFingers;			/* keep track of the number of fingers */
    Bool threeFingers;			/* on the touchpad. */
} SynapticsPrivate;


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
static Bool SynapticsGetHwState(LocalDevicePtr, SynapticsPrivate *, struct SynapticsHwState *);
static Bool SynapticsParseEventData(LocalDevicePtr, SynapticsPrivate *,
				    struct SynapticsHwState *);
static Bool SynapticsReadEvent(SynapticsPrivate *, struct input_event *);
static Bool SynapticsParseRawPacket(LocalDevicePtr, SynapticsPrivate *,
				    struct SynapticsHwState *);
static Bool SynapticsGetPacket(LocalDevicePtr, SynapticsPrivate *);
static void PrintIdent(const synapticshw_t *);

#endif /* SYNAPTICS_PRIVATE */


#endif /* _SYNAPTICS_H_ */
