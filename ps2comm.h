#ifndef _PS2COMM_H_
#define _PS2COMM_H_

/* synatics modes */
#define SYN_BIT_ABSOLUTE_MODE (1<<7)
#define SYN_BIT_HIGH_RATE (1<<6)
#define SYN_BIT_SLEEP_MODE (1<<3)
#define SYN_BIT_DISABLE_GESTURE (1<<2)
#define SYN_BIT_W_MODE (1<<0)

/* synaptics model ID bits */
#define SYN_MODEL_ROT180(m) (m&(1<<23))
#define SYN_MODEL_PORTRAIT(m) (m&(1<<22))
#define SYN_MODEL_SENSOR(m) ((m>>16)&0x3f)
#define SYN_MODEL_HARDWARE(m) ((m>>9)0x7f)
#define SYN_MODEL_NEWABS(m) (m&(1<<7))
#define SYN_MODEL_PEN(m) (m&(1<<6))
#define SYN_MODEL_SIMPLIC(m) (m&(1<<5))
#define SYN_MODEL_GEOMETRY(m) (m&0x0f)

/* synaptics capability bits */
#define SYN_CAP_EXTENDED(c) (c&(1<<23))
#define SYN_CAP_SLEEP(c) (c&(1<<4))
#define SYN_CAP_FOUR_BUTTON(c) (c&(1<<3))
#define SYN_CAP_MULTIFINGER(c) (c&(1<<1))
#define SYN_CAP_PALMDETECT(c) (c&(1<<0))
#define SYN_CAP_VALID(c) (((c&0x00ff00)>>8)==0x47)
#define SYN_EXT_CAP_REQUESTS(c) ((c&0x700000) == 0x100000)
#define SYN_CAP_MULTI_BUTTON_NO(ec) ((ec&0x00f000)>>12)

/* synaptics modes query bits */
#define SYN_MODE_ABSOLUTE(m) (m&(1<<7)) 
#define SYN_MODE_RATE(m) (m&(1<<6))
#define SYN_MODE_BAUD_SLEEP(m) (m&(1<<3))
#define SYN_MODE_DISABLE_GESTURE(m) (m&(1<<2))
#define SYN_MODE_PACKSIZE(m) (m&(1<<1))
#define SYN_MODE_WMODE(m) (m&(1<<0))
#define SYN_MODE_VALID(m) ((m&0xffff00)==0x3B47)

/* synaptics identify query bits */
#define SYN_ID_MODEL(i) ((i>>4)&0x0f)
#define SYN_ID_MAJOR(i) (i&0x0f)
#define SYN_ID_MINOR(i) ((i>>16)&0xff)
#define SYN_ID_IS_SYNAPTICS(i) (((i>>8)&0xff)==0x47)

typedef unsigned char byte;

Bool
synaptics_reset(int fd);

Bool
synaptics_model_id(int fd, unsigned long int *model_id);

Bool
synaptics_capability(int fd, unsigned long int *capability, unsigned long int *ext_capab);

Bool
synaptics_identify(int fd, unsigned long int *ident);

Bool 
synaptics_set_mode(int fd, byte cmd); 

Bool
synaptics_read_mode(int fd, unsigned char *mode);

Bool
SynapticsEnableDevice(int fd);

Bool
QueryIsSynaptics(int fd);

#endif /* _PS2COMM_H_ */
