#ifndef AUDIO_H
#define AUDIO_H

// 16-bit mono background music streamed to aplay.

// Returns 0 on success, non-zero on failure.
int audio_start(void);

// Generate and stream audio for elapsed time (seconds). No-op if audio disabled.
void audio_step(double dt);

// Stop background music and clean up any child process.
void audio_stop(void);

// Adjust volume by delta. Clamped to [0, 1].
void audio_adjust_volume(float delta);

// Get current master volume in [0, 1].
float audio_get_volume(void);

#endif // AUDIO_H
