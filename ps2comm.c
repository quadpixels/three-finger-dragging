/* Copyright (C) 2001 Stefan Gmeiner <riddlebox@freesurf.ch>
 *
 *   Copyright (c) 1997 C. Scott Ananian <cananian@alumni.priceton.edu>
 *   Copyright (c) 1998-2000 Bruce Kalk <kall@compass.com>
 *     code für the special synaptics commands (from the tpconfig-source)
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
 */

#include "xf86_OSproc.h"

#include "ps2comm.h"
#include "synaptics.h"

/* acknowledge for commands and parameter */
#define PS2_ACK 			0xFA
#define PS2_ERROR			0xFC

/* standard PS/2 commands */
#define PS2_CMD_RESET			0xFF
#define PS2_CMD_RESEND			0xFE
#define PS2_CMD_SET_DEFAULT		0xF6
#define PS2_CMD_DISABLE			0xF5
#define PS2_CMD_ENABLE			0xF4
#define PS2_CMD_SET_SAMPLE_RATE		0xF3
#define PS2_CMD_READ_DEVICE_TYPE	0xF2
#define PS2_CMD_SET_REMOTE_MODE		0xF0
#define PS2_CMD_SET_WRAP_MODE		0xEE
#define PS2_CMD_RESET_WRAP_MODE		0xEC
#define PS2_CMD_READ_DATA		0xEB
#define PS2_CMD_SET_STREAM_MODE		0xEA
#define PS2_CMD_STATUS_REQUEST		0xE9
#define PS2_CMD_SET_RESOLUTION		0xE8
#define PS2_CMD_SET_SCALING_2_1		0xE7
#define PS2_CMD_SET_SCALING_1_1		0xE6

/* synaptics queries */
#define SYN_QUE_IDENTIFY		0x00
#define SYN_QUE_MODES			0x01
#define SYN_QUE_CAPABILITIES		0x02
#define SYN_QUE_MODEL			0x03
#define SYN_QUE_SERIAL_NUMBER_PREFIX	0x06
#define SYN_QUE_SERIAL_NUMBER_SUFFIX	0x07
#define SYN_QUE_RESOLUTION		0x08
#define SYN_QUE_EXT_CAPAB		0x09

/* status request response bits (PS2_CMD_STATUS_REQUEST) */
#define PS2_RES_REMOTE(r)	((r) & (1 << 22))
#define PS2_RES_ENABLE(r)	((r) & (1 << 21))
#define PS2_RES_SCALING(r)	((r) & (1 << 20))
#define PS2_RES_LEFT(r)		((r) & (1 << 18))
#define PS2_RES_MIDDLE(r)	((r) & (1 << 17))
#define PS2_RES_RIGHT(r)	((r) & (1 << 16))
#define PS2_RES_RESOLUTION(r)	(((r) >> 8) & 0x03)
#define PS2_RES_SAMPLE_RATE(r)	((r) & 0xff)

/* #define DEBUG */

#ifdef DEBUG
#define DBG(x) (x)
#else
#define DBG(x)
#endif

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
    if (xf86WaitForInput(fd, 50000) > 0) {
	if (xf86ReadSerial(fd, b, 1) != 1) {
	    DBG(ErrorF("ps2_getbyte: No byte read\n"));
	    return !Success;
	}
	DBG(ErrorF("ps2_getbyte: byte %02X read\n", *b));
	return Success;
    }
    DBG(ErrorF("ps2_getbyte: timeout xf86WaitForInput\n"));
    return !Success;
}

/*
 * Write a byte to the ps/2 port, wait for ACK
 */
