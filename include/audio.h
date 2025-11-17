#ifndef AUDIO_H
#define AUDIO_H

// Simple 16-bit mono background music generated in C and streamed
// to an external player (aplay). Audio failures degrade gracefully.

// Start background music. Returns 0 on success, non-zero on failure.
int audio_start(void);

// Generate and stream audio for the elapsed frame time (seconds).
// Safe to call even if audio_start failed (it will do nothing).
void audio_step(double dt);

// Stop background music and clean up any child process.
void audio_stop(void);

// Adjust master volume in the range [0, 1]. Positive delta increases
// volume, negative delta decreases it. Out-of-range is clamped.
void audio_adjust_volume(float delta);

// Get current master volume in [0, 1].
float audio_get_volume(void);

#endif // AUDIO_H
