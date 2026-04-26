#ifndef MINIMAP_H
#define MINIMAP_H

#include <stdint.h>

void minimap_init(void);
void minimap_update_player(uint16_t world_x);

#endif // MINIMAP_H