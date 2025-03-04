#include "pressable-obj.h"
#include "util.h"
#include "log.h"
#include <stdlib.h>

pressable_obj_t * pressable_obj_create(void)
{
    pressable_obj_t *po = calloc(1, sizeof(pressable_obj_t));
    if (po == NULL)
        s_log_fatal("pressable-obj", "pressable_obj_create",
            "calloc failed for %s", "new pressable obj");

    return po;
}

void pressable_obj_update(pressable_obj_t *po, bool state)
{
    if (po == NULL) return;

    /* Down is active if on the current tick the object is pressed,
     * but wasn't pressed on the previous one */
    po->down = (state && !po->pressed && !po->force_released);

    /* Up is active immidiately after object was released. */
    po->up = (!state && po->pressed && !po->force_released);

    /* Update the po->pressed member only if the object isn't force released */
    po->pressed = state && !po->force_released;

    /* Time pressed should always be incremented when the object is pressed */
    if(state) {
        po->time++;
    } else {
        /* Otherwise reset the time, as well as the force_released state */
        po->time = 0;
        po->force_released = false;
    }
}

void pressable_obj_reset(pressable_obj_t *po)
{
    if (po == NULL) return;
    memset(po, 0, sizeof(pressable_obj_t));
}

void pressable_obj_force_release(pressable_obj_t *po)
{
    if (po == NULL) return;
    /* The pressed, up and down values as well as the time
     * should be 0 until the force_released state is reset */
    po->pressed = false;
    po->up = false;
    po->down = false;
    po->time = 0;
    po->force_released = true;
}

void pressable_obj_destroy(pressable_obj_t **po_p)
{
    if (po_p == NULL || *po_p == NULL) return;

    u_nzfree(po_p);
}
