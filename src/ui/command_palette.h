#ifndef COMMAND_PALETTE_H
#define COMMAND_PALETTE_H

#include "core/dialect.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize or reset command palette state
void command_palette_init(void);

// Open the command palette
void command_palette_open(void);

// Close the command palette
void command_palette_close(void);

// Toggle the command palette open/closed state
void command_palette_toggle(void);

// Check if the command palette is currently open
bool command_palette_is_open(void);

// Handle character input from window message pump (WM_CHAR)
void command_palette_on_char(char c);

// Per-frame update (processes keyboard navigation, selection, mouse hover/click, scrolling)
void command_palette_update(void);

// Per-frame draw (renders backdrop, palette box, search input, scrollable results, shortcuts)
void command_palette_draw(void);

// Format human-readable shortcut string for a flut function name from actions_array
void command_palette_get_shortcut(const char *func_name, char *buf, size_t max_len);

// flut command callback
void Toggle_Command_Palette(void *arg);

#ifdef __cplusplus
}
#endif

#endif // COMMAND_PALETTE_H
