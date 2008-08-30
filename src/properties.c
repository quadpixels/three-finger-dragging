/*
 * Copyright © 2008 Red Hat, Inc.
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
 * Authors: Peter Hutterer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86Module.h"
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 3

#include <X11/Xatom.h>
#include <xf86Xinput.h>
#include <exevents.h>

#include "synaptics.h"
#include "synapticsstr.h"
#include "synaptics-properties.h"

Atom prop_edges                 = 0;
Atom prop_finger                = 0;
Atom prop_tap_time              = 0;
Atom prop_tap_move              = 0;
Atom prop_tap_durations         = 0;
Atom prop_tap_fast              = 0;
Atom prop_middle_timeout        = 0;
Atom prop_twofinger_pressure    = 0;
Atom prop_scrolldist            = 0;
Atom prop_scrolledge            = 0;
Atom prop_scrolltwofinger       = 0;
Atom prop_speed                 = 0;
Atom prop_edgemotion_pressure   = 0;
Atom prop_edgemotion_speed      = 0;
Atom prop_edgemotion_always     = 0;
Atom prop_buttonscroll          = 0;
Atom prop_buttonscroll_repeat   = 0;
Atom prop_buttonscroll_time     = 0;
Atom prop_off                   = 0;
Atom prop_guestmouse            = 0;
Atom prop_lockdrags             = 0;
Atom prop_lockdrags_time        = 0;
Atom prop_tapaction             = 0;
Atom prop_clickaction           = 0;
Atom prop_circscroll            = 0;
Atom prop_circscroll_dist       = 0;
Atom prop_circscroll_trigger    = 0;
Atom prop_circpad               = 0;
Atom prop_palm                  = 0;
Atom prop_palm_dim              = 0;
Atom prop_coastspeed            = 0;
Atom prop_pressuremotion        = 0;
Atom prop_grab                  = 0;

static Atom
InitAtom(DeviceIntPtr dev, char *name, int format, int nvalues, int *values)
{
    int i;
    Atom atom;
    uint8_t val_8[9]; /* we never have more than 9 values in an atom */
    uint16_t val_16[9];
    uint32_t val_32[9];
    pointer converted;


    for (i = 0; i < nvalues; i++)
    {
        switch(format)
        {
            case 8:  val_8[i]  = values[i]; break;
            case 16: val_16[i] = values[i]; break;
            case 32: val_32[i] = values[i]; break;
        }
    }

    switch(format)
    {
        case 8: converted = val_8; break;
        case 16: converted = val_16; break;
        case 32: converted = val_32; break;
    }

    atom = MakeAtom(name, strlen(name), TRUE);
    XIChangeDeviceProperty(dev, atom, XA_INTEGER, format,
                           PropModeReplace, nvalues,
                           converted, FALSE, FALSE, FALSE);
    return atom;
}

/**
 * Configure the property if the atom is valid.
 */
static void
ConfigureProperty(DeviceIntPtr dev, Atom atom, int nvalues, long* values)
{
    if (atom)
        XIConfigureDeviceProperty(dev, atom, FALSE, FALSE, FALSE, nvalues, values);
}

