/* Copyright (C) 2001 Stefan Gmeiner <riddlebox@freesurf.ch>
 * 
 *   Copyright (c) 1997 C. Scott Ananian <cananian@alumni.priceton.edu>
 *   Copyright (c) 1998-2000 Bruce Kalk <kall@compass.com>
 *     code für the special synaptics commands (from the tpconfig-source)
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
 */

#include "xf86_OSproc.h"

#include "ps2comm.h"

/* acknowledge for commands and parameter */
#define PS2_ACK 0xFA
#define PS2_ERROR 0xFC

/* standard PS/2 commands */
#define PS2_CMD_RESET 0xFF
#define PS2_CMD_RESEND 0xFE
#define PS2_CMD_SET_DEFAULT 0xF6
#define PS2_CMD_DISABLE 0xF5
#define PS2_CMD_ENABLE 0xF4
#define PS2_CMD_SET_SAMPLE_RATE 0xF3
#define PS2_CMD_READ_DEVICE_TYPE 0xF2
#define PS2_CMD_SET_REMOTE_MODE 0xF0
#define PS2_CMD_SET_WRAP_MODE 0xEE
#define PS2_CMD_RESET_WRAP_MODE 0xEC
#define PS2_CMD_READ_DATA 0xEB
#define PS2_CMD_SET_STREAM_MODE 0xEA
#define PS2_CMD_STATUS_REQUEST 0xE9
#define PS2_CMD_SET_RESOLUTION 0xE8
#define PS2_CMD_SET_SCALING_2_1 0xE7
#define PS2_CMD_SET_SCALING_1_1 0xE6

/* synaptics queries */
#define SYN_QUE_IDENTIFY 0x00
#define SYN_QUE_MODES 0x01
#define SYN_QUE_CAPABILITIES 0x02
#define SYN_QUE_MODEL 0x03
#define SYN_QUE_SERIAL_NUMBER_PREFIX 0x06
#define SYN_QUE_SERIAL_NUMBER_SUFFIX 0x07
#define SYN_QUE_RESOLUTION 0x08

/* status request response bits (PS2_CMD_STATUS_REQUEST) */
#define PS2_RES_REMOTE(r) (r&(1<<22))
#define PS2_RES_ENABLE(r) (r&(1<<21))
#define PS2_RES_SCALING(r) (r&(1<<20))
#define PS2_RES_LEFT(r) (r&(1<<18))
#define PS2_RES_MIDDLE(r) (r&(1<<17))
#define PS2_RES_RIGHT(r) (r&(1<<16))
#define PS2_RES_RESOLUTION(r) ((r>>8)&0x03)
#define PS2_RES_SAMPLE_RATE(r) (r&0xff)



/*****************************************************************************
 *	PS/2 Utility functions.
 *     Many parts adapted from tpconfig.c by C. Scott Ananian
 ****************************************************************************/

/* 
 * Read a byte from the ps/2 port
 */
static Bool 
ps2_getbyte(int fd, byte *b) 
{
	if(xf86WaitForInput(fd, 50000) > 0) {
		if(xf86ReadSerial(fd, b, 1) != 1) {
#ifdef DEBUG
			ErrorF("ps2_getbyte: No byte read\n");
#endif
			return !Success;
		}
#ifdef DEBUG
		ErrorF("ps2_getbyte: byte %02X read\n", *b);
#endif
		return Success;
	}
#ifdef DEBUG
	ErrorF("ps2_getbyte: timeout xf86WaitForInput\n");
#endif
	return !Success;		
}

/*
 * Write a byte to the ps/2 port, wair for ACK 
 */
static Bool 
ps2_putbyte(int fd, byte b) 
{
	byte ack;

	if(xf86WriteSerial(fd, &b, 1) != 1) {
#ifdef DEBUG
		ErrorF("ps2_putbyte: error xf86WriteSerial\n");
#endif
		return !Success;
	}
#ifdef DEBUG
	ErrorF("ps2_putbyte: byte %02X send\n", b);
#endif
	/* wait for an ACK */
	if(ps2_getbyte(fd, &ack) != Success) {
		return !Success;
	}
	if(ack != PS2_ACK) {
#ifdef DEBUG
		ErrorF("ps2_putbyte: wrong acknowledge 0x%02x\n", ack);
#endif
		return !Success;
	}
	return Success;
}

/*
 * Use the Synaptics extended ps/2 syntax to write a special command byte. Needed by 
 * ps2_send_cmd and ps2_set_mode. 
 * special command: 0xE8 rr 0xE8 ss 0xE8 tt 0xE8 uu where (rr*64)+(ss*16)+(tt*4)+uu
 *                  is the command. A 0xF3 or 0xE9 must follow (see ps2_send_cmd, ps2_set_mode)
 */
static Bool 
ps2_special_cmd(int fd, byte cmd) 
{
	int i;

	/* initialize with 'inert' command */
	if(ps2_putbyte(fd, PS2_CMD_SET_SCALING_1_1) == Success)
		/* send 4x 2-bits with set resolution command */
		for (i=0; i<4; i++) {
			if(((ps2_putbyte(fd, PS2_CMD_SET_RESOLUTION)) != Success) ||
			   ((ps2_putbyte(fd, (cmd>>6)&0x3) != Success)))
				return !Success;
			cmd<<=2;
		}
	else
		return !Success;
	return Success;
}

/*
 * Send a command to the synpatics touchpad by special commands
 */ 
