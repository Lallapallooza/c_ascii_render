#include "terminal.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>

int terminal_init(TerminalState* state) {
    if (tcgetattr(STDIN_FILENO, &state->orig_termios) == -1) {
        return -1;
    }

    terminal_get_size(&state->width, &state->height);
    terminal_hide_cursor();
    terminal_clear();

    // Enable basic mouse reporting (for scroll wheel volume control)
    // Use xterm button tracking + SGR extended coordinates.
    printf("\033[?1000h\033[?1006h");
    fflush(stdout);

    return 0;
}

void terminal_restore(TerminalState* state) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &state->orig_termios);
    terminal_show_cursor();
    // Disable mouse reporting
    printf("\033[?1000l\033[?1006l");
    fflush(stdout);
    terminal_clear();
}

void terminal_get_size(int* width, int* height) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *width = ws.ws_col;
        *height = ws.ws_row;
    } else {
        *width = 80;
        *height = 24;
    }
}

void terminal_clear(void) {
    printf("\033[2J");
    fflush(stdout);
}

void terminal_move_cursor(int x, int y) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

void terminal_hide_cursor(void) {
    printf("\033[?25l");
    fflush(stdout);
}

void terminal_show_cursor(void) {
    printf("\033[?25h");
    fflush(stdout);
}
