#include "input.h"
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <fcntl.h>

static struct termios orig_termios;

int input_init(void) {
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
        return -1;
    }

    raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        return -1;
    }

    // Set non-blocking
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

void input_poll(InputState* state, int timeout_ms) {
    (void)timeout_ms;
    char c;

    // Reset per-frame state; inertia is handled in physics, not here.
    state->w_pressed = false;
    state->a_pressed = false;
    state->s_pressed = false;
    state->d_pressed = false;
    state->m_pressed = false;
    state->volume_delta = 0;

    // Read all available characters, last one wins.
    while (read(STDIN_FILENO, &c, 1) == 1) {
        switch (c) {
            case 'w':
            case 'W':
                state->w_pressed = true;
                state->a_pressed = false;
                state->s_pressed = false;
                state->d_pressed = false;
                break;
            case 'a':
            case 'A':
                state->w_pressed = false;
                state->a_pressed = true;
                state->s_pressed = false;
                state->d_pressed = false;
                break;
            case 's':
            case 'S':
                state->w_pressed = false;
                state->a_pressed = false;
                state->s_pressed = true;
                state->d_pressed = false;
                break;
            case 'd':
            case 'D':
                state->w_pressed = false;
                state->a_pressed = false;
                state->s_pressed = false;
                state->d_pressed = true;
                break;
            case '+':
            case '=':
                state->volume_delta += 1;
                break;
            case '-':
            case '_':
                state->volume_delta -= 1;
                break;
            case 'm':
            case 'M':
                state->m_pressed = true;
                break;
            case 'q':
            case 'Q':
                state->quit_requested = true;
                break;
            case 27: {  // ESC or escape sequence (e.g., mouse/scroll)
                char buf[32];
                int n = 0;
                // Collect a small escape sequence chunk
                while (n < (int)sizeof(buf) - 1) {
                    char ch;
                    ssize_t r = read(STDIN_FILENO, &ch, 1);
                    if (r != 1) break;
                    buf[n++] = ch;
                    if (ch == 'M' || ch == 'm' || ch == '~') break;
                }
                buf[n] = '\0';

                // SGR mouse: ESC [ < btn ; x ; y M
                if (n >= 6 && buf[0] == '[' && buf[1] == '<') {
                    int btn = 0;
                    int i = 2;
                    while (i < n && buf[i] >= '0' && buf[i] <= '9') {
                        btn = btn * 10 + (buf[i] - '0');
                        i++;
                    }
                    // 64: wheel up, 65: wheel down
                    if (btn == 64) {
                        state->volume_delta += 1;
                    } else if (btn == 65) {
                        state->volume_delta -= 1;
                    }
                } else if (n >= 3 && buf[0] == '[' && buf[2] == '~') {
                    // Page Up / Page Down fallback: ESC[5~ / ESC[6~
                    if (buf[1] == '5') {
                        state->volume_delta += 1;
                    } else if (buf[1] == '6') {
                        state->volume_delta -= 1;
                    }
                } else if (n == 0) {
                    // Bare ESC (no sequence): treat as quit
                    state->quit_requested = true;
                }
                break;
            }
        }
    }
}

void input_cleanup(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
