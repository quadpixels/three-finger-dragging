/*
 *   Copyright 2004 Peter Osterlund <petero2@telia.com>
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
#ifndef _SYNPROTO_H_
#define _SYNPROTO_H_

#include <unistd.h>
#include <sys/ioctl.h>
#include <xf86Xinput.h>
#include <xisb.h>

/*
 * A structure to describe the state of the touchpad hardware (buttons and pad)
 */
struct SynapticsHwState {
    int millis;			/* Timestamp in milliseconds */
    int x;			/* X position of finger */
    int y;			/* Y position of finger */
    int z;			/* Finger pressure */
    int numFingers;
    int fingerWidth;

    Bool left;
    Bool right;
    Bool up;
    Bool down;

    Bool multi[8];
    Bool middle;		/* Some ALPS touchpads have a middle button */

    Bool guest_left;		/* guest device */
    Bool guest_mid;
    Bool guest_right;
    int  guest_dx;
    int  guest_dy;
};

struct CommData {
    XISBuffer *buffer;
    unsigned char protoBuf[6];		/* Buffer for Packet */
    unsigned char lastByte;		/* Last read byte. Use for reset sequence detection. */
    int outOfSync;			/* How many consecutive incorrect packets we
					   have received */
    int protoBufTail;

    /* Used for keeping track of partial HwState updates. */
    struct SynapticsHwState hwState;
    Bool oneFinger;
    Bool twoFingers;
    Bool threeFingers;
};

enum SynapticsProtocol {
    SYN_PROTO_PSAUX,		/* Raw psaux device */
    SYN_PROTO_EVENT,		/* Linux kernel event interface */
    SYN_PROTO_PSM,		/* FreeBSD psm driver */
    SYN_PROTO_ALPS		/* ALPS touchpad protocol */
};

struct SynapticsHwInfo;
struct CommData;

struct SynapticsProtocolOperations {
    void (*DeviceOnHook)(LocalDevicePtr local);
    void (*DeviceOffHook)(LocalDevicePtr local);
    Bool (*QueryHardware)(LocalDevicePtr local, struct SynapticsHwInfo *synhw);
    Bool (*ReadHwState)(LocalDevicePtr local, struct SynapticsHwInfo *synhw,
			struct SynapticsProtocolOperations *proto_ops,
			struct CommData *comm, struct SynapticsHwState *hwRet);
    Bool (*AutoDevProbe)(LocalDevicePtr local);
};

extern struct SynapticsProtocolOperations psaux_proto_operations;
extern struct SynapticsProtocolOperations event_proto_operations;
extern struct SynapticsProtocolOperations psm_proto_operations;
extern struct SynapticsProtocolOperations alps_proto_operations;


#endif /* _SYNPROTO_H_ */
