#include "audio.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// Very small, self-contained "16-bit music" generator.
// We stream signed 16-bit PCM to `aplay` via a pipe. If `aplay` is not
// available or the pipe fails, we simply disable audio.

#define AUDIO_SAMPLE_RATE 44100.0
#define TWO_PI 6.28318530717958647692

static int audio_fd = -1;
static pid_t audio_pid = -1;
static double audio_time = 0.0;
static bool audio_enabled = false;
static float audio_volume = 0.4f;

static float square_wave(double t, float freq) {
    double phase = fmod(t * freq, 1.0);
    return phase < 0.5 ? 1.0f : -1.0f;
}

static float lofi_track_sample(double t) {
    // Simple looping 2-bar chiptune-ish pattern at 120 BPM
    const double bpm = 120.0;
    const double beat_len = 60.0 / bpm;      // seconds per beat
    const double step_len = beat_len / 2.0;  // 8th notes
    int step = (int)(t / step_len) % 16;

    // Very small melodic pattern (A minor-ish)
    static const float melody_freqs[16] = {
        440.0f, 440.0f, 523.25f, 493.88f,
        440.0f, 440.0f, 659.25f, 587.33f,
        440.0f, 440.0f, 523.25f, 493.88f,
        440.0f, 659.25f, 587.33f, 523.25f
    };

    // Bass hits once per beat
    int bass_step = (int)(t / beat_len) % 8;
    static const float bass_freqs[8] = {
        110.0f, 110.0f, 82.41f, 82.41f,
        98.0f, 98.0f, 82.41f, 82.41f
    };

    float melody = 0.25f * square_wave(t, melody_freqs[step]);
    float bass = 0.20f * square_wave(t, bass_freqs[bass_step]);

    // Simple hi-hat using noisy triangle
    double hat_phase = fmod(t / step_len, 1.0);
    float hat = 0.0f;
    if (hat_phase < 0.25) {
        // Short burst at each 8th-note edge
        double n = t * 8000.0;
        // Cheap pseudo-noise from fractional part
        double frac = n - floor(n);
        hat = (float)((frac * 2.0) - 1.0) * 0.08f;
    }

    float sample = melody + bass + hat;
    // Gentle soft clipping
    if (sample > 0.9f) sample = 0.9f;
    if (sample < -0.9f) sample = -0.9f;
    return sample * audio_volume;
}

int audio_start(void) {
    if (audio_enabled) {
        return 0;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return -1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        // Child: hook pipe read-end to stdin and exec aplay.
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execlp("aplay",
               "aplay",
               "-q",                // quiet
               "-f", "S16_LE",      // 16-bit signed little endian
               "-c", "1",           // mono
               "-r", "44100",       // sample rate
               "-t", "raw",         // raw stream
               "-",                 // stdin
               (char*)NULL);

        // If exec fails, just exit silently so we don't spam the terminal.
        _exit(0);
    }

    // Parent
    close(pipefd[0]);
    audio_fd = pipefd[1];
    audio_pid = pid;
    audio_time = 0.0;
    audio_enabled = true;
    return 0;
}

void audio_step(double dt) {
    if (!audio_enabled || audio_fd < 0) {
        return;
    }

    if (dt <= 0.0) {
        return;
    }

    // Generate a chunk of samples matching the elapsed frame time.
    int total_samples = (int)(dt * AUDIO_SAMPLE_RATE);
    if (total_samples <= 0) {
        return;
    }

    // Limit chunk size for safety
    if (total_samples > 4096) {
        total_samples = 4096;
    }

    int16_t buffer[4096];

    for (int i = 0; i < total_samples; i++) {
        double t = audio_time + (double)i / AUDIO_SAMPLE_RATE;
        float s = lofi_track_sample(t);
        int16_t v = (int16_t)(s * 32767.0f);
        buffer[i] = v;
    }

    ssize_t bytes = write(audio_fd, buffer, (size_t)(total_samples * (int)sizeof(int16_t)));
    (void)bytes; // Ignore short writes; aplay will simply stop if pipe closes.

    audio_time += (double)total_samples / AUDIO_SAMPLE_RATE;

    // Keep time bounded to avoid floating point drift over very long runs
    if (audio_time > 60.0) {
        audio_time -= 60.0;
    }
}

void audio_stop(void) {
    if (!audio_enabled) {
        return;
    }

    if (audio_fd >= 0) {
        close(audio_fd);
        audio_fd = -1;
    }

    if (audio_pid > 0) {
        // Reap child if it is still running.
        int status;
        waitpid(audio_pid, &status, WNOHANG);
        audio_pid = -1;
    }

    audio_enabled = false;
}

void audio_adjust_volume(float delta) {
    if (!audio_enabled && delta == 0.0f) {
        return;
    }

    audio_volume += delta;
    if (audio_volume < 0.0f) {
        audio_volume = 0.0f;
    } else if (audio_volume > 1.0f) {
        audio_volume = 1.0f;
    }
}

float audio_get_volume(void) {
    if (audio_volume < 0.0f) return 0.0f;
    if (audio_volume > 1.0f) return 1.0f;
    return audio_volume;
}
