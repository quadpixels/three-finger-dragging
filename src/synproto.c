/*
 * Copyright © 2012 Canonical, Ltd.
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
 *      Chase Douglas <chase.douglas@canonical.com>
 *
 * Trademarks are the property of their respective owners.
 */


#include "synproto.h"
#include "synaptics.h"
#include "synapticsstr.h"

#ifdef HAVE_MULTITOUCH
static int
HwStateAllocTouch(struct SynapticsHwState *hw, SynapticsPrivate *priv)
{
    int num_vals;
    int i = 0;

    hw->num_mt_mask = priv->num_slots;
    hw->mt_mask = malloc(hw->num_mt_mask * sizeof(ValuatorMask *));
    if (!hw->mt_mask)
        goto fail;

    num_vals = 2; /* x and y */
    num_vals += 2; /* scroll axes */
    num_vals += priv->num_mt_axes;

    for (; i < hw->num_mt_mask; i++)
    {
        hw->mt_mask[i] = valuator_mask_new(num_vals);
        if (!hw->mt_mask[i])
            goto fail;
    }

    hw->slot_state = calloc(hw->num_mt_mask, sizeof(enum SynapticsSlotState));
    if (!hw->slot_state)
        goto fail;

    return Success;

fail:
    for (i--; i >= 0; i--)
        valuator_mask_free(&hw->mt_mask[i]);
    free(hw->mt_mask);
    hw->mt_mask = NULL;
    return BadAlloc;
}
#endif

struct SynapticsHwState *
SynapticsHwStateAlloc(SynapticsPrivate *priv)
{
    struct SynapticsHwState *hw;

    hw = calloc(1, sizeof(struct SynapticsHwState));
    if (!hw)
        return NULL;

#ifdef HAVE_MULTITOUCH
    if (HwStateAllocTouch(hw, priv) != Success)
    {
        free(hw);
        return NULL;
    }
#endif

    return hw;
}

void
SynapticsHwStateFree(struct SynapticsHwState **hw)
{
#ifdef HAVE_MULTITOUCH
    int i;

    if (!*hw)
        return;

    free((*hw)->slot_state);
    for (i = 0; i < (*hw)->num_mt_mask; i++)
        valuator_mask_free(&(*hw)->mt_mask[i]);
    free((*hw)->mt_mask);
#endif

    free(*hw);
    *hw = NULL;
}

void
SynapticsCopyHwState(struct SynapticsHwState *dst,
                     const struct SynapticsHwState *src)
{
#ifdef HAVE_MULTITOUCH
    int i;
#endif

    dst->millis = src->millis;
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
    dst->cumulative_dx = src->cumulative_dx;
    dst->cumulative_dy = src->cumulative_dy;
    dst->numFingers = src->numFingers;
    dst->fingerWidth = src->fingerWidth;
    dst->left = src->left & BTN_EMULATED_FLAG ? 0 : src->left;
    dst->right = src->right & BTN_EMULATED_FLAG ? 0 : src->right;
    dst->up = src->up;
    dst->down = src->down;
    memcpy(dst->multi, src->multi, sizeof(dst->multi));
    dst->middle = src->middle & BTN_EMULATED_FLAG ? 0 : src->middle;
#ifdef HAVE_MULTITOUCH
    for (i = 0; i < dst->num_mt_mask && i < src->num_mt_mask; i++)
        valuator_mask_copy(dst->mt_mask[i], src->mt_mask[i]);
    memcpy(dst->slot_state, src->slot_state,
           dst->num_mt_mask * sizeof(enum SynapticsSlotState));
#endif
}

void
SynapticsResetTouchHwState(struct SynapticsHwState *hw)
{
#ifdef HAVE_MULTITOUCH
    int i;

    for (i = 0; i < hw->num_mt_mask; i++)
    {
        int j;

        /* Leave x and y valuators in case we need to restart touch */
        for (j = 2; j < valuator_mask_num_valuators(hw->mt_mask[i]); j++)
            valuator_mask_unset(hw->mt_mask[i], j);

        hw->slot_state[i] = SLOTSTATE_EMPTY;
    }
#endif
}
