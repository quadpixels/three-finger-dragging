#ifndef _LINUX_INPUT_H_
#define _LINUX_INPUT_H_

/*
 * These defines are taken from input.h in the linux kernel source tree.
 * Copyright (c) 1999-2002 Vojtech Pavlik
 */

/*
 * The event structure.
 */
struct input_event {
	unsigned long tv_sec;
	unsigned long tv_usec;
	unsigned short type;
	unsigned short code;
	int value;
};

struct input_id {
	unsigned short bustype;
	unsigned short vendor;
	unsigned short product;
	unsigned short version;
};

#define EVIOCGID		_IOR('E', 0x02, struct input_id)	/* get device ID */
#define EVIOCGRAB		_IOW('E', 0x90, int)			/* Grab/Release device */


#define EV_SYN			0x00
#define EV_KEY			0x01
#define EV_REL			0x02
#define EV_ABS			0x03
#define EV_MSC			0x04

#define SYN_REPORT		0

#define BTN_LEFT		0x110
#define BTN_RIGHT		0x111
#define BTN_MIDDLE		0x112
#define BTN_FORWARD		0x115
#define BTN_BACK		0x116
#define BTN_0			0x100
#define BTN_1			0x101
#define BTN_2			0x102
#define BTN_3			0x103
#define BTN_4			0x104
#define BTN_5			0x105
#define BTN_6			0x106
#define BTN_7			0x107
#define BTN_TOOL_FINGER		0x145
#define BTN_TOOL_DOUBLETAP	0x14d
#define BTN_TOOL_TRIPLETAP	0x14e

#define ABS_X			0x00
#define ABS_Y			0x01
#define ABS_PRESSURE		0x18
#define ABS_TOOL_WIDTH		0x1c

#define MSC_GESTURE		0x02


#define BUS_I8042		0x11

#define PSMOUSE_SYNAPTICS 	7
#define PSMOUSE_ALPS 		8


#endif /* _LINUX_INPUT_H_ */
