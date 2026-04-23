#ifndef PLATFORM_FRAME_SYNC_H
#define PLATFORM_FRAME_SYNC_H

#include <stdint.h>

void frame_sync_init(void);
void frame_sync_wait_vblank(void);
uint8_t frame_sync_subframe_60_to_10hz(void);

#endif