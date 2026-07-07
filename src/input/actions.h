#ifndef ACTIONS_H
#define ACTIONS_H

#include "config/actions_loader.h"

extern key_action_t *actions_array;
extern i32 actions_count;

// Process all registered actions. Must be called from the main thread after
// BeginDrawing (so tr_raylib input state is current).
void process_actions(void);

#endif //ACTIONS_H