static Bool
ps2_putbyte(int fd, byte b)
{
    byte ack;

    if (xf86WriteSerial(fd, &b, 1) != 1) {
	DBG(ErrorF("ps2_putbyte: error xf86WriteSerial\n"));
	return !Success;
    }
    DBG(ErrorF("ps2_putbyte: byte %02X send\n", b));
    /* wait for an ACK */
    if (ps2_getbyte(fd, &ack) != Success) {
	return !Success;
    }
    if (ack != PS2_ACK) {
	DBG(ErrorF("ps2_putbyte: wrong acknowledge 0x%02x\n", ack));
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
    if (ps2_putbyte(fd, PS2_CMD_SET_SCALING_1_1) == Success)
	/* send 4x 2-bits with set resolution command */
	for (i = 0; i < 4; i++) {
	    if (((ps2_putbyte(fd, PS2_CMD_SET_RESOLUTION)) != Success) ||
		((ps2_putbyte(fd, (cmd >> 6) & 0x3) != Success)))
		return !Success;
	    cmd <<= 2;
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
    DBG(ErrorF("send command: 0x%02X\n", c));
    return ps2_special_cmd(fd, c) || ps2_putbyte(fd, PS2_CMD_STATUS_REQUEST);
}

/*****************************************************************************
 *	Synaptics passthrough functions
 ****************************************************************************/

static Bool
ps2_getbyte_passthrough(int fd, byte *response)
{
    byte ack;
    int timeout_count;
#define MAX_RETRY_COUNT 30

    /* Getting a response back through the passthrough could take some time.
     * Spin a little for the first byte */
    for (timeout_count = 0;
	 (ps2_getbyte(fd, &ack) != Success) && timeout_count <= MAX_RETRY_COUNT;
	 timeout_count++)
	;
    /* Do some sanity checking */
    if ((ack & 0xfc) != 0x84) {
	DBG(ErrorF("ps2_getbyte_passthrough: expected 0x84 and got: %02x\n",
		   ack & 0xfc));
	return !Success;
    }

    ps2_getbyte(fd, response);
    ps2_getbyte(fd, &ack);
    ps2_getbyte(fd, &ack);
    if ((ack & 0xcc) != 0xc4) {
	DBG(ErrorF("ps2_getbyte_passthrough: expected 0xc4 and got: %02x\n",
		   ack & 0xcc));
	return !Success;
    }
    ps2_getbyte(fd, &ack);
    ps2_getbyte(fd, &ack);

    return Success;
}

static Bool
ps2_putbyte_passthrough(int fd, byte c)
{
    byte ack;

    ps2_special_cmd(fd, c);
    ps2_putbyte(fd, 0xF3);
    ps2_putbyte(fd, 0x28);

    ps2_getbyte_passthrough(fd, &ack);
    if (ack != PS2_ACK) {
	DBG(ErrorF("ps2_putbyte_passthrough: wrong acknowledge 0x%02x\n", ack));
	return !Success;
    }
    return Success;
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
    DBG(ErrorF("set mode byte to: 0x%02X\n", mode));
    return (ps2_special_cmd(fd, mode) ||
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
    DBG(ErrorF("Reset the Touchpad...\n"));
    if (ps2_putbyte(fd, PS2_CMD_RESET) != Success) {
	DBG(ErrorF("...failed\n"));
	return !Success;
    }
    xf86WaitForInput(fd, 4000000);
    if ((ps2_getbyte(fd, &r[0]) == Success) &&
	(ps2_getbyte(fd, &r[1]) == Success)) {
	if (r[0] == 0xAA && r[1] == 0x00) {
	    DBG(ErrorF("...done\n"));
	    return Success;
	} else {
	    DBG(ErrorF("...failed. Wrong reset ack 0x%02x, 0x%02x\n", r[0], r[1]));
	    return !Success;
	}
    }
    DBG(ErrorF("...failed\n"));
    return !Success;
}

Bool
SynapticsResetPassthrough(int fd)
{
    byte ack;

    /* send reset */
    ps2_putbyte_passthrough(fd, 0xff);
    ps2_getbyte_passthrough(fd, &ack);
    if (ack != 0xaa) {
	DBG(ErrorF("SynapticsResetPassthrough: ack was %02x not 0xaa\n", ack));
	return !Success;
    }
    ps2_getbyte_passthrough(fd, &ack);
    if (ack != 0x00) {
	DBG(ErrorF("SynapticsResetPassthrough: ack was %02x not 0x00\n", ack));
	return !Success;
    }

    /* set defaults, turn on streaming, and enable the mouse */
    return (ps2_putbyte_passthrough(fd, 0xf6) ||
	    ps2_putbyte_passthrough(fd, 0xea) ||
	    ps2_putbyte_passthrough(fd, 0xf4));
}

/*
 * Read the model-id bytes from the touchpad
 * see also SYN_MODEL_* macros
 */
Bool
synaptics_model_id(int fd, unsigned long int *model_id)
{
    byte mi[3];

    DBG(ErrorF("Read mode id...\n"));

    if ((ps2_send_cmd(fd, SYN_QUE_MODEL) == Success) &&
	(ps2_getbyte(fd, &mi[0]) == Success) &&
	(ps2_getbyte(fd, &mi[1]) == Success) &&
	(ps2_getbyte(fd, &mi[2]) == Success)) {
	*model_id = (mi[0] << 16) | (mi[1] << 8) | mi[2];
	DBG(ErrorF("mode-id %06X\n", *model_id));
	DBG(ErrorF("...done.\n"));
	return Success;
    }
    DBG(ErrorF("...failed.\n"));
    return !Success;
}

/*
 * Read the capability-bits from the touchpad
 * see also the SYN_CAP_* macros
 */
Bool
synaptics_capability(int fd, struct synapticshw *synhw)
{
    byte cap[3];

    DBG(ErrorF("Read capabilites...\n"));

    synhw->ext_cap = 0;
    if ((ps2_send_cmd(fd, SYN_QUE_CAPABILITIES) == Success) &&
	(ps2_getbyte(fd, &cap[0]) == Success) &&
	(ps2_getbyte(fd, &cap[1]) == Success) &&
	(ps2_getbyte(fd, &cap[2]) == Success)) {
	synhw->capabilities = (cap[0] << 16) | (cap[1] << 8) | cap[2];
	DBG(ErrorF("capabilities %06X\n", synhw->capabilities));
	if (SYN_CAP_VALID(*synhw)) {
	    if (SYN_EXT_CAP_REQUESTS(*synhw)) {
		if ((ps2_send_cmd(fd, SYN_QUE_EXT_CAPAB) == Success) &&
		    (ps2_getbyte(fd, &cap[0]) == Success) &&
		    (ps2_getbyte(fd, &cap[1]) == Success) &&
		    (ps2_getbyte(fd, &cap[2]) == Success)) {
		    synhw->ext_cap = (cap[0] << 16) | (cap[1] << 8) | cap[2];
		    DBG(ErrorF("ext-capability %06X\n", synhw->ext_cap));
		} else {
		    DBG(ErrorF("synaptics says, that it has extended-capabilities, "
			       "but I cannot read them."));
		}
	    }
	    DBG(ErrorF("...done.\n"));
	    return Success;
	}
    }
    DBG(ErrorF("...failed.\n"));
    return !Success;
}

/*
 * Identify Touchpad
 * See also the SYN_ID_* macros
 */
Bool
synaptics_identify(int fd, struct synapticshw *synhw)
{
    byte id[3];

    DBG(ErrorF("Identify Touchpad...\n"));

    synhw->identity = 0;
    if ((ps2_send_cmd(fd, SYN_QUE_IDENTIFY) == Success) &&
	(ps2_getbyte(fd, &id[0]) == Success) &&
	(ps2_getbyte(fd, &id[1]) == Success) &&
	(ps2_getbyte(fd, &id[2]) == Success)) {
	synhw->identity = (id[0] << 16) | (id[1] << 8) | id[2];
	DBG(ErrorF("ident %06X\n", synhw->identity));
	if (SYN_ID_IS_SYNAPTICS(*synhw)) {
	    DBG(ErrorF("...done.\n"));
	    return Success;
	}
    }
    DBG(ErrorF("...failed.\n"));
    return !Success;
}

/*
 * read mode byte
 */
Bool
synaptics_read_mode(int fd, unsigned char *mode)
{
    byte modes[3];

    DBG(ErrorF("Read mode byte...\n"));

    if ((ps2_send_cmd(fd, SYN_QUE_MODES) == Success) &&
	(ps2_getbyte(fd, &modes[0]) == Success) &&
	(ps2_getbyte(fd, &modes[1]) == Success) &&
	(ps2_getbyte(fd, &modes[2]) == Success)) {

	*mode = modes[2];
	DBG(ErrorF("modes byte %02X%02X%02X\n", modes[0], modes[1], modes[2]));
	if ((modes[0] == 0x3B) && (modes[1] == 0x47)) {
	    DBG(ErrorF("...done.\n"));
	    return Success;
	}
    }
    DBG(ErrorF("...failed.\n"));
    return !Success;
}

Bool
SynapticsEnableDevice(int fd)
{
    return ps2_putbyte(fd, PS2_CMD_ENABLE);
}

Bool
SynapticsDisableDevice(int fd)
{
    xf86FlushInput(fd);
    return ps2_putbyte(fd, PS2_CMD_DISABLE);
}

Bool
QueryIsSynaptics(int fd)
{
    struct synapticshw synhw;
    int i;

    for (i = 0; i < 3; i++) {
	if (SynapticsDisableDevice(fd) == Success)
	    break;
    }

    xf86WaitForInput(fd, 20000);
    xf86FlushInput(fd);
    if (synaptics_identify(fd, &synhw) == Success) {
	return TRUE;
    } else {
	ErrorF("Query no Synaptics: %06X\n", synhw.identity);
	return FALSE;
    }
}
