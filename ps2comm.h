#ifndef _PS2COMM_H_
#define _PS2COMM_H_

#include <sys/ioctl.h>
#include "xf86_OSproc.h"

/* synatics modes */
#define SYN_BIT_ABSOLUTE_MODE		(1 << 7)
#define SYN_BIT_HIGH_RATE		(1 << 6)
#define SYN_BIT_SLEEP_MODE		(1 << 3)
#define SYN_BIT_DISABLE_GESTURE		(1 << 2)
#define SYN_BIT_W_MODE			(1 << 0)

/* synaptics model ID bits */
#define SYN_MODEL_ROT180(synhw) 	((synhw).model_id & (1 << 23))
#define SYN_MODEL_PORTRAIT(synhw)	((synhw).model_id & (1 << 22))
#define SYN_CAP_MIDDLE_BUTTON(synhw)	((synhw).model_id & (1 << 18))
#define SYN_MODEL_SENSOR(synhw)		(((synhw).model_id >> 16) & 0x3f)
#define SYN_MODEL_HARDWARE(synhw)	(((synhw).model_id >> 9) & 0x7f)
#define SYN_MODEL_NEWABS(synhw)		((synhw).model_id & (1 << 7))
#define SYN_MODEL_PEN(synhw)		((synhw).model_id & (1 << 6))
#define SYN_MODEL_SIMPLIC(synhw)	((synhw).model_id & (1 << 5))
#define SYN_MODEL_GEOMETRY(synhw)	((synhw).model_id & 0x0f)

/* synaptics capability bits */
#define SYN_CAP_EXTENDED(synhw)		((synhw).capabilities & (1 << 23))
#define SYN_CAP_PASSTHROUGH(synhw)	((synhw).capabilities & (1 << 7))
#define SYN_CAP_SLEEP(synhw)		((synhw).capabilities & (1 << 4))
#define SYN_CAP_FOUR_BUTTON(synhw)	((synhw).capabilities & (1 << 3))
#define SYN_CAP_MULTIFINGER(synhw)	((synhw).capabilities & (1 << 1))
#define SYN_CAP_PALMDETECT(synhw)	((synhw).capabilities & (1 << 0))
#define SYN_CAP_VALID(synhw)		((((synhw).capabilities & 0x00ff00) >> 8) == 0x47)
#define SYN_EXT_CAP_REQUESTS(synhw)	(((synhw).capabilities & 0x700000) == 0x100000)
#define SYN_CAP_MULTI_BUTTON_NO(synhw)	(((synhw).ext_cap & 0x00f000) >> 12)

/* synaptics modes query bits */
#define SYN_MODE_ABSOLUTE(synhw)	((synhw).model_id & (1 << 7))
#define SYN_MODE_RATE(synhw)		((synhw).model_id & (1 << 6))
#define SYN_MODE_BAUD_SLEEP(synhw)	((synhw).model_id & (1 << 3))
#define SYN_MODE_DISABLE_GESTURE(synhw)	((synhw).model_id & (1 << 2))
#define SYN_MODE_PACKSIZE(synhw)	((synhw).model_id & (1 << 1))
#define SYN_MODE_WMODE(synhw)		((synhw).model_id & (1 << 0))
#define SYN_MODE_VALID(synhw)		(((synhw).model_id & 0xffff00) == 0x3B47)

/* synaptics identify query bits */
#define SYN_ID_MODEL(synhw)		(((synhw).identity >> 4) & 0x0f)
#define SYN_ID_MAJOR(synhw)		((synhw).identity & 0x0f)
#define SYN_ID_MINOR(synhw)		(((synhw).identity >> 16) & 0xff)
#define SYN_ID_IS_SYNAPTICS(synhw)	((((synhw).identity >> 8) & 0xff) == 0x47)

typedef unsigned char byte;

Bool
synaptics_reset(int fd);

Bool
SynapticsEnableDevice(int fd);

#endif /* _PS2COMM_H_ */
