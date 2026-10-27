#include "command_palette.h"
#include "core/context_t.h"
#include "core/settings.h"
#include "input/flut.h"
#include "input/actions.h"
#include "input/keyboard_key.h"
#include "ui/font.h"
#include "ui/text.h"
#include "utils/imvw_time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <trlib.h>

extern context_t ctx;
extern settings_t settings;

#define MAX_PALETTE_COMMANDS 256
#define MAX_VISIBLE_ROWS 9
#define ROW_HEIGHT 30
#define HEADER_HEIGHT 40
#define FOOTER_HEIGHT 24

typedef struct {
    bool is_open;
    char query[128];
    int query_len;
    int selected_index;
    int scroll_offset;
    int filtered_indices[MAX_PALETTE_COMMANDS];
    int filtered_scores[MAX_PALETTE_COMMANDS];
    int filtered_count;

    // Key repeat timers
    int held_key;
    f128 held_time_start;
    f128 last_repeat_time;
} palette_state_t;

static palette_state_t g_palette = {0};

// Forward declaration
static void refilter_commands(void);
static void execute_index(int idx);

// ── Key / Shortcut Formatting Helpers ────────────────────────────────────────

static const char *get_key_name(KeyboardKey key) {
    switch (key) {
        case KEY_APOSTROPHE: return "'";
        case KEY_COMMA: return ",";
        case KEY_MINUS: return "-";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_ZERO: return "0";
        case KEY_ONE: return "1";
        case KEY_TWO: return "2";
        case KEY_THREE: return "3";
        case KEY_FOUR: return "4";
        case KEY_FIVE: return "5";
        case KEY_SIX: return "6";
        case KEY_SEVEN: return "7";
        case KEY_EIGHT: return "8";
        case KEY_NINE: return "9";
        case KEY_SEMICOLON: return ";";
        case KEY_EQUAL: return "=";
        case KEY_A: return "A";
        case KEY_B: return "B";
        case KEY_C: return "C";
        case KEY_D: return "D";
        case KEY_E: return "E";
        case KEY_F: return "F";
        case KEY_G: return "G";
        case KEY_H: return "H";
        case KEY_I: return "I";
        case KEY_J: return "J";
        case KEY_K: return "K";
        case KEY_L: return "L";
        case KEY_M: return "M";
        case KEY_N: return "N";
        case KEY_O: return "O";
        case KEY_P: return "P";
        case KEY_Q: return "Q";
        case KEY_R: return "R";
        case KEY_S: return "S";
        case KEY_T: return "T";
        case KEY_U: return "U";
        case KEY_V: return "V";
        case KEY_W: return "W";
        case KEY_X: return "X";
        case KEY_Y: return "Y";
        case KEY_Z: return "Z";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_BACKSLASH: return "\\";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_GRAVE: return "`";
        case KEY_SPACE: return "Space";
        case KEY_ESCAPE: return "Esc";
        case KEY_ENTER: return "Enter";
        case KEY_TAB: return "Tab";
        case KEY_BACKSPACE: return "Bksp";
        case KEY_INSERT: return "Ins";
        case KEY_DELETE: return "Del";
        case KEY_RIGHT: return "Right";
        case KEY_LEFT: return "Left";
        case KEY_DOWN: return "Down";
        case KEY_UP: return "Up";
        case KEY_PAGE_UP: return "PgUp";
        case KEY_PAGE_DOWN: return "PgDn";
        case KEY_HOME: return "Home";
        case KEY_END: return "End";
        case KEY_CAPS_LOCK: return "Caps";
        case KEY_SCROLL_LOCK: return "ScrLk";
        case KEY_NUM_LOCK: return "NumLk";
        case KEY_PRINT_SCREEN: return "PrtScn";
        case KEY_PAUSE: return "Pause";
        case KEY_F1: return "F1";
        case KEY_F2: return "F2";
        case KEY_F3: return "F3";
        case KEY_F4: return "F4";
        case KEY_F5: return "F5";
        case KEY_F6: return "F6";
        case KEY_F7: return "F7";
        case KEY_F8: return "F8";
        case KEY_F9: return "F9";
        case KEY_F10: return "F10";
        case KEY_F11: return "F11";
        case KEY_F12: return "F12";
        default: return NULL;
    }
}

