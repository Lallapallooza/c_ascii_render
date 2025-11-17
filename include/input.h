#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

typedef struct {
    bool w_pressed;
    bool a_pressed;
    bool s_pressed;
    bool d_pressed;
    bool m_pressed;
    bool quit_requested;
    int volume_delta;
} InputState;

// Initialize input system (non-blocking mode)
int input_init(void);

// Poll for input with timeout_ms (0 for non-blocking)
void input_poll(InputState* state, int timeout_ms);

// Cleanup input system
void input_cleanup(void);

#endif // INPUT_H
