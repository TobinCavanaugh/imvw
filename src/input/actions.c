#include "actions.h"
#include "core/context_t.h"
#include "input/flut.h"
#include <string.h>
#include <stdio.h>

extern context_t ctx;

key_action_t *actions_array = NULL;
i32 actions_count = 0;

// Pre-fetch modifier key states once per frame so per-action checks use the
// cached values instead of calling IsKeyDown (GetAsyncKeyState) repeatedly.
#define MOD_CTRL  (IsKeyDown(KEY_LEFT_CONTROL)  || IsKeyDown(KEY_RIGHT_CONTROL))
#define MOD_SHIFT (IsKeyDown(KEY_LEFT_SHIFT)    || IsKeyDown(KEY_RIGHT_SHIFT))
#define MOD_ALT   (IsKeyDown(KEY_LEFT_ALT)      || IsKeyDown(KEY_RIGHT_ALT))
#define MOD_SUPER (IsKeyDown(KEY_LEFT_SUPER)    || IsKeyDown(KEY_RIGHT_SUPER))
#define MOD_ANY   (MOD_CTRL || MOD_SHIFT || MOD_ALT || MOD_SUPER)

void process_actions(void) {
    // Skip all input processing when the window doesn't have focus —
    // tr_raylib polls keys via GetAsyncKeyState which ignores focus,
    // so physically held keys would otherwise register as held even
    // when our window is in the background.
    if (!ctx.focused) return;

    // We need a way to track which keys we've already polled this iteration.
    // Since key constants go up to ~348, a fixed array is fine.
    static u8 pressed_cache[512];
    static u8 down_cache[512];

    // Zero out the cache for this frame
    memset(pressed_cache, 0, sizeof(pressed_cache));
    memset(down_cache, 0, sizeof(down_cache));

    // Batch all modifier key states once — 8 syscalls total, regardless of
    // how many actions reference them.
    static u8 cached_mod_ctrl  = 0;
    static u8 cached_mod_shift = 0;
    static u8 cached_mod_alt   = 0;
    static u8 cached_mod_super = 0;
    static u8 cached_mod_any   = 0;
    cached_mod_ctrl  = MOD_CTRL;
    cached_mod_shift = MOD_SHIFT;
    cached_mod_alt   = MOD_ALT;
    cached_mod_super = MOD_SUPER;
    cached_mod_any   = MOD_ANY;

    for (i32 i = 0; i < actions_count; i++) {
        key_action_t a = actions_array[i];
        u8 happened = 0;

        u8 modifier_match = false;
        if (a.modifier == KEY_ANY) {
            modifier_match = true;
        } else if (a.modifier != KEY_NULL) {
            // Check specific modifier from cache if it's one of the four
            // pairs, otherwise fall through to IsKeyDown.
            switch (a.modifier) {
                case KEY_LEFT_CONTROL:  case KEY_RIGHT_CONTROL:
                    modifier_match = cached_mod_ctrl;  break;
                case KEY_LEFT_SHIFT:    case KEY_RIGHT_SHIFT:
                    modifier_match = cached_mod_shift; break;
                case KEY_LEFT_ALT:      case KEY_RIGHT_ALT:
                    modifier_match = cached_mod_alt;   break;
                case KEY_LEFT_SUPER:    case KEY_RIGHT_SUPER:
                    modifier_match = cached_mod_super; break;
                default:
                    modifier_match = IsKeyDown(a.modifier);
                    break;
            }
        } else {
            modifier_match = !cached_mod_any;
        }

        if (modifier_match) {
            // Cache the KeyPress and KeyDown state so subsequent actions
            // on the same key don't miss the event.
            if (a.press != KEY_NULL) {
                if (pressed_cache[a.press] == 0) {
                    pressed_cache[a.press] = IsKeyPressed(a.press) ? 2 : 1;
                }
                if (pressed_cache[a.press] == 2) happened = 1;
            }

            if (a.hold != KEY_NULL) {
                if (down_cache[a.hold] == 0) {
                    down_cache[a.hold] = IsKeyDown(a.hold) ? 2 : 1;
                }
                if (down_cache[a.hold] == 2) happened = 1;
            }

            if (a.priv_use_mouse && IsMouseButtonDown(a.button)) {
                happened = 1;
            }
        }

        if (!happened) continue;

        flut_func_t fft;
        if (flut_get(a.func, &fft)) {
            if (a.arg_type == ARG_TYPE_NONE) {
                fft.func(NULL);
            } else {
                if (a.arg_type == ARG_TYPE_NUM) fft.func(&a.arg_num);
                else if (a.arg_type == ARG_TYPE_BOOL) fft.func(&a.arg_bool);
                else if (a.arg_type == ARG_TYPE_STR) fft.func(a.arg_str);
                else if (a.arg_type == ARG_TYPE_OBJECT) fft.func(a.arg_obj);
            }
        } else {
            fprintf(stderr, "Could not find function of name `%s`\n", a.func);
        }
    }
}