static const char *get_mod_name(i32 mod) {
    switch (mod) {
        case KEY_LEFT_CONTROL:
        case KEY_RIGHT_CONTROL:
            return "Ctrl";
        case KEY_LEFT_SHIFT:
        case KEY_RIGHT_SHIFT:
            return "Shift";
        case KEY_LEFT_ALT:
        case KEY_RIGHT_ALT:
            return "Alt";
        case KEY_LEFT_SUPER:
        case KEY_RIGHT_SUPER:
            return "Win";
        default:
            return NULL;
    }
}

static const char *get_mouse_btn_name(MouseButton btn) {
    switch (btn) {
        case MOUSE_BUTTON_LEFT: return "LMB";
        case MOUSE_BUTTON_RIGHT: return "RMB";
        case MOUSE_BUTTON_MIDDLE: return "MMB";
        case MOUSE_BUTTON_SIDE: return "Mouse4";
        case MOUSE_BUTTON_EXTRA: return "Mouse5";
        case MOUSE_BUTTON_FORWARD: return "MouseFwd";
        case MOUSE_BUTTON_BACK: return "MouseBack";
        default: return "Mouse";
    }
}

static void format_single_action(const key_action_t *a, char *buf, size_t max_len) {
    buf[0] = '\0';
    const char *mod = (a->modifier != KEY_NULL && a->modifier != KEY_ANY) ? get_mod_name(a->modifier) : NULL;

    if (a->priv_use_mouse) {
        const char *btn = get_mouse_btn_name(a->button);
        if (mod) snprintf(buf, max_len, "%s+%s", mod, btn);
        else snprintf(buf, max_len, "%s", btn);
        return;
    }

    KeyboardKey k = (a->press != KEY_NULL) ? a->press : a->hold;
    const char *kname = get_key_name(k);
    if (!kname) return;

    if (mod) {
        snprintf(buf, max_len, "%s+%s", mod, kname);
    } else {
        snprintf(buf, max_len, "%s", kname);
    }
}

void command_palette_get_shortcut(const char *func_name, char *buf, size_t max_len) {
    buf[0] = '\0';
    if (!actions_array || actions_count <= 0 || !func_name) return;

    char single[32];
    int count = 0;
    for (int i = 0; i < actions_count; i++) {
        if (strcmp(actions_array[i].func, func_name) == 0) {
            format_single_action(&actions_array[i], single, sizeof(single));
            if (single[0] != '\0') {
                if (strstr(buf, single) == NULL) {
                    if (count > 0) {
                        if (strlen(buf) + strlen(single) + 4 < max_len) {
                            strcat(buf, " / ");
                            strcat(buf, single);
                            count++;
                        }
                    } else {
                        snprintf(buf, max_len, "%s", single);
                        count++;
                    }
                }
            }
        }
    }
}

// ── Search & Filter Logic ───────────────────────────────────────────────────

static int score_match(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return 1000;
    if (!haystack || !haystack[0]) return 0;

    char h[128], n[128];
    int h_len = 0, n_len = 0;
    while (haystack[h_len] && h_len < 127) {
        h[h_len] = (char)tolower((unsigned char)haystack[h_len]);
        h_len++;
    }
    h[h_len] = '\0';

    while (needle[n_len] && n_len < 127) {
        n[n_len] = (char)tolower((unsigned char)needle[n_len]);
        n_len++;
    }
    n[n_len] = '\0';

    // 1. Exact match
    if (strcmp(h, n) == 0) return 10000;

    // 2. Substring match
    char *sub = strstr(h, n);
    if (sub) {
        int pos = (int)(sub - h);
        if (pos == 0) return 5000; // Prefix match
        if (pos > 0 && (h[pos - 1] == '_' || h[pos - 1] == ' ' || h[pos - 1] == '-')) {
            return 4000; // Word boundary match
        }
        return 3000 - pos;
    }

    // 3. Fuzzy subsequence match
    int ni = 0;
    int score = 1000;
    int consecutive = 0;
    for (int hi = 0; hi < h_len && ni < n_len; hi++) {
        if (h[hi] == n[ni]) {
            ni++;
            score += 50 + consecutive * 25;
            if (hi == 0 || h[hi - 1] == '_' || h[hi - 1] == ' ') score += 40;
            consecutive++;
        } else {
            consecutive = 0;
            score -= 1;
        }
    }

    if (ni == n_len) {
        return score > 0 ? score : 1;
    }

    return 0;
}

