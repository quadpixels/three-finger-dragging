/*
 * Copyright © 2011 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *      Peter Hutterer
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <assert.h>

#include "synaptics.h"
#include "synapticsstr.h"
#include "eventcomm.h"

int fd_read, fd_write;

/* A syn event, always handy to have */
struct input_event syn = { {0, 0}, EV_SYN, SYN_REPORT, 0 };

static void
create_pipe_fd(void)
{
    int pipefd[2];

    assert(pipe(pipefd) != -1);

    fd_read = pipefd[0];
    fd_write = pipefd[1];
}

static void
reset_data(struct SynapticsHwState *hw, struct CommData *comm)
{
    memset(comm, 0, sizeof(struct CommData));
    memset(hw, 0, sizeof(struct SynapticsHwState));
}

/**
 * Write n input events to fd, followed by the syn event.
 */
static void
write_event(int fd, struct input_event *ev, int n)
{
    write(fd, ev, sizeof(struct input_event) * n);
    write(fd, &syn, sizeof(syn));
}


static void
test_buttons(int fd,
             InputInfoPtr pInfo,
             struct SynapticsHwState *hw,
             struct CommData *comm)
{
    struct input_event ev = {{0, 0}, EV_KEY, 0, 0};

    reset_data(hw, comm);

#define _test_press_release(_code, field)       \
        ev.code = (_code);                      \
        ev.value = 1;                           \
        write_event(fd, &ev, 1);                \
        EventReadHwState(pInfo, comm, hw);      \
        assert(hw->field == 1);                 \
        ev.value = 0; /* button release */      \
        write_event(fd_write, &ev, 1);          \
        EventReadHwState(pInfo, comm, hw);      \
        assert(hw->field == 0);

    _test_press_release(BTN_LEFT,       left);
    _test_press_release(BTN_RIGHT,      right);
    _test_press_release(BTN_MIDDLE,     middle);
    _test_press_release(BTN_FORWARD,    up);
    _test_press_release(BTN_BACK,       down);
    _test_press_release(BTN_0,          multi[0]);
    _test_press_release(BTN_1,          multi[1]);
    _test_press_release(BTN_2,          multi[2]);
    _test_press_release(BTN_3,          multi[3]);
    _test_press_release(BTN_4,          multi[4]);
    _test_press_release(BTN_5,          multi[5]);
    _test_press_release(BTN_6,          multi[6]);
    _test_press_release(BTN_7,          multi[7]);
}

/**
 * This test checks that the recognised event fields set the right hardware
 * state. It's a fairly limited test and does not check whether any of the
 * others change the HW state at all.
 */
static void
test_read_hw_state(void)
{
    InputInfoRec                info    = {0};
    SynapticsPrivate            private;
    struct SynapticsHwState     hw      = {0};
    struct CommData             comm    = {0};

    struct input_event ev[] = {
	{ {0, 0}, EV_KEY, BTN_TOOL_FINGER, 1 },
	{ {0, 0}, EV_KEY, BTN_TOOL_DOUBLETAP, 1 },
	{ {0, 0}, EV_KEY, BTN_TOOL_TRIPLETAP, 1 },
	{ {0, 0}, EV_ABS, ABS_X, 42 },
	{ {0, 0}, EV_ABS, ABS_Y, 21 },
	{ {0, 0}, EV_ABS, ABS_PRESSURE, 56 },
	{ {0, 0}, EV_ABS, ABS_TOOL_WIDTH, 204 },
    };

    memset(&private, 0, sizeof(private));

    info.private = &private;
    info.fd = fd_read;


    /* just the syn event */
    reset_data(&hw, &comm);
    write(fd_write, &syn, sizeof(syn));
    EventReadHwState(&info, &comm, &hw);
    assert(hw.numFingers == 0);

    /* one finger */
    reset_data(&hw, &comm);
    write_event(fd_write, &ev[0], 1);
    EventReadHwState(&info, &comm, &hw);
    assert(hw.numFingers == 1);

    /* two fingers */
    reset_data(&hw, &comm);
    write_event(fd_write, &ev[1], 1);
    EventReadHwState(&info, &comm, &hw);
    assert(hw.numFingers == 2);

    /* three fingers */
    reset_data(&hw, &comm);
    write_event(fd_write, &ev[2], 1);
    EventReadHwState(&info, &comm, &hw);
    assert(hw.numFingers == 3);

    /* x/y data */
    reset_data(&hw, &comm);
    write_event(fd_write, &ev[3], 2);
    EventReadHwState(&info, &comm, &hw);
    assert(hw.x == ev[3].value);
    assert(hw.y == ev[4].value);

    /* pressure */
    reset_data(&hw, &comm);
    write_event(fd_write, &ev[5], 1);
    EventReadHwState(&info, &comm, &hw);
    assert(hw.z == ev[5].value);

    /* finger width */
    reset_data(&hw, &comm);
    write_event(fd_write, &ev[6], 1);
    EventReadHwState(&info, &comm, &hw);
    assert(hw.fingerWidth == ev[6].value);

    /* the various buttons */
    test_buttons(fd_write, &info, &hw, &comm);
}

int main (int argc, char **argv)
{
    create_pipe_fd();

    test_read_hw_state();
    return 0;
}
