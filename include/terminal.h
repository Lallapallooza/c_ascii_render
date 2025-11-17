#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>

typedef struct {
    struct termios orig_termios;
    int width;
    int height;
} TerminalState;

// Initialize terminal (raw mode, hide cursor)
int terminal_init(TerminalState* state);

// Restore terminal to original state
void terminal_restore(TerminalState* state);

// Get terminal dimensions
void terminal_get_size(int* width, int* height);

// Clear screen
void terminal_clear(void);

// Move cursor to position
void terminal_move_cursor(int x, int y);

// Hide cursor
void terminal_hide_cursor(void);

// Show cursor
void terminal_show_cursor(void);

#endif // TERMINAL_H