static void refilter_commands(void) {
    g_palette.filtered_count = 0;
    if (!flut_array || flut_count <= 0) return;

    int count = flut_count < MAX_PALETTE_COMMANDS ? flut_count : MAX_PALETTE_COMMANDS;

    for (int i = 0; i < count; i++) {
        const char *name = flut_array[i].key;
        char shortcut[64] = {0};
        command_palette_get_shortcut(name, shortcut, sizeof(shortcut));

        int score_name = score_match(name, g_palette.query);
        int score_sc = shortcut[0] ? score_match(shortcut, g_palette.query) : 0;
        int max_score = score_name > score_sc ? score_name : score_sc;

        if (max_score > 0) {
            // Insertion sort descending by score
            int insert_pos = g_palette.filtered_count;
            while (insert_pos > 0 && g_palette.filtered_scores[insert_pos - 1] < max_score) {
                g_palette.filtered_indices[insert_pos] = g_palette.filtered_indices[insert_pos - 1];
                g_palette.filtered_scores[insert_pos] = g_palette.filtered_scores[insert_pos - 1];
                insert_pos--;
            }
            g_palette.filtered_indices[insert_pos] = i;
            g_palette.filtered_scores[insert_pos] = max_score;
            g_palette.filtered_count++;
        }
    }

    if (g_palette.selected_index >= g_palette.filtered_count) {
        g_palette.selected_index = g_palette.filtered_count > 0 ? g_palette.filtered_count - 1 : 0;
    }
    if (g_palette.selected_index < 0) {
        g_palette.selected_index = 0;
    }
    g_palette.scroll_offset = 0;
}

// ── Public Control Functions ────────────────────────────────────────────────

void command_palette_init(void) {
    g_palette.is_open = false;
    g_palette.query[0] = '\0';
    g_palette.query_len = 0;
    g_palette.selected_index = 0;
    g_palette.scroll_offset = 0;
    g_palette.filtered_count = 0;
    g_palette.held_key = 0;
}

void command_palette_open(void) {
    g_palette.is_open = true;
    g_palette.query[0] = '\0';
    g_palette.query_len = 0;
    g_palette.selected_index = 0;
    g_palette.scroll_offset = 0;
    g_palette.held_key = 0;
    refilter_commands();
}

void command_palette_close(void) {
    g_palette.is_open = false;
    g_palette.query[0] = '\0';
    g_palette.query_len = 0;
    g_palette.held_key = 0;
}

void command_palette_toggle(void) {
    if (g_palette.is_open) {
        command_palette_close();
    } else {
        command_palette_open();
    }
}

bool command_palette_is_open(void) {
    return g_palette.is_open;
}

void command_palette_on_char(char c) {
    if (!g_palette.is_open) return;

    // Printable ASCII only
    if (c >= 32 && c <= 126) {
        if (g_palette.query_len < (int)sizeof(g_palette.query) - 1) {
            g_palette.query[g_palette.query_len++] = c;
            g_palette.query[g_palette.query_len] = '\0';
            g_palette.selected_index = 0;
            refilter_commands();
        }
    }
}

