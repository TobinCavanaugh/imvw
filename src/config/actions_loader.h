//
// Created by tobin on 2025-02-22.
//

#ifndef ACTIONS_LOADER_H
#define ACTIONS_LOADER_H

#include "input/keyboard_key.h"
#include "input/flut.h"
#include "../external/yyjson.h"
#include <stdio.h>

typedef enum {
    ARG_TYPE_NONE,
    ARG_TYPE_STR,
    ARG_TYPE_BOOL,
    ARG_TYPE_NUM,
    ARG_TYPE_OBJECT,
} ARG_TYPES;

typedef struct {
    // Function to be called
    char func[FLUT_FUNC_NAME_MAX]; //Include

    //Optional T
    union {
        char *arg_json_str;
        u8 *arg_str;
        f32 arg_num;
        u8 arg_bool;
    };

    //TODO Rework to be:
    // - PRESS
    // - DOWN
    // - RELEASE
    // Add support for an array of modifier keys
    // Add support for negative keys, i.e. ones that cannot be held for action

    ARG_TYPES arg_type; // Exclude

    // Key to be pressed
    KeyboardKey press; //Include
    // u8 priv_use_key; //Exclude

    // Key to be held
    KeyboardKey hold;
    // u8 priv_use_hold;

    // Optional key to be held
    // KeyboardKey
    i32 modifier; //Optional

    // Mousebutton to be used
    MouseButton button; //Optional
    u8 priv_use_mouse;
} key_action_t;

static u0 actions_add(key_action_t **out_ptr_actions_array, i32 *out_actions_count, key_action_t act) {
    *out_ptr_actions_array = (key_action_t *) realloc(*out_ptr_actions_array,
                                                      (*out_actions_count + 1) * sizeof(key_action_t));
    if (*out_ptr_actions_array == NULL) {
        exit(123);
    }
    (*out_ptr_actions_array)[*out_actions_count] = act;
    (*out_actions_count)++;
}


/// Load the key actions from the `imvw.json` file.
static u0 load_actions(yyjson_val *json_data, key_action_t **out_ptr_actions_array, i32 *out_actions_count) {
    // TODO: Final build should check next to exe? or maybe in %appdata%

    // Parse the actions
    yyjson_val *actions = yyjson_obj_get(json_data, "actions");
    if (actions == NULL) {
        fprintf(stderr, "Failed to load `actions` from your `imvw.json` settings file.\n");
    }
    i32 array_size = (i32) yyjson_arr_size(actions);
    {
        i32 i = 0;
        for (; i < array_size; i++) {
            yyjson_val *action = yyjson_arr_get(actions, i);
            yyjson_val *func = yyjson_obj_get(action, "func");

            //TODO implement a key repeat like aaaaaaaaaaaaaaaaaaaaaa
            yyjson_val *press = yyjson_obj_get(action, "press");
            yyjson_val *hold = yyjson_obj_get(action, "hold");
            yyjson_val *modifier = yyjson_obj_get(action, "modifier");
            yyjson_val *button = yyjson_obj_get(action, "button");
            yyjson_val *arg = yyjson_obj_get(action, "arg");

            if (action == NULL) {
                continue;
            }

            key_action_t key_action = {0};

            // Copy the function name
            if (func != NULL) strcpy(key_action.func, yyjson_get_str(func));

            if (press != NULL) {
                KeyboardKey k = GetKeyFromName((char*) yyjson_get_str(press));
                if (k == KEY_NULL) {
                    fprintf(stderr, "Could not parse press `%s` into keyboard key\n", yyjson_get_str(press));
                }

                key_action.press = k;
            }
            if (modifier != NULL) {
                // TODO Fix for explicit null (?)
                i32 m = GetKeyFromName((char*) yyjson_get_str(modifier));
                if (m == KEY_NULL) {
                    fprintf(stderr, "Could not parse modifier `%s` into keyboard key\n",
                            yyjson_get_str(modifier));
                }
                key_action.modifier = m;
            }
            if (hold != NULL) {
                i32 h = GetKeyFromName((char*) yyjson_get_str(hold));
                if (h == KEY_NULL) {
                    fprintf(stderr, "Could not parse hold `%s` into keyboard key\n", yyjson_get_str(hold));
                }
                key_action.hold = h;
            }
            if (button != NULL) {
                if (yyjson_is_num(button)) {
                    key_action.button = (MouseButton) (i32) yyjson_get_num(button);
                    key_action.priv_use_mouse = 1;
                }
                if (yyjson_is_str(button)) {
                    key_action.button = GetButtonFromName((char*) yyjson_get_str(button));
                    key_action.priv_use_mouse = 1;
                }
            }

            key_action.arg_type = ARG_TYPE_NONE;
            if (arg) {
                if (yyjson_is_num(arg)) {
                    key_action.arg_type = ARG_TYPE_NUM;
                    key_action.arg_num = (f32) yyjson_get_num(arg);
                } else if (yyjson_is_bool(arg)) {
                    key_action.arg_type = ARG_TYPE_BOOL;
                    key_action.arg_bool = (u8) yyjson_get_bool(arg);
                } else if (yyjson_is_str(arg)) {
                    key_action.arg_type = ARG_TYPE_STR;
                    key_action.arg_str = (unsigned char *) strdup(yyjson_get_str(arg));
                } else {
                    key_action.arg_type = ARG_TYPE_OBJECT;
                    key_action.arg_json_str = yyjson_val_write(arg, 0, NULL);
                }
            }

            if (key_action.priv_use_mouse == 0 && key_action.hold == KEY_NULL && key_action.press == KEY_NULL &&
                key_action.modifier == KEY_NULL) {
                char *x = yyjson_val_write(action, 0, NULL);
                fprintf(
                        stderr,
                        "Action does not contain an associated key. It will not be added. JSON Contents: \n`\n%s\n`\n",
                        x);
                free(x);
            }
            if (strlen(key_action.func) == 0) {
                char *x = yyjson_val_write(action, 0, NULL);
                fprintf(
                        stderr,
                        "Action does not contain an associated function. It will not be added. JSON Contents: \n`\n%s\n`\n",
                        x);
                free(x);
            }


            actions_add(out_ptr_actions_array, out_actions_count, key_action);
        }
    }
}

#endif //ACTIONS_LOADER_H
