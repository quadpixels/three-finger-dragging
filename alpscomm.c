/* Copyright (C) 2001 Stefan Gmeiner <riddlebox@freesurf.ch>
 *
 * Copyright (c) 2003 Neil Brown <neilb@cse.unsw.edu.au>
 * Copyright (c) 2003-2004 Peter Osterlund <petero2@telia.com>
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

#include "alpscomm.h"
#include "synproto.h"
#include "synaptics.h"
#include <xf86.h>

/*
 * send the ALPSinit sequence, ie 4 consecutive "disable"s before the "enable"
 */
static void
ALPS_initialize(int fd)
{
    xf86FlushInput(fd);
    ps2_putbyte(fd, PS2_CMD_DISABLE);
    ps2_putbyte(fd, PS2_CMD_DISABLE);
    ps2_putbyte(fd, PS2_CMD_DISABLE);
    ps2_putbyte(fd, PS2_CMD_DISABLE);
    ps2_putbyte(fd, PS2_CMD_ENABLE);
}

static void
ALPSDeviceOnHook(LocalDevicePtr local)
{
}

static void
ALPSDeviceOffHook(LocalDevicePtr local)
{
}

static Bool
ALPSQueryHardware(LocalDevicePtr local, struct SynapticsHwInfo *synhw)
{
    ALPS_initialize(local->fd);
    return TRUE;
}

static Bool
ALPS_packet_ok(struct CommData *comm)
{
    /* ALPS absolute mode packets start with 0b11111mrl */
    if ((comm->protoBuf[0] & 0xf8) == 0xf8)
	return TRUE;
    return FALSE;
}

static Bool
ALPS_get_packet(struct CommData *comm)
{
    int c;

    while ((c = XisbRead(comm->buffer)) >= 0) {
	unsigned char u = (unsigned char)c;

	comm->protoBuf[comm->protoBufTail++] = u;

	/* Check that we have a valid packet. If not, we are out of sync,
	   so we throw away the first byte in the packet.*/
	if (comm->protoBufTail >= 6) {
	    if (!ALPS_packet_ok(comm)) {
		int i;
		for (i = 0; i < comm->protoBufTail - 1; i++)
		    comm->protoBuf[i] = comm->protoBuf[i + 1];
		comm->protoBufTail--;
	    }
	}

	if (comm->protoBufTail >= 6) { /* Full packet received */
	    comm->protoBufTail = 0;
	    return TRUE;
	}
    }

    return FALSE;
}

/*
 * ALPS abolute Mode
 * byte 0: 1 1 1 1 1 mid0 rig0 lef0
 * byte 1: 0 x6 x5 x4 x3 x2 x1 x0
 * byte 2: 0 x10 x9 x8 x7 up1 fin ges
 * byte 3: 0 y9 y8 y7 1 mid1 rig1 lef1
 * byte 4: 0 y6 y5 y4 y3 y2 y1 y0
 * byte 5: 0 z6 z5 z4 z3 z2 z1 z0
 *
 * On a dualpoint, {mid,rig,lef}0 are the stick, 1 are the pad.
 * We just 'or' them together for now.
 *
 * The touchpad on an 'Acer Aspire' has 4 buttons:
 *   left,right,up,down.
 * This device always sets {mid,rig,lef}0 to 1 and
 * reflects left,right,down,up in lef1,rig1,mid1,up1.
 */
static void
ALPS_process_packet(unsigned char *packet, struct SynapticsHwState *hw)
{
    int x, y, z;
    int left = 0, right = 0, middle = 0;

    x = (packet[1] & 0x7f) | ((packet[2] & 0x78) << (7-3));
    y = (packet[4] & 0x7f) | ((packet[3] & 0x70) << (7-4));
    z = packet[5];

    if (z > 0) {
	hw->x = x;
	hw->y = y;
    }
    hw->z = z;
    hw->numFingers = (z > 0) ? 1 : 0;
    hw->fingerWidth = 5;

    left  |= (packet[2]     ) & 1;
    left  |= (packet[3]     ) & 1;
    right |= (packet[3] >> 1) & 1;
    if (packet[0] == 0xff) {
	int back    = (packet[3] >> 2) & 1;
	int forward = (packet[2] >> 2) & 1;
	if (back && forward) {
	    middle = 1;
	    back = 0;
	    forward = 0;
	}
	hw->down = back;
	hw->up = forward;
    } else {
	left   |= (packet[0]     ) & 1;
	right  |= (packet[0] >> 1) & 1;
	middle |= (packet[0] >> 2) & 1;
	middle |= (packet[3] >> 2) & 1;
    }

    hw->left = left;
    hw->right = right;
    hw->middle = middle;
}

static Bool
ALPSReadHwState(LocalDevicePtr local, struct SynapticsHwInfo *synhw,
		struct SynapticsProtocolOperations *proto_ops,
		struct CommData *comm, struct SynapticsHwState *hwRet)
{
    unsigned char *buf = comm->protoBuf;
    struct SynapticsHwState *hw = &(comm->hwState);

    if (!ALPS_get_packet(comm))
	return FALSE;

    memset(hw, 0, sizeof(*hw));

    ALPS_process_packet(buf, hw);

    *hwRet = *hw;
    return TRUE;
}

static Bool
ALPSAutoDevProbe(LocalDevicePtr local)
{
    return FALSE;
}

struct SynapticsProtocolOperations alps_proto_operations = {
    ALPSDeviceOnHook,
    ALPSDeviceOffHook,
    ALPSQueryHardware,
    ALPSReadHwState,
    ALPSAutoDevProbe
};
