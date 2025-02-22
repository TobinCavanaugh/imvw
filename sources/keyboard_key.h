//
// Created by tobin on 2025-02-21.
//

#ifndef KEYBOARD_KEY_H
#define KEYBOARD_KEY_H
#include "raylib.h"

typedef struct {
    char name[32];
    int key;
} keyboard_key_t;

typedef struct {
    char name[32];
    int key;
} mouse_button_t;

mouse_button_t mouse_buttons[] = {
    {"BUTTON_LEFT", MOUSE_BUTTON_LEFT},
    {"BUTTON_RIGHT", MOUSE_BUTTON_RIGHT},
    {"BUTTON_MIDDLE", MOUSE_BUTTON_MIDDLE},
    {"BUTTON_SIDE", MOUSE_BUTTON_SIDE},
    {"BUTTON_EXTRA", MOUSE_BUTTON_EXTRA},
    {"BUTTON_FORWARD", MOUSE_BUTTON_FORWARD},
    {"BUTTON_BACK", MOUSE_BUTTON_BACK},
};

keyboard_key_t keyboard_keys[] = {
    {"NULL", KEY_NULL},
    {"APOSTROPHE", KEY_APOSTROPHE},
    {"COMMA", KEY_COMMA},
    {"MINUS", KEY_MINUS},
    {"PERIOD", KEY_PERIOD},
    {"SLASH", KEY_SLASH},
    {"ZERO", KEY_ZERO},
    {"ONE", KEY_ONE},
    {"TWO", KEY_TWO},
    {"THREE", KEY_THREE},
    {"FOUR", KEY_FOUR},
    {"FIVE", KEY_FIVE},
    {"SIX", KEY_SIX},
    {"SEVEN", KEY_SEVEN},
    {"EIGHT", KEY_EIGHT},
    {"NINE", KEY_NINE},
    {"SEMICOLON", KEY_SEMICOLON},
    {"EQUAL", KEY_EQUAL},
    {"A", KEY_A},
    {"B", KEY_B},
    {"C", KEY_C},
    {"D", KEY_D},
    {"E", KEY_E},
    {"F", KEY_F},
    {"G", KEY_G},
    {"H", KEY_H},
    {"I", KEY_I},
    {"J", KEY_J},
    {"K", KEY_K},
    {"L", KEY_L},
    {"M", KEY_M},
    {"N", KEY_N},
    {"O", KEY_O},
    {"P", KEY_P},
    {"Q", KEY_Q},
    {"R", KEY_R},
    {"S", KEY_S},
    {"T", KEY_T},
    {"U", KEY_U},
    {"V", KEY_V},
    {"W", KEY_W},
    {"X", KEY_X},
    {"Y", KEY_Y},
    {"Z", KEY_Z},
    {"LEFT_BRACKET", KEY_LEFT_BRACKET},
    {"BACKSLASH", KEY_BACKSLASH},
    {"RIGHT_BRACKET", KEY_RIGHT_BRACKET},
    {"GRAVE", KEY_GRAVE},
    {"SPACE", KEY_SPACE},
    {"ESCAPE", KEY_ESCAPE},
    {"ENTER", KEY_ENTER},
    {"TAB", KEY_TAB},
    {"BACKSPACE", KEY_BACKSPACE},
    {"INSERT", KEY_INSERT},
    {"DELETE", KEY_DELETE},
    {"RIGHT", KEY_RIGHT},
    {"LEFT", KEY_LEFT},
    {"DOWN", KEY_DOWN},
    {"UP", KEY_UP},
    {"PAGE_UP", KEY_PAGE_UP},
    {"PAGE_DOWN", KEY_PAGE_DOWN},
    {"HOME", KEY_HOME},
    {"END", KEY_END},
    {"CAPS_LOCK", KEY_CAPS_LOCK},
    {"SCROLL_LOCK", KEY_SCROLL_LOCK},
    {"NUM_LOCK", KEY_NUM_LOCK},
    {"PRINT_SCREEN", KEY_PRINT_SCREEN},
    {"PAUSE", KEY_PAUSE},
    {"F1", KEY_F1},
    {"F2", KEY_F2},
    {"F3", KEY_F3},
    {"F4", KEY_F4},
    {"F5", KEY_F5},
    {"F6", KEY_F6},
    {"F7", KEY_F7},
    {"F8", KEY_F8},
    {"F9", KEY_F9},
    {"F10", KEY_F10},
    {"F11", KEY_F11},
    {"F12", KEY_F12},
    {"LEFT_SHIFT", KEY_LEFT_SHIFT},
    {"LEFT_CONTROL", KEY_LEFT_CONTROL},
    {"LEFT_ALT", KEY_LEFT_ALT},
    {"LEFT_SUPER", KEY_LEFT_SUPER},
    {"RIGHT_SHIFT", KEY_RIGHT_SHIFT},
    {"RIGHT_CONTROL", KEY_RIGHT_CONTROL},
    {"RIGHT_ALT", KEY_RIGHT_ALT},
    {"RIGHT_SUPER", KEY_RIGHT_SUPER},
    {"KB_MENU", KEY_KB_MENU},
    {"KP_0", KEY_KP_0},
    {"KP_1", KEY_KP_1},
    {"KP_2", KEY_KP_2},
    {"KP_3", KEY_KP_3},
    {"KP_4", KEY_KP_4},
    {"KP_5", KEY_KP_5},
    {"KP_6", KEY_KP_6},
    {"KP_7", KEY_KP_7},
    {"KP_8", KEY_KP_8},
    {"KP_9", KEY_KP_9},
    {"KP_DECIMAL", KEY_KP_DECIMAL},
    {"KP_DIVIDE", KEY_KP_DIVIDE},
    {"KP_MULTIPLY", KEY_KP_MULTIPLY},
    {"KP_SUBTRACT", KEY_KP_SUBTRACT},
    {"KP_ADD", KEY_KP_ADD},
    {"KP_ENTER", KEY_KP_ENTER},
    {"KP_EQUAL", KEY_KP_EQUAL},
    {"BACK", KEY_BACK},
    {"MENU", KEY_MENU},
    {"VOLUME_UP", KEY_VOLUME_UP},
    {"VOLUME_DOWN", KEY_VOLUME_DOWN}
};


#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

KeyboardKey GetKeyFromName(char *name) {
    char *pre = "KEY_";
    if (strncmp(pre, name, strlen(pre)) == 0) {
        name += strlen(pre);
    }

    i32 i = 0;
    for (; i < ARRAY_LEN(keyboard_keys); i++) {
        if (strcmp(name, keyboard_keys[i].name) == 0) {
            return keyboard_keys[i].key;
        }
    }

    return KEY_NULL;
}

MouseButton GetButtonFromName(const char *name) {
    char *pre = "MOUSE_";
    if (strncmp(pre, name, strlen(pre)) == 0) {
        name += strlen(pre);
    }

    i32 i = 0;
    for (; i < ARRAY_LEN(mouse_buttons); i++) {
        if (strcmp(name, mouse_buttons[i].name) == 0) {
            return mouse_buttons[i].key;
        }
    }

    return -1;
}

#endif //KEYBOARD_KEY_H
