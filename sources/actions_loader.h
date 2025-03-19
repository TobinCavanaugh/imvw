//
// Created by tobin on 2025-02-22.
//

#include "keyboard_key.h"
#include "flut.h"

#ifndef ACTIONS_LOADER_H
#define ACTIONS_LOADER_H

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
        cJSON *arg_obj;
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
    KeyboardKey modifier; //Optional

    // Mousebutton to be used
    MouseButton button; //Optional
    u8 priv_use_mouse;
} key_action_t;

u0 actions_add(key_action_t **out_ptr_actions_array, i32 *out_actions_count, key_action_t act) {
    *out_ptr_actions_array = realloc(*out_ptr_actions_array, (*out_actions_count + 1) * sizeof(key_action_t));
    if (*out_ptr_actions_array == NULL) {
        exit(123);
    }
    (*out_ptr_actions_array)[*out_actions_count] = act;
    (*out_actions_count)++;
}


/// Load the key actions from the `imvw.json` file.
u0 load_actions(cJSON *json_data, key_action_t **out_ptr_actions_array, i32 *out_actions_count) {
    // TODO: Final build should check next to exe? or maybe in %appdata%

    // Parse the actions
    cJSON *actions = cJSON_GetObjectItem(json_data, "actions");
    if (actions == NULL) {
        fprintf(stderr, "Failed to load `actions` from your `imvw.json` settings file.\n");
    }
    i32 array_size = cJSON_GetArraySize(actions); {
        i32 i = 0;
        for (; i < array_size; i++) {
            cJSON *action = cJSON_GetArrayItem(actions, i);
            cJSON *func = cJSON_GetObjectItem(action, "func");

            //TODO implement a key repeat like aaaaaaaaaaaaaaaaaaaaaa
            cJSON *press = cJSON_GetObjectItem(action, "press");
            cJSON *hold = cJSON_GetObjectItem(action, "hold");
            cJSON *modifier = cJSON_GetObjectItem(action, "modifier");
            cJSON *button = cJSON_GetObjectItem(action, "button");
            cJSON *arg = cJSON_GetObjectItem(action, "arg");

            if (action == NULL) {
                continue;
            }

            key_action_t key_action = {0};

            // Copy the function name
            func != NULL ? strcpy(key_action.func, cJSON_GetStringValue(func)) : 0;

            if (press != NULL) {
                KeyboardKey k = GetKeyFromName(cJSON_GetStringValue(press));
                if (k == KEY_NULL) {
                    fprintf(stderr, "Could not parse press `%s` into keyboard key\n", cJSON_GetStringValue(press));
                }

                key_action.press = k;
            }
            if (modifier != NULL) {
                KeyboardKey m = GetKeyFromName(cJSON_GetStringValue(modifier));
                if (m == KEY_NULL) {
                    fprintf(stderr, "Could not parse modifier `%s` into keyboard key\n",
                            cJSON_GetStringValue(modifier));
                }
                key_action.modifier = m;
            }
            if (hold != NULL) {
                KeyboardKey h = GetKeyFromName(cJSON_GetStringValue(hold));
                if (h == KEY_NULL) {
                    fprintf(stderr, "Could not parse hold `%s` into keyboard key\n", cJSON_GetStringValue(hold));
                }
                key_action.hold = h;
            }
            if (button != NULL) {
                if (cJSON_IsNumber(button)) {
                    key_action.button = (i32) cJSON_GetNumberValue(button);
                    key_action.priv_use_mouse = 1;
                }
                if (cJSON_IsString(button)) {
                    key_action.button = GetButtonFromName(cJSON_GetStringValue(button));
                    key_action.priv_use_mouse = 1;
                }
            }

            key_action.arg_type = ARG_TYPE_NONE;
            if (arg) {
                if (cJSON_IsNumber(arg)) {
                    key_action.arg_type = ARG_TYPE_NUM;
                    key_action.arg_num = (f32) cJSON_GetNumberValue(arg);
                } else if (cJSON_IsBool(arg)) {
                    key_action.arg_type = ARG_TYPE_BOOL;
                    key_action.arg_bool = (u64) cJSON_IsTrue(arg);
                } else if (cJSON_IsString(arg)) {
                    key_action.arg_type = ARG_TYPE_STR;
                    key_action.arg_str = strdup(cJSON_GetStringValue(arg));
                } else {
                    key_action.arg_type = ARG_TYPE_OBJECT;
                    key_action.arg_obj = cJSON_Duplicate(arg, 1);
                }
            }

            if (key_action.priv_use_mouse == 0 && key_action.hold == KEY_NULL && key_action.press == KEY_NULL &&
                key_action.modifier == KEY_NULL) {
                char *x = cJSON_Print(action);
                fprintf(
                    stderr,
                    "Action does not contain an associated key. It will not be added. JSON Contents: \n`\n%s\n`\n", x);
                free(x);
            }
            if (key_action.func == NULL || strlen(key_action.func) == 0) {
                char *x = cJSON_Print(action);
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
