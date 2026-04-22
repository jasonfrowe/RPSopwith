#include "platform/frame_sync.h"

#include <rp6502.h>

static uint8_t s_last_vsync;
static uint8_t s_subframe;

void frame_sync_init(void)
{
    s_last_vsync = RIA.vsync;
    s_subframe = 0;
}

void frame_sync_wait_vblank(void)
{
    while (RIA.vsync == s_last_vsync) {
    }
    s_last_vsync = RIA.vsync;
}

uint8_t frame_sync_subframe_60_to_10hz(void)
{
    s_subframe++;
    if (s_subframe >= 6) {
        s_subframe = 0;
    }
    return s_subframe;
}