static void execute_index(int filtered_idx) {
    if (filtered_idx < 0 || filtered_idx >= g_palette.filtered_count) return;
    int cmd_idx = g_palette.filtered_indices[filtered_idx];
    if (cmd_idx < 0 || cmd_idx >= flut_count) return;

    flut_func_t cmd = flut_array[cmd_idx];

    // Find any matching action to pass the configured argument
    key_action_t *matching_act = NULL;
    if (actions_array && actions_count > 0) {
        for (int i = 0; i < actions_count; i++) {
            if (strcmp(actions_array[i].func, cmd.key) == 0) {
                matching_act = &actions_array[i];
                break;
            }
        }
    }

    command_palette_close();

    if (cmd.func) {
        if (matching_act && matching_act->arg_type != ARG_TYPE_NONE) {
            if (matching_act->arg_type == ARG_TYPE_NUM) cmd.func(&matching_act->arg_num);
            else if (matching_act->arg_type == ARG_TYPE_BOOL) cmd.func(&matching_act->arg_bool);
            else if (matching_act->arg_type == ARG_TYPE_STR) cmd.func(matching_act->arg_str);
            else if (matching_act->arg_type == ARG_TYPE_OBJECT) cmd.func(matching_act->arg_json_str);
            else cmd.func(NULL);
        } else {
            cmd.func(NULL);
        }
    }
}

void Toggle_Command_Palette(void *arg) {
    (void)arg;
    command_palette_toggle();
}

// ── Per-Frame Update ────────────────────────────────────────────────────────