void
InitDeviceProperties(LocalDevicePtr local)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) local->private;
    SynapticsSHM *para = priv->synpara;

    int i;
    int values[9]; /* we never have more than 9 values in an atom */
    long valid_values[9];

    values[0] = para->left_edge;
    values[1] = para->right_edge;
    values[2] = para->top_edge;
    values[3] = para->bottom_edge;

    prop_edges = InitAtom(local->dev, SYNAPTICS_PROP_EDGES, 32, 4, values);

    values[0] = para->finger_low;
    values[1] = para->finger_high;
    values[2] = para->finger_press;

    prop_finger = InitAtom(local->dev, SYNAPTICS_PROP_FINGER, 32, 3, values);
    prop_tap_time = InitAtom(local->dev, SYNAPTICS_PROP_TAP_TIME, 32, 1, &para->tap_time);
    prop_tap_move = InitAtom(local->dev, SYNAPTICS_PROP_TAP_MOVE, 32, 1, &para->tap_move);

    values[0] = para->single_tap_timeout;
    values[1] = para->tap_time_2;
    values[2] = para->click_time;

    prop_tap_durations = InitAtom(local->dev, SYNAPTICS_PROP_TAP_DURATIONS, 32, 3, values);
    prop_tap_fast = InitAtom(local->dev, SYNAPTICS_PROP_TAP_FAST, 8, 1, &para->fast_taps);
    prop_middle_timeout = InitAtom(local->dev, SYNAPTICS_PROP_MIDDLE_TIMEOUT,
                                   32, 1, &para->emulate_mid_button_time);
    prop_twofinger_pressure = InitAtom(local->dev, SYNAPTICS_PROP_TWOFINGER_PRESSURE,
                                       32, 1, &para->emulate_twofinger_z);

    values[0] = para->scroll_dist_vert;
    values[1] = para->scroll_dist_horiz;
    prop_scrolldist = InitAtom(local->dev, SYNAPTICS_PROP_SCROLL_DISTANCE, 32, 2, values);

    values[0] = para->scroll_edge_vert;
    values[1] = para->scroll_edge_horiz;
    values[2] = para->scroll_edge_corner;
    prop_scrolledge = InitAtom(local->dev, SYNAPTICS_PROP_SCROLL_EDGE,8, 3, values);
    values[0] = para->scroll_twofinger_vert;
    values[1] = para->scroll_twofinger_horiz;
    prop_scrolltwofinger = InitAtom(local->dev, SYNAPTICS_PROP_SCROLL_TWOFINGER,8, 2, values);

    /* FIXME: MISSING: speed is a float */

    values[0] = para->edge_motion_min_z;
    values[1] = para->edge_motion_max_z;
    prop_edgemotion_pressure = InitAtom(local->dev, SYNAPTICS_PROP_EDGEMOTION_PRESSURE, 32, 2, values);

    values[0] = para->edge_motion_min_speed;
    values[1] = para->edge_motion_max_speed;
    prop_edgemotion_speed = InitAtom(local->dev, SYNAPTICS_PROP_EDGEMOTION_SPEED, 32, 2, values);
    prop_edgemotion_always = InitAtom(local->dev, SYNAPTICS_PROP_EDGEMOTION, 8, 1, &para->edge_motion_use_always);

    values[0] = para->updown_button_scrolling;
    values[1] = para->leftright_button_scrolling;
    prop_buttonscroll = InitAtom(local->dev, SYNAPTICS_PROP_BUTTONSCROLLING, 8, 2, values);

    values[0] = para->updown_button_repeat;
    values[1] = para->leftright_button_repeat;
    prop_buttonscroll_repeat = InitAtom(local->dev, SYNAPTICS_PROP_BUTTONSCROLLING_REPEAT, 8, 2, values);
    prop_buttonscroll_time = InitAtom(local->dev, SYNAPTICS_PROP_BUTTONSCROLLING_TIME, 32, 1, &para->scroll_button_repeat);
    prop_off = InitAtom(local->dev, SYNAPTICS_PROP_OFF, 8, 1, &para->touchpad_off);
    prop_guestmouse = InitAtom(local->dev, SYNAPTICS_PROP_GUESTMOUSE, 8, 1, &para->guestmouse_off);
    prop_lockdrags = InitAtom(local->dev, SYNAPTICS_PROP_LOCKED_DRAGS, 8, 1, &para->locked_drags);
    prop_lockdrags_time = InitAtom(local->dev, SYNAPTICS_PROP_LOCKED_DRAGS_TIMEOUT, 32, 1, &para->locked_drag_time);

    memcpy(values, para->tap_action, MAX_TAP * sizeof(int));
    prop_tapaction = InitAtom(local->dev, SYNAPTICS_PROP_TAP_ACTION, 8, MAX_TAP, values);

    memcpy(values, para->click_action, MAX_CLICK * sizeof(int));
    prop_clickaction = InitAtom(local->dev, SYNAPTICS_PROP_CLICK_ACTION, 8, MAX_CLICK, values);

    prop_circscroll = InitAtom(local->dev, SYNAPTICS_PROP_CIRCULAR_SCROLLING, 8, 1, &para->circular_scrolling);
    /* FIXME: missing: scroll_dist_circ is a float */
    prop_circscroll_trigger = InitAtom(local->dev, SYNAPTICS_PROP_CIRCULAR_SCROLLING_TRIGGER, 8, 1, &para->circular_trigger);
    prop_circpad = InitAtom(local->dev, SYNAPTICS_PROP_CIRCULAR_PAD, 8, 1, &para->circular_pad);
    prop_palm = InitAtom(local->dev, SYNAPTICS_PROP_PALM_DETECT, 8, 1, &para->palm_detect);

    values[0] = para->palm_min_width;
    values[1] = para->palm_min_z;

    prop_palm_dim = InitAtom(local->dev, SYNAPTICS_PROP_PALM_DIMENSIONS, 32, 2, values);

    /* FIXME: missing, coastspeed is a float */

    values[0] = para->press_motion_min_z;
    values[1] = para->press_motion_max_z;
    prop_pressuremotion = InitAtom(local->dev, SYNAPTICS_PROP_PRESSURE_MOTION, 32, 2, values);

    /* FIXME: missing, motion_min/max is a float */

    prop_grab = InitAtom(local->dev, SYNAPTICS_PROP_GRAB, 8, 1, &para->grab_event_device);

    /* Configure value ranges as appropriate */

    /* boolean properties */
    valid_values[0] = TRUE;
    valid_values[0] = FALSE;
    ConfigureProperty(local->dev, prop_tap_fast, 2, valid_values);
    ConfigureProperty(local->dev, prop_scrolledge, 2, valid_values);
    ConfigureProperty(local->dev, prop_scrolltwofinger, 2, valid_values);
    ConfigureProperty(local->dev, prop_edgemotion_always, 2, valid_values);
    ConfigureProperty(local->dev, prop_buttonscroll, 2, valid_values);
    ConfigureProperty(local->dev, prop_buttonscroll_repeat, 2, valid_values);
    ConfigureProperty(local->dev, prop_guestmouse, 2, valid_values);
    ConfigureProperty(local->dev, prop_lockdrags, 2, valid_values);
    ConfigureProperty(local->dev, prop_circscroll, 2, valid_values);
    ConfigureProperty(local->dev, prop_circpad, 2, valid_values);
    ConfigureProperty(local->dev, prop_palm, 2, valid_values);
    ConfigureProperty(local->dev, prop_grab, 2, valid_values);
    /* end of boolean props */

    valid_values[0] = 0;
    valid_values[1] = 1;
    valid_values[2] = 2;
    ConfigureProperty(local->dev, prop_off, 3, valid_values);

    for (i = 0; i <= 8; i++)
        valid_values[i] = i;
    ConfigureProperty(local->dev, prop_circscroll_trigger, 8, valid_values);
}

