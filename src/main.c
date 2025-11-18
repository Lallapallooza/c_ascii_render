#define _POSIX_C_SOURCE 199309L

#include "main.h"
#include "vec3.h"
#include "matrix.h"
#include "physics.h"
#include "terminal.h"
#include "render.h"
#include "input.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <locale.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>

static volatile int resize_flag = 0;
static TerminalState term_state;

void sigwinch_handler(int sig) {
    (void)sig;
    resize_flag = 1;
}

void sigint_handler(int sig) {
    (void)sig;
    terminal_restore(&term_state);
    terminal_show_cursor();
    input_cleanup();
    exit(0);
}

int parse_args(int argc, char** argv, Config* config) {
    // Set defaults
    config->cube_size = 1.0f;
    config->rotation_speed = 1.0f;
    // Default light in front, high and to the left
    config->light_x = -3.0f;
    config->light_y = 4.5f;
    config->light_z = 4.0f;
    config->max_raymarch_steps = 100;

    struct option long_options[] = {
        {"size", required_argument, 0, 's'},
        {"speed", required_argument, 0, 'r'},
        {"light-x", required_argument, 0, 'x'},
        {"light-y", required_argument, 0, 'y'},
        {"light-z", required_argument, 0, 'z'},
        {"max-steps", required_argument, 0, 'm'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "s:r:x:y:z:m:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 's':
                config->cube_size = atof(optarg);
                break;
            case 'r':
                config->rotation_speed = atof(optarg);
                break;
            case 'x':
                config->light_x = atof(optarg);
                break;
            case 'y':
                config->light_y = atof(optarg);
                break;
            case 'z':
                config->light_z = atof(optarg);
                break;
            case 'm':
                config->max_raymarch_steps = atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return 2;
        }
    }

    return 0;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nRaytraced ASCII Cube - Interactive 3D rendering in your terminal\n\n");
    printf("Controls:\n");
    printf("  W/S    - Rotate around X axis\n");
    printf("  A/D    - Rotate around Y axis\n");
    printf("  M      - Toggle motion mode (fly in circular path for depth effect)\n");
    printf("  Q/ESC  - Quit\n\n");
    printf("Options:\n");
    printf("  --size FLOAT          Cube half-extent (default: 1.0)\n");
    printf("  --speed FLOAT         Base rotation speed multiplier (default: 1.0)\n");
    printf("  --light-x FLOAT       Light X position (default: -3.0)\n");
    printf("  --light-y FLOAT       Light Y position (default: 4.5)\n");
    printf("  --light-z FLOAT       Light Z position (default: 4.0)\n");
    printf("  --max-steps INT       Maximum raymarching iterations (default: 100)\n");
    printf("  --help                Show this help message\n");
}

static double get_time_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "");

    Config config;
    if (parse_args(argc, argv, &config) != 0) {
        return 2;
    }

    // Initialize terminal
    if (terminal_init(&term_state) != 0) {
        fprintf(stderr, "Failed to initialize terminal\n");
        return 1;
    }

    // Initialize input
    if (input_init() != 0) {
        fprintf(stderr, "Failed to initialize input\n");
        terminal_restore(&term_state);
        return 1;
    }

    // Start background audio (best-effort; ignore failure)
    audio_start();

    // Setup signal handlers
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    signal(SIGWINCH, sigwinch_handler);

    // Get terminal size and create framebuffer
    int term_width, term_height;
    terminal_get_size(&term_width, &term_height);

    Framebuffer* fb = framebuffer_create(term_width, term_height - 1);  // -1 for status line
    if (!fb) {
        fprintf(stderr, "Failed to create framebuffer\n");
        terminal_restore(&term_state);
        input_cleanup();
        return 3;
    }

    // Initialize cube state
    CubeState cube = {
        // Initial tilt
        .rotation = mat3_multiply(mat3_rotate_y(0.6f), mat3_rotate_x(-0.4f)),
        .angular_velocity = {0.25f, 0.35f, 0.10f},
        .position = {0, 0, 0},
        .size = config.cube_size,
        .motion_mode = false,
        .motion_phase = 0.0f
    };

    // Physics configuration
    PhysicsConfig physics_config = {
        .acceleration = 9.0f * config.rotation_speed,
        .damping = 0.97f,
        .max_velocity = 20.0f * config.rotation_speed
    };

    // Light setup
    Light light = {
        .position = {config.light_x, config.light_y, config.light_z},
        .ambient = 0.2f,
        .diffuse = 0.8f,
        .specular = 0.5f
    };

    // Input state
    InputState input = {0};

    // Frame timing
    const double target_fps = 60.0;
    const double target_frame_time = 1.0 / target_fps;
    double last_frame_time = get_time_seconds();
    double fps_smooth = 60.0;
    unsigned long frame_count = 0;

    // Main loop
    while (!input.quit_requested) {
        double frame_start = get_time_seconds();

        // Handle resize
        if (resize_flag) {
            resize_flag = 0;
            terminal_get_size(&term_width, &term_height);
            framebuffer_destroy(fb);
            fb = framebuffer_create(term_width, term_height - 1);
            if (!fb) {
                fprintf(stderr, "Failed to reallocate framebuffer\n");
                break;
            }
        }

        // Poll input
        input_poll(&input, 0);

        // Apply audio volume changes (from scroll wheel or +/- keys)
        if (input.volume_delta != 0) {
            audio_adjust_volume((float)input.volume_delta * 0.01f);
        }

        // Update physics
        float dt = (float)(frame_start - last_frame_time);
        dt = dt > 0.1f ? 0.1f : dt;  // Clamp dt
        physics_step(&cube, input, physics_config, dt);

        // Advance background music in lock-step with frame time
        audio_step(frame_start - last_frame_time);

        // Prepare frame stats
        FrameStats stats = {
            .frame_time_ms = (float)((frame_start - last_frame_time) * 1000.0),
            .fps = (float)fps_smooth,
            .frame_count = frame_count
        };

        // Render
        render_cube(fb, &cube, light, stats);
        framebuffer_display(fb);

        // Frame timing
        double frame_end = get_time_seconds();
        double frame_duration = frame_end - frame_start;

        // Sleep to target 60 FPS
        if (frame_duration < target_frame_time) {
            struct timespec sleep_time;
            double sleep_duration = target_frame_time - frame_duration;
            sleep_time.tv_sec = (time_t)sleep_duration;
            sleep_time.tv_nsec = (long)((sleep_duration - sleep_time.tv_sec) * 1000000000.0);
            nanosleep(&sleep_time, NULL);
        }

        double actual_frame_time = get_time_seconds() - frame_start;
        double current_fps = 1.0 / actual_frame_time;

        // Smooth FPS
        fps_smooth = fps_smooth * 0.9 + current_fps * 0.1;

        last_frame_time = frame_start;
        frame_count++;
    }

    // Cleanup
    framebuffer_destroy(fb);
    terminal_restore(&term_state);
    terminal_show_cursor();
    input_cleanup();
    audio_stop();

    return 0;
}