void command_palette_update(void) {
    if (!ctx.focused) return;

    // Check for Ctrl+P toggle shortcut
    bool ctrl_down = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                      (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
    if (ctrl_down && IsKeyPressed(KEY_P)) {
        command_palette_toggle();
        return;
    }

    if (!g_palette.is_open) return;

    f128 now = getTimeHD_ms();

    // 1. Escape: dismiss
    if (IsKeyPressed(KEY_ESCAPE)) {
        command_palette_close();
        return;
    }

    // 2. Enter: execute selected command
    if (IsKeyPressed(KEY_ENTER)) {
        execute_index(g_palette.selected_index);
        return;
    }

    // 3. Backspace handling with repeat
    if (IsKeyPressed(KEY_BACKSPACE)) {
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            g_palette.query[0] = '\0';
            g_palette.query_len = 0;
            g_palette.selected_index = 0;
            refilter_commands();
        } else if (g_palette.query_len > 0) {
            g_palette.query[--g_palette.query_len] = '\0';
            g_palette.selected_index = 0;
            refilter_commands();
        }
        g_palette.held_key = KEY_BACKSPACE;
        g_palette.held_time_start = now;
        g_palette.last_repeat_time = now;
    } else if (IsKeyDown(KEY_BACKSPACE) && g_palette.held_key == KEY_BACKSPACE) {
        if (now - g_palette.held_time_start > 300.0 && now - g_palette.last_repeat_time > 35.0) {
            if (g_palette.query_len > 0) {
                g_palette.query[--g_palette.query_len] = '\0';
                g_palette.selected_index = 0;
                refilter_commands();
            }
            g_palette.last_repeat_time = now;
        }
    }

    // 4. Navigation: Up / Down with repeat
    bool move_up = IsKeyPressed(KEY_UP) ||
                   ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_K)));
    bool move_down = IsKeyPressed(KEY_DOWN) ||
                     ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && (IsKeyPressed(KEY_N) || IsKeyPressed(KEY_J)));

    if (move_up) {
        if (g_palette.filtered_count > 0) {
            g_palette.selected_index = (g_palette.selected_index - 1 + g_palette.filtered_count) % g_palette.filtered_count;
        }
        g_palette.held_key = KEY_UP;
        g_palette.held_time_start = now;
        g_palette.last_repeat_time = now;
    } else if (IsKeyDown(KEY_UP) && g_palette.held_key == KEY_UP) {
        if (now - g_palette.held_time_start > 300.0 && now - g_palette.last_repeat_time > 40.0) {
            if (g_palette.filtered_count > 0) {
                g_palette.selected_index = (g_palette.selected_index - 1 + g_palette.filtered_count) % g_palette.filtered_count;
            }
            g_palette.last_repeat_time = now;
        }
    }

    if (move_down) {
        if (g_palette.filtered_count > 0) {
            g_palette.selected_index = (g_palette.selected_index + 1) % g_palette.filtered_count;
        }
        g_palette.held_key = KEY_DOWN;
        g_palette.held_time_start = now;
        g_palette.last_repeat_time = now;
    } else if (IsKeyDown(KEY_DOWN) && g_palette.held_key == KEY_DOWN) {
        if (now - g_palette.held_time_start > 300.0 && now - g_palette.last_repeat_time > 40.0) {
            if (g_palette.filtered_count > 0) {
                g_palette.selected_index = (g_palette.selected_index + 1) % g_palette.filtered_count;
            }
            g_palette.last_repeat_time = now;
        }
    }

    if (!IsKeyDown(KEY_UP) && !IsKeyDown(KEY_DOWN) && !IsKeyDown(KEY_BACKSPACE)) {
        g_palette.held_key = 0;
    }

    // Page Up / Page Down
    if (IsKeyPressed(KEY_PAGE_UP)) {
        g_palette.selected_index -= MAX_VISIBLE_ROWS;
        if (g_palette.selected_index < 0) g_palette.selected_index = 0;
    }
    if (IsKeyPressed(KEY_PAGE_DOWN)) {
        g_palette.selected_index += MAX_VISIBLE_ROWS;
        if (g_palette.selected_index >= g_palette.filtered_count) {
            g_palette.selected_index = g_palette.filtered_count > 0 ? g_palette.filtered_count - 1 : 0;
        }
    }

    // Auto-scroll to keep selected item in view
    if (g_palette.selected_index < g_palette.scroll_offset) {
        g_palette.scroll_offset = g_palette.selected_index;
    }
    if (g_palette.selected_index >= g_palette.scroll_offset + MAX_VISIBLE_ROWS) {
        g_palette.scroll_offset = g_palette.selected_index - MAX_VISIBLE_ROWS + 1;
    }

    // 5. Mouse Interaction
    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    int box_w = screen_w - 40 < 540 ? screen_w - 40 : 540;
    if (box_w < 280) box_w = 280;
    int box_x = (screen_w - box_w) / 2;
    int box_y = screen_h * 0.12f > 24 ? (int)(screen_h * 0.12f) : 24;

    int visible_rows = g_palette.filtered_count < MAX_VISIBLE_ROWS ? g_palette.filtered_count : MAX_VISIBLE_ROWS;
    if (visible_rows == 0) visible_rows = 1; // "No matching commands" row
    int box_h = HEADER_HEIGHT + visible_rows * ROW_HEIGHT + 6 + FOOTER_HEIGHT;

    Vector2 mouse = GetMousePosition();

    // Mouse wheel scrolling
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        g_palette.scroll_offset -= (int)roundf(wheel) * 2;
        int max_scroll = g_palette.filtered_count > MAX_VISIBLE_ROWS ? g_palette.filtered_count - MAX_VISIBLE_ROWS : 0;
        if (g_palette.scroll_offset > max_scroll) g_palette.scroll_offset = max_scroll;
        if (g_palette.scroll_offset < 0) g_palette.scroll_offset = 0;
    }

    // Mouse click outside palette box closes it
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (mouse.x < box_x || mouse.x > box_x + box_w || mouse.y < box_y || mouse.y > box_y + box_h) {
            command_palette_close();
            return;
        }
    }

    // Mouse hover and row click
    if (g_palette.filtered_count > 0) {
        int list_start_y = box_y + HEADER_HEIGHT + 3;
        for (int r = 0; r < visible_rows && r < g_palette.filtered_count; r++) {
            int row_y = list_start_y + r * ROW_HEIGHT;
            if (mouse.x >= box_x + 6 && mouse.x <= box_x + box_w - 6 &&
                mouse.y >= row_y && mouse.y < row_y + ROW_HEIGHT) {
                int item_idx = g_palette.scroll_offset + r;
                if (item_idx < g_palette.filtered_count) {
                    if (ctx.mouse_delta.x != 0.0f || ctx.mouse_delta.y != 0.0f) {
                        g_palette.selected_index = item_idx;
                    }
                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        g_palette.selected_index = item_idx;
                        execute_index(item_idx);
                        return;
                    }
                }
            }
        }
    }
}