static Bool
ps2_send_cmd(int fd, byte c)
{
#ifdef DEBUG
	ErrorF("send command: 0x%02X\n", c);
#endif
	return(ps2_special_cmd(fd, c) || ps2_putbyte(fd, PS2_CMD_STATUS_REQUEST));
}

/*****************************************************************************
 *	Synaptics communications functions
 ****************************************************************************/

/*
 * Set the synaptics touchpad mode byte by special commands
 */
Bool 
synaptics_set_mode(int fd, byte mode) 
{
#ifdef DEBUG
	ErrorF("set mode byte to: 0x%02X\n", mode);
#endif
	return(ps2_special_cmd(fd, mode) ||
	       ps2_putbyte(fd, PS2_CMD_SET_SAMPLE_RATE) ||
	       ps2_putbyte(fd, 0x14));
}
 
/* 
 * reset the touchpad 
 */
Bool
synaptics_reset(int fd)
{
	byte r[2];

	xf86FlushInput(fd);
#ifdef DEBUG
	ErrorF("Reset the Touchpad...\n");
#endif
	if(ps2_putbyte(fd, PS2_CMD_RESET) != Success) {
#ifdef DEBUG
		ErrorF("...failed\n");
#endif
		return !Success; 
	}
	xf86WaitForInput(fd, 1500000);
	if((ps2_getbyte(fd, &r[0]) == Success) && 
	   (ps2_getbyte(fd, &r[1]) == Success)) {
		if(r[0] == 0xAA && r[1] == 0x00) {
#ifdef DEBUG
			ErrorF("...done\n");
#endif
			return Success;
		} else {
#ifdef DEBUG
			ErrorF("...failed. Wrong reset ack 0x%02x, 0x%02x\n", r[0], r[1]);
#endif
			return !Success;
		}
	}
#ifdef DEBUG
		ErrorF("...failed\n");
#endif
	return !Success;
}

/* 
 * Read the model-id bytes from the touchpad
 * see also SYN_MODEL_* macros
 */
Bool
synaptics_model_id(int fd, unsigned long int *model_id)
{
	byte mi[3];

#ifdef DEBUG
	ErrorF("Read mode id...\n");
#endif

	if((ps2_send_cmd(fd, SYN_QUE_MODEL) == Success) && 
	   (ps2_getbyte(fd, &mi[0]) == Success) &&
	   (ps2_getbyte(fd, &mi[1]) == Success) &&
   	   (ps2_getbyte(fd, &mi[2]) == Success)) {
		*model_id = (mi[0]<<16) | (mi[1]<<8) | mi[2]; 
#ifdef DEBUG
		ErrorF("mode-id %06X\n", *model_id);
#endif
#ifdef DEBUG
		ErrorF("...done.\n");
#endif
		return Success;
	} 
#ifdef DEBUG
		ErrorF("...failed.\n");
#endif
	return !Success;
}

/*
 * Read the capability-bits from the touchpad
 * see also the SYN_CAP_* macros
 */
Bool
synaptics_capability(int fd, unsigned long int *capability)
{
	byte cap[3];

#ifdef DEBUG
	ErrorF("Read capabilites...\n");
#endif

	if((ps2_send_cmd(fd, SYN_QUE_CAPABILITIES) == Success) &&
	   (ps2_getbyte(fd, &cap[0]) == Success) &&
	   (ps2_getbyte(fd, &cap[1]) == Success) &&
	   (ps2_getbyte(fd, &cap[2]) == Success)) { 
		*capability = (cap[0]<<16) | (cap[1]<<8) | cap[2];
#ifdef DEBUG
		ErrorF("capability %06X\n", *capability);
#endif
		if(SYN_CAP_VALID(*capability)) {
#ifdef DEBUG
			ErrorF("...done.\n");
#endif
			return Success;
		}
	}
#ifdef DEBUG
	ErrorF("...failed.\n");
#endif
	return !Success;
}

/*
 * Identify Touchpad 
 * See also the SYN_ID_* macros
 */
Bool
synaptics_identify(int fd, unsigned long int *ident)
{
	byte id[3];

#ifdef DEBUG
	ErrorF("Identify Touchpad...\n");
#endif

	if((ps2_send_cmd(fd, SYN_QUE_IDENTIFY) == Success) &&
	   (ps2_getbyte(fd, &id[0]) == Success) &&
	   (ps2_getbyte(fd, &id[1]) == Success) &&
	   (ps2_getbyte(fd, &id[2]) == Success)) { 
		*ident = (id[0]<<16) | (id[1]<<8) | id[2];
#ifdef DEBUG
		ErrorF("ident %06X\n", *ident);
#endif
		if(SYN_ID_IS_SYNAPTICS(*ident)) {
#ifdef DEBUG
			ErrorF("...done.\n");
#endif
			return Success;
		}
	}
#ifdef DEBUG
	ErrorF("...failed.\n");
#endif
	return !Success;
}

Bool
SynapticsEnableDevice(int fd)
{
	return(ps2_putbyte(fd, PS2_CMD_ENABLE));
}

Bool
QueryIsSynaptics(int fd)
{
	unsigned long id;

	xf86FlushInput(fd);
	if(synaptics_identify(fd, &id) == Success) {
		return TRUE;
	} else {
		ErrorF("Query no Synaptics: %06X\n", id);
		return FALSE;
	}
}

/* 
 * vim: ts=4 sw=4 cindent
 */