Bool
SetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop)
{
    LocalDevicePtr local = (LocalDevicePtr) dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) local->private;
    SynapticsSHM *para = priv->synpara;

    if (property == prop_edges)
    {
        INT32 *edges;
        if (prop->size != 4 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        edges = (INT32*)prop->data;
        if (edges[0] > edges[1] || edges[2] > edges[3])
            return FALSE;

        para->left_edge   = edges[0];
        para->right_edge  = edges[1];
        para->top_edge    = edges[2];
        para->bottom_edge = edges[3];

    } else if (property == prop_finger)
    {
        INT32 *finger;
        if (prop->size != 3 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        finger = (INT32*)prop->data;
        if (finger[0] > finger[1])
            return FALSE;

        para->finger_low   = finger[0];
        para->finger_high  = finger[1];
        para->finger_press = finger[2];

    } else if (property == prop_tap_time)
    {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        para->tap_time = *(INT32*)prop->data;

    } else if (property == prop_tap_move)
    {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        para->tap_move = *(INT32*)prop->data;
    } else if (property == prop_tap_durations)
    {
        INT32 *timeouts;

        if (prop->size != 3 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        timeouts = (INT32*)prop->data;

        para->single_tap_timeout = timeouts[0];
        para->tap_time_2         = timeouts[1];
        para->click_time         = timeouts[2];

    } else if (property == prop_tap_fast)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->fast_taps = *(BOOL*)prop->data;

    } else if (property == prop_middle_timeout)
    {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        para->emulate_mid_button_time = *(INT32*)prop->data;
    } else if (property == prop_twofinger_pressure)
    {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        para->emulate_twofinger_z = *(INT32*)prop->data;
    } else if (property == prop_scrolldist)
    {
        INT32 *dist;
        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        dist = (INT32*)prop->data;
        para->scroll_dist_vert = dist[0];
        para->scroll_dist_horiz = dist[1];
    } else if (property == prop_scrolledge)
    {
        CARD8 *edge;
        if (prop->size != 3 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        edge = (BOOL*)prop->data;
        para->scroll_edge_vert   = edge[0];
        para->scroll_edge_horiz  = edge[1];
        para->scroll_edge_corner = edge[2];
    } else if (property == prop_scrolltwofinger)
    {
        CARD8 *twofinger;

        if (prop->size != 2 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        twofinger = (BOOL*)prop->data;
        para->scroll_twofinger_vert  = twofinger[0];
        para->scroll_twofinger_horiz = twofinger[1];
    } else if (property == prop_speed)
    {
        /* XXX */
    } else if (property == prop_edgemotion_pressure)
    {
        CARD32 *pressure;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        pressure = (CARD32*)prop->data;
        if (pressure[0] > pressure[1])
            return FALSE;

        para->edge_motion_min_z = pressure[0];
        para->edge_motion_max_z = pressure[1];

    } else if (property == prop_edgemotion_speed)
    {
        CARD32 *speed;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        speed = (CARD32*)prop->data;
        if (speed[0] > speed[1])
            return FALSE;

        para->edge_motion_min_speed = speed[0];
        para->edge_motion_max_speed = speed[1];

    } else if (property == prop_edgemotion_always)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->edge_motion_use_always = *(BOOL*)prop->data;

    } else if (property == prop_buttonscroll)
    {
        BOOL *scroll;

        if (prop->size != 2 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        scroll = (BOOL*)prop->data;
        para->updown_button_scrolling    = scroll[0];
        para->leftright_button_scrolling = scroll[1];

    } else if (property == prop_buttonscroll_repeat)
    {
        BOOL *repeat;

        if (prop->size != 2 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        repeat = (BOOL*)prop->data;
        para->updown_button_repeat    = repeat[0];
        para->leftright_button_repeat = repeat[1];
    } else if (property == prop_buttonscroll_time)
    {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        para->scroll_button_repeat = *(INT32*)prop->data;

    } else if (property == prop_off)
    {
        CARD8 off;
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        off = *(CARD8*)prop->data;

        if (off > 2)
            return FALSE;

        para->touchpad_off = off;
    } else if (property == prop_guestmouse)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->guestmouse_off = *(BOOL*)prop->data;
    } else if (property == prop_lockdrags)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->locked_drags = *(BOOL*)prop->data;
    } else if (property == prop_lockdrags_time)
    {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        para->locked_drag_time = *(INT32*)prop->data;
    } else if (property == prop_tapaction)
    {
        int i;
        CARD8 *action;

        if (prop->size > MAX_TAP || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        action = (CARD8*)prop->data;

        for (i = 0; i < MAX_TAP; i++)
            para->tap_action[i] = action[i];
    } else if (property == prop_clickaction)
    {
        int i;
        CARD8 *action;

        if (prop->size > MAX_CLICK || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        action = (CARD8*)prop->data;

        for (i = 0; i < MAX_CLICK; i++)
            para->click_action[i] = action[i];
    } else if (property == prop_circscroll)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->circular_scrolling = *(BOOL*)prop->data;

    } else if (property == prop_circscroll_dist)
    {
        /* FIXME */
    } else if (property == prop_circscroll_trigger)
    {
        int trigger;
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        trigger = *(CARD8*)prop->data;
        if (trigger > 8)
            return FALSE;

        para->circular_trigger = trigger;

    } else if (property == prop_circpad)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->circular_pad = *(BOOL*)prop->data;
    } else if (property == prop_palm)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->palm_detect = *(BOOL*)prop->data;
    } else if (property == prop_palm_dim)
    {
        INT32 *dim;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        dim = (INT32*)prop->data;

        para->palm_min_width = dim[0];
        para->palm_min_z     = dim[1];
    } else if (property == prop_coastspeed)
    {
        /* FIXME */
    } else if (property == prop_pressuremotion)
    {
        INT32 *press;
        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return FALSE;

        press = (INT32*)prop->data;
        if (press[0] > press[1])
            return FALSE;

        para->press_motion_min_z = press[0];
        para->press_motion_max_z = press[1];
    } else if (property == prop_grab)
    {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return FALSE;

        para->grab_event_device = *(BOOL*)prop->data;
    }

    return TRUE;
}

#endif