// ── Text Rendering Helpers ──────────────────────────────────────────────────

static void palette_draw_text(const char *text, float x, float y, Color color) {
    if (ctx.current_font.handle != NULL) {
        i32 fs = ctx.current_font.baseSize ? ctx.current_font.baseSize : 16;
        draw_flush();
        DrawTextPro(ctx.current_font, text, V2f(x, y), V2f(0, 0), 0, (f32)fs, 0, color);
    } else if (ctx.fallback_atlas_pixels != NULL) {
        draw_text_fallback(text, x, y, 16.0f, color);
    }
}

static float palette_measure_text_w(const char *text) {
    if (ctx.current_font.handle != NULL) {
        i32 fs = ctx.current_font.baseSize ? ctx.current_font.baseSize : 16;
        return MeasureTextEx(ctx.current_font, text, (f32)fs, 0).x;
    }
    return (float)strlen(text) * 8.5f;
}

// ── Per-Frame Draw ──────────────────────────────────────────────────────────

void command_palette_draw(void) {
    if (!g_palette.is_open) return;

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();

    // 1. Semi-transparent backdrop overlay
    DrawRectangle(0, 0, screen_w, screen_h, (Color){0, 0, 0, 110});

    // 2. Palette Dimensions
    int box_w = screen_w - 40 < 540 ? screen_w - 40 : 540;
    if (box_w < 280) box_w = 280;
    int box_x = (screen_w - box_w) / 2;
    int box_y = screen_h * 0.12f > 24 ? (int)(screen_h * 0.12f) : 24;

    int visible_rows = g_palette.filtered_count < MAX_VISIBLE_ROWS ? g_palette.filtered_count : MAX_VISIBLE_ROWS;
    if (visible_rows == 0) visible_rows = 1;
    int list_h = visible_rows * ROW_HEIGHT + 6;
    int box_h = HEADER_HEIGHT + list_h + FOOTER_HEIGHT;

    // 3. Drop shadow & Window frame
    DrawRectangle(box_x - 1, box_y - 1, box_w + 2, box_h + 2, (Color){60, 66, 80, 255});
    DrawRectangle(box_x, box_y, box_w, box_h, (Color){20, 22, 28, 248});

    // 4. Header (Search input)
    DrawRectangle(box_x, box_y, box_w, HEADER_HEIGHT, (Color){28, 31, 40, 255});
    DrawRectangle(box_x, box_y + HEADER_HEIGHT - 1, box_w, 1, (Color){45, 50, 65, 255});

    // Header Prompt '>' Icon
    palette_draw_text(">", (float)(box_x + 14), (float)(box_y + 11), (Color){90, 175, 255, 255});

    // Search Query or Placeholder
    if (g_palette.query_len == 0) {
        palette_draw_text("Type a command...", (float)(box_x + 32), (float)(box_y + 11), (Color){110, 118, 136, 255});
    } else {
        palette_draw_text(g_palette.query, (float)(box_x + 32), (float)(box_y + 11), (Color){245, 248, 255, 255});
    }

    // Blinking cursor
    if (fmod(GetTime(), 0.9) < 0.5) {
        float q_width = g_palette.query_len > 0 ? palette_measure_text_w(g_palette.query) : 0.0f;
        float cur_x = (float)(box_x + 32) + q_width + 1.0f;
        DrawRectangle((int)cur_x, box_y + 10, 2, 18, (Color){120, 185, 255, 255});
    }

    // 5. Item Rows
    int list_start_y = box_y + HEADER_HEIGHT + 3;

    if (g_palette.filtered_count == 0) {
        // "No matching commands"
        palette_draw_text("No matching commands", (float)(box_x + 32), (float)(list_start_y + 6), (Color){120, 126, 142, 255});
    } else {
        for (int r = 0; r < visible_rows && (g_palette.scroll_offset + r) < g_palette.filtered_count; r++) {
            int item_idx = g_palette.scroll_offset + r;
            int cmd_idx = g_palette.filtered_indices[item_idx];
            int row_y = list_start_y + r * ROW_HEIGHT;
            bool is_selected = (item_idx == g_palette.selected_index);

            const char *cmd_name = flut_array[cmd_idx].key;
            char shortcut[64] = {0};
            command_palette_get_shortcut(cmd_name, shortcut, sizeof(shortcut));

            // Selection highlight background
            if (is_selected) {
                DrawRectangle(box_x + 5, row_y, box_w - 10, ROW_HEIGHT - 2, (Color){40, 80, 145, 235});
                DrawRectangle(box_x + 5, row_y, 3, ROW_HEIGHT - 2, (Color){100, 185, 255, 255});
            }

            // Command Name (LHS)
            Color name_color = is_selected ? (Color){255, 255, 255, 255} : (Color){215, 222, 235, 255};
            palette_draw_text(cmd_name, (float)(box_x + 18), (float)(row_y + 6), name_color);

            // Keybind Badge (RHS)
            if (shortcut[0] != '\0') {
                float sc_w = palette_measure_text_w(shortcut);
                int badge_pad = 6;
                int badge_w = (int)sc_w + badge_pad * 2;
                int badge_x = box_x + box_w - badge_w - 16;
                int badge_y = row_y + 4;
                int badge_h = ROW_HEIGHT - 10;

                Color badge_bg = is_selected ? (Color){26, 56, 105, 220} : (Color){32, 36, 48, 220};
                Color badge_border = is_selected ? (Color){60, 110, 190, 200} : (Color){50, 56, 72, 200};
                Color badge_text_col = is_selected ? (Color){215, 235, 255, 255} : (Color){150, 160, 180, 255};

                DrawRectangle(badge_x - 1, badge_y - 1, badge_w + 2, badge_h + 2, badge_border);
                DrawRectangle(badge_x, badge_y, badge_w, badge_h, badge_bg);

                palette_draw_text(shortcut, (float)(badge_x + badge_pad), (float)(row_y + 6), badge_text_col);
            }
        }

        // Scrollbar if needed
        if (g_palette.filtered_count > MAX_VISIBLE_ROWS) {
            int track_x = box_x + box_w - 6;
            int track_y = list_start_y;
            int track_h = list_h - 6;

            float thumb_ratio = (float)MAX_VISIBLE_ROWS / (float)g_palette.filtered_count;
            int thumb_h = (int)(track_h * thumb_ratio);
            if (thumb_h < 12) thumb_h = 12;

            int max_scroll = g_palette.filtered_count - MAX_VISIBLE_ROWS;
            float scroll_ratio = max_scroll > 0 ? (float)g_palette.scroll_offset / (float)max_scroll : 0.0f;
            int thumb_y = track_y + (int)((track_h - thumb_h) * scroll_ratio);

            DrawRectangle(track_x, track_y, 3, track_h, (Color){35, 39, 50, 160});
            DrawRectangle(track_x, thumb_y, 3, thumb_h, (Color){90, 105, 130, 220});
        }
    }

    // 6. Footer
    int footer_y = box_y + box_h - FOOTER_HEIGHT;
    DrawRectangle(box_x, footer_y, box_w, 1, (Color){40, 45, 58, 255});
    DrawRectangle(box_x, footer_y + 1, box_w, FOOTER_HEIGHT - 1, (Color){24, 26, 34, 255});

    palette_draw_text("Up/Down Navigate   Enter Select   Esc Close",
                      (float)(box_x + 14), (float)(footer_y + 5), (Color){110, 118, 135, 255});

    draw_flush();
}
