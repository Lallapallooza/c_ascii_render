#include "render.h"
#include "raymarch.h"
#include "sdf.h"
#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <stdio.h>
#include <math.h>

// Shading ramp from dark to bright
static const wchar_t SHADE_CHARS[] = L" ·⋅∙•∘○◌◍◎●◉⬤";
static const int SHADE_LEVELS = (int)(sizeof(SHADE_CHARS) / sizeof(wchar_t)) - 1;

// Subpixel sampling pattern (currently single sample at pixel center)
static const float SUBPIXEL_OFFSETS[][2] = {
    {0.5f, 0.5f}
};
static const int SUBPIXEL_SAMPLES = 1;

static unsigned int hash_u32(unsigned int v);
static float compute_soft_shadow(Vec3 point, Vec3 light_dir, float light_distance, CubeState* cube);
static float compute_ambient_occlusion(Vec3 point, Vec3 normal, CubeState* cube);
static bool detect_edge(Vec3 hit_point, CubeState* cube, Mat3 inv_rot);
static float sample_shading(Vec3 hit_point, Vec3 normal, Vec3 camera_pos, CubeState* cube, Light light);
static void render_environment_background(Framebuffer* fb, FrameStats stats);
static void render_rain_background(Framebuffer* fb, FrameStats stats);

// ANSI color codes
#define COLOR_NONE      0
#define COLOR_CUBE      1  // Cyan for cube
#define COLOR_GROUND    2  // Dark gray for ground
#define COLOR_MOUNTAIN  3  // Blue-gray for mountains
#define COLOR_BUILDING  4  // Yellow for buildings
#define COLOR_RAIN      5  // Bright cyan for rain
#define COLOR_SUN       6  // Bright yellow for sun
#define COLOR_FPS       7  // White for FPS

Framebuffer* framebuffer_create(int width, int height) {
    Framebuffer* fb = malloc(sizeof(Framebuffer));
    if (!fb) return NULL;

    fb->width = width;
    fb->height = height;
    fb->chars = calloc(width * height, sizeof(wchar_t));
    fb->depth = calloc(width * height, sizeof(float));
    fb->colors = calloc(width * height, sizeof(unsigned char));

    if (!fb->chars || !fb->depth || !fb->colors) {
        free(fb->chars);
        free(fb->depth);
        free(fb->colors);
        free(fb);
        return NULL;
    }

    return fb;
}

static unsigned int hash_u32(unsigned int v) {
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    v ^= v >> 16;
    return v;
}

void framebuffer_destroy(Framebuffer* fb) {
    if (fb) {
        free(fb->chars);
        free(fb->depth);
        free(fb->colors);
        free(fb);
    }
}

void framebuffer_clear(Framebuffer* fb) {
    for (int i = 0; i < fb->width * fb->height; i++) {
        fb->chars[i] = L' ';
        fb->depth[i] = 1000.0f;
        fb->colors[i] = COLOR_NONE;
    }
}

wchar_t intensity_to_char(float intensity, bool is_edge) {
    if (is_edge) {
        // Edge characters by intensity
        if (intensity > 0.8f) return L'◆';
        if (intensity > 0.6f) return L'◇';
        if (intensity > 0.4f) return L'◈';
        if (intensity > 0.2f) return L'◊';
        return L'◌';
    }

    // Map intensity to character index
    intensity = fmaxf(0.0f, fminf(1.0f, intensity));
    int idx = (int)(intensity * (SHADE_LEVELS - 1) + 0.5f);
    if (idx >= SHADE_LEVELS) idx = SHADE_LEVELS - 1;
    if (idx < 0) idx = 0;
    return SHADE_CHARS[idx];
}

static void render_environment_background(Framebuffer* fb, FrameStats stats) {
    (void)stats;
    int width = fb->width;
    int height = fb->height;

    if (width <= 0 || height <= 0) {
        return;
    }

    int horizon = (height * 2) / 3;
    if (horizon < 4) {
        horizon = height / 2;
    }

    // Ground plane below horizon with gradient pattern
    for (int y = horizon; y < height; y++) {
        float t = (float)(y - horizon) / (float)(height - horizon + 1);
        wchar_t ch;
        if (t < 0.2f) {
            ch = L'⋅';
        } else if (t < 0.4f) {
            ch = L'∙';
        } else if (t < 0.6f) {
            ch = L'•';
        } else if (t < 0.8f) {
            ch = L'◦';
        } else {
            ch = L'○';
        }

        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            fb->chars[idx] = ch;
            fb->depth[idx] = 1000.0f;
            fb->colors[idx] = COLOR_GROUND;
        }
    }

    // Mountain silhouettes on the horizon
    for (int x = 0; x < width; x++) {
        float xf = (float)x / (float)(width - 1);
        float m1 = 1.0f - fabsf(xf - 0.18f) / 0.22f;
        float m2 = 1.0f - fabsf(xf - 0.55f) / 0.25f;
        float m3 = 1.0f - fabsf(xf - 0.82f) / 0.18f;

        float m = fmaxf(0.0f, fmaxf(m1, fmaxf(m2, m3)));
        int peak = (int)(m * (float)(height / 4));
        if (peak <= 0) continue;

        int top_y = horizon - peak;
        if (top_y < 1) top_y = 1;

        for (int y = top_y; y < horizon; y++) {
            float band = (float)(horizon - y) / (float)peak;
            wchar_t ch;
            if (band > 0.8f) {
                ch = L'▲';  // Sharp peak
            } else if (band > 0.6f) {
                ch = L'△';  // Upper slopes
            } else if (band > 0.4f) {
                ch = L'⋀';  // Mid slopes
            } else if (band > 0.2f) {
                ch = L'∧';  // Lower slopes
            } else {
                ch = L'˄';  // Base
            }

            int idx = y * width + x;
            fb->chars[idx] = ch;
            fb->depth[idx] = 1000.0f;
            fb->colors[idx] = COLOR_MOUNTAIN;
        }
    }

    // Simple distant building silhouettes near the horizon
    int base_y = horizon - 1;
    if (base_y > 2) {
        int spacing = width / 8;
        if (spacing < 6) spacing = 6;

        for (int bx = spacing / 2; bx < width - spacing / 2; bx += spacing) {
            int b_width = 3 + (bx % 3);          // 3..5
            int b_height = height / 8 + (bx % 5); // modest height

            int left = bx - b_width / 2;
            int right = bx + b_width / 2;
            int top = base_y - b_height;
            if (top < 1) top = 1;

            for (int x = left; x <= right; x++) {
                if (x < 0 || x >= width) continue;
                for (int y = top; y <= base_y; y++) {
                    int idx = y * width + x;
                    wchar_t ch;
                    if (y == top || y == base_y || x == left || x == right) {
                        ch = L'█'; // solid outline
                    } else if (((x + y) & 1) == 0) {
                        ch = L'▪'; // lit window
                    } else {
                        ch = L'·'; // dark area
                    }
                    fb->chars[idx] = ch;
                    fb->depth[idx] = 1000.0f;
                    fb->colors[idx] = COLOR_BUILDING;
                }
            }
        }
    }
}

static void render_rain_background(Framebuffer* fb, FrameStats stats) {
    int width = fb->width;
    int height = fb->height;

    if (width <= 0 || height <= 1) {
        return;
    }

    unsigned long t = stats.frame_count;
    unsigned long slow_t = t / 3; // bigger divisor => slower rain

    for (int x = 0; x < width; x++) {
        // Leave space on the right for the FPS overlay
        if (x > width - 16) {
            continue;
        }

        unsigned int h = hash_u32((unsigned int)x * 2654435761U);

        // Roughly 70% of columns have rain at all
        if ((h & 0x3U) == 0U) {
            continue;
        }

        int speed = 1 + (int)(h % 3U);                  // 1..3
        int trail_length = 3 + (int)((h >> 3) % 6U);    // 3..8
        int gap = 4 + (int)((h >> 6) % 8U);             // vertical gap between streaks

        unsigned long phase = (unsigned long)(h >> 16);
        unsigned long cycle = (unsigned long)(height + trail_length + gap);
        int head = (int)((slow_t * (unsigned long)speed + phase) % cycle);

        // Only draw when the head is actually on-screen
        if (head >= height + trail_length) {
            continue;
        }

        int horizon = (height * 2) / 3;
        for (int i = 0; i < trail_length; i++) {
            int y = head - i;
            if (y <= 0 || y >= height) {
                continue;
            }
            // Do not draw rain on the ground plane to keep it readable
            if (y >= horizon) {
                continue;
            }

            int idx = y * width + x;
            wchar_t ch;
            if (i == 0) {
                ch = L'╿';  // Rain head
            } else if (i == 1) {
                ch = L'│';  // Upper trail
            } else if (i < trail_length - 2) {
                ch = L'┆';  // Mid trail
            } else if (i == trail_length - 2) {
                ch = L'╎';  // Lower trail
            } else {
                ch = L'˙';  // Tail
            }

            fb->chars[idx] = ch;
            fb->depth[idx] = 1000.0f;
            fb->colors[idx] = COLOR_RAIN;
        }
    }
}

static float compute_soft_shadow(Vec3 point, Vec3 light_dir, float light_distance, CubeState* cube) {
    float shadow = 1.0f;
    float t = 0.02f;
    for (int i = 0; i < 16 && t < light_distance; i++) {
        Vec3 sample = vec3_add(point, vec3_multiply(light_dir, t));
        float dist = sdf_cube(sample, cube->position, cube->size, cube->rotation);
        if (dist < 0.0005f) {
            return 0.0f;
        }
        shadow = fminf(shadow, 4.0f * dist / t);
        t += fmaxf(dist, 0.03f);
    }
    return fmaxf(shadow, 0.0f);
}

static float compute_ambient_occlusion(Vec3 point, Vec3 normal, CubeState* cube) {
    const int AO_STEPS = 5;
    const float AO_STEP = fmaxf(0.03f, cube->size * 0.12f);

    float occlusion = 0.0f;
    float max_component = 0.0f;

    for (int i = 1; i <= AO_STEPS; i++) {
        float sample_dist = AO_STEP * i;
        Vec3 sample_point = vec3_add(point, vec3_multiply(normal, sample_dist));
        float dist = sdf_cube(sample_point, cube->position, cube->size, cube->rotation);
        float contribution = fmaxf(0.0f, sample_dist - dist) / (float)i;
        occlusion += contribution;
        max_component += AO_STEP / (float)i;
    }

    if (max_component <= 0.0f) {
        return 1.0f;
    }

    float ao = 1.0f - fminf(1.0f, (occlusion * 1.1f) / max_component);
    return ao;
}

static bool detect_edge(Vec3 hit_point, CubeState* cube, Mat3 inv_rot) {
    Vec3 local_point = vec3_subtract(hit_point, cube->position);
    local_point = mat3_multiply_vec3(inv_rot, local_point);

    float edge_dist = fmaxf(0.02f, cube->size * 0.08f);
    int near_boundary = 0;

    if (fabsf(fabsf(local_point.x) - cube->size) < edge_dist) near_boundary++;
    if (fabsf(fabsf(local_point.y) - cube->size) < edge_dist) near_boundary++;
    if (fabsf(fabsf(local_point.z) - cube->size) < edge_dist) near_boundary++;

    return near_boundary >= 2;
}

static float sample_shading(Vec3 hit_point, Vec3 normal, Vec3 camera_pos, CubeState* cube, Light light) {
    Vec3 to_light = vec3_subtract(light.position, hit_point);
    float light_distance = vec3_length(to_light);
    if (light_distance < 0.0001f) {
        light_distance = 0.0001f;
    }
    Vec3 light_dir = vec3_multiply(to_light, 1.0f / light_distance);

    float diffuse = fmaxf(0.0f, vec3_dot(normal, light_dir));

    Vec3 view_dir = vec3_normalize(vec3_subtract(camera_pos, hit_point));
    float specular_term = 0.0f;
    if (diffuse > 0.0f && light.specular > 0.0f) {
        Vec3 reflect_dir = vec3_reflect(vec3_multiply(light_dir, -1.0f), normal);
        specular_term = powf(fmaxf(vec3_dot(reflect_dir, view_dir), 0.0f), 32.0f);
    }

    Vec3 shadow_origin = vec3_add(hit_point, vec3_multiply(normal, 0.015f));
    float shadow = compute_soft_shadow(shadow_origin, light_dir, light_distance, cube);
    float ambient_occlusion = compute_ambient_occlusion(hit_point, normal, cube);

    float effective_ambient = light.ambient * 0.8f;
    float diffuse_spec = light.diffuse * diffuse + light.specular * specular_term;

    float ambient_term = effective_ambient * (0.3f + 0.7f * ambient_occlusion);
    float direct_term = shadow * ambient_occlusion * diffuse_spec;

    float intensity = ambient_term + direct_term;

    intensity = fmaxf(0.0f, fminf(1.0f, intensity));
    return powf(intensity, 1.1f);
}

void render_cube(Framebuffer* fb, CubeState* cube, Light light, FrameStats stats) {
    framebuffer_clear(fb);

    render_environment_background(fb, stats);
    render_rain_background(fb, stats);

    Vec3 camera_pos = {0, 0, 6.0f};
    float aspect = (float)fb->width / (float)fb->height * 0.5f;
    float fov = 50.0f * 3.14159f / 180.0f;
    float half_fov = fov * 0.5f;
    float scale = tanf(half_fov);
    float inv_width = 1.0f / (float)fb->width;
    float inv_height = 1.0f / (float)fb->height;

    RaymarchConfig raymarch_config = {
        .max_steps = 100,
        .epsilon = 0.001f,
        .max_distance = 100.0f
    };

    Mat3 inv_rot = mat3_transpose(cube->rotation);

    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            float accumulated_intensity = 0.0f;
            int samples_hit = 0;
            int edge_votes = 0;
            float nearest_depth = 1000.0f;

            for (int sample = 0; sample < SUBPIXEL_SAMPLES; sample++) {
                float offset_x = SUBPIXEL_OFFSETS[sample][0];
                float offset_y = SUBPIXEL_OFFSETS[sample][1];

                float px = (2.0f * ((x + offset_x) * inv_width) - 1.0f) * aspect;
                float py = 1.0f - 2.0f * ((y + offset_y) * inv_height);

                Vec3 ray_dir = vec3_normalize((Vec3){
                    px * scale,
                    py * scale,
                    -1.0f
                });

                Vec3 hit_point, normal;
                if (raymarch(camera_pos, ray_dir, raymarch_config,
                             &hit_point, &normal, cube->position,
                             cube->size, cube->rotation)) {
                    float sample_intensity = sample_shading(hit_point, normal, camera_pos, cube, light);
                    accumulated_intensity += sample_intensity;
                    samples_hit++;

                    if (detect_edge(hit_point, cube, inv_rot)) {
                        edge_votes++;
                    }

                    float depth = vec3_length(vec3_subtract(hit_point, camera_pos));
                    if (depth < nearest_depth) {
                        nearest_depth = depth;
                    }
                }
            }

            int idx = y * fb->width + x;
            if (samples_hit > 0) {
                float final_intensity = accumulated_intensity / (float)samples_hit;
                // Depth-based falloff: farther points get dimmer
                float depth_near = 3.5f;
                float depth_far = 9.5f;
                float depth_n = (nearest_depth - depth_near) / (depth_far - depth_near);
                if (depth_n < 0.0f) depth_n = 0.0f;
                if (depth_n > 1.0f) depth_n = 1.0f;
                float fog = 1.0f - 0.35f * depth_n;
                final_intensity *= fog;

                bool is_edge = edge_votes >= (samples_hit + 1) / 2;
                fb->chars[idx] = intensity_to_char(final_intensity, is_edge);
                fb->depth[idx] = nearest_depth;
                fb->colors[idx] = COLOR_CUBE;
            }
        }
    }
    // Draw sun to indicate light direction
    if (fb->width > 8 && fb->height > 4) {
        // Direction from cube center to light
        Vec3 to_light_dir = vec3_normalize(vec3_subtract(light.position, cube->position));

        // Map light direction (x,y) to a point around the cube on screen
        float ux = to_light_dir.x;
        float uy = to_light_dir.y;

        float max_offset_x = (float)fb->width * 0.8f;
        float max_offset_y = (float)fb->height * 0.8f;

        int cx = fb->width / 2 + (int)(ux * max_offset_x);
        int cy = fb->height / 2 - (int)(uy * max_offset_y);

        // Clamp inside screen
        if (cx < 2) cx = 2;
        if (cx > fb->width - 3) cx = fb->width - 3;
        if (cy < 1) cy = 1;
        if (cy > fb->height - 2) cy = fb->height - 2;

        int min_dim = fb->height < fb->width ? fb->height : fb->width;
        int radius = min_dim / 10;
        if (radius < 3) radius = 3;
        if (radius > 8) radius = 8;

        for (int y = cy - radius; y <= cy + radius; y++) {
            if (y < 0 || y >= fb->height) continue;
            for (int x = cx - radius; x <= cx + radius; x++) {
                if (x < 0 || x >= fb->width) continue;

                float dx = (float)(x - cx);
                float dy = (float)(y - cy);
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > (float)radius) {
                    continue;
                }

                float r = dist / (float)radius;
                wchar_t ch;
                if (r < 0.15f) {
                    ch = L'⬤';  // Bright center
                } else if (r < 0.35f) {
                    ch = L'●';  // Inner glow
                } else if (r < 0.55f) {
                    ch = L'◉';  // Mid glow
                } else if (r < 0.75f) {
                    ch = L'◎';  // Outer glow
                } else if (r < 0.9f) {
                    ch = L'○';  // Edge
                } else {
                    ch = L'◦';  // Halo
                }

                int idx = y * fb->width + x;
                fb->chars[idx] = ch;
                fb->depth[idx] = 1000.0f;
                fb->colors[idx] = COLOR_SUN;
            }
        }
    }

    // FPS counter with box frame and volume
    char fps_str[32];
    snprintf(fps_str, sizeof(fps_str), "%.1f", stats.fps);
    int fps_len = strlen(fps_str);

    int box_width = fps_len + 6;  // "FPS: " + value + padding
    if (box_width < 30) {
        box_width = 30;           // Ensure enough room for controls text
    }
    int fps_x = fb->width - box_width - 1;
    int fps_y = 0;

    if (fps_x >= 0 && box_width < fb->width && fb->height >= 7) {
        // Top border
        fb->chars[fps_y * fb->width + fps_x] = L'╭';
        fb->colors[fps_y * fb->width + fps_x] = COLOR_FPS;
        for (int i = 1; i < box_width - 1; i++) {
            fb->chars[fps_y * fb->width + fps_x + i] = L'─';
            fb->colors[fps_y * fb->width + fps_x + i] = COLOR_FPS;
        }
        fb->chars[fps_y * fb->width + fps_x + box_width - 1] = L'╮';
        fb->colors[fps_y * fb->width + fps_x + box_width - 1] = COLOR_FPS;

        // FPS content line
        fps_y = 1;
        fb->chars[fps_y * fb->width + fps_x] = L'│';
        fb->colors[fps_y * fb->width + fps_x] = COLOR_FPS;
        fb->chars[fps_y * fb->width + fps_x + 1] = L' ';
        fb->colors[fps_y * fb->width + fps_x + 1] = COLOR_FPS;

        // "FPS: XX.X"
        const char* fps_label = "FPS:";
        for (int i = 0; fps_label[i]; i++) {
            fb->chars[fps_y * fb->width + fps_x + 2 + i] = fps_label[i];
            fb->colors[fps_y * fb->width + fps_x + 2 + i] = COLOR_FPS;
        }
        for (int i = 0; fps_str[i]; i++) {
            fb->chars[fps_y * fb->width + fps_x + 6 + i] = fps_str[i];
            fb->colors[fps_y * fb->width + fps_x + 6 + i] = COLOR_FPS;
        }
        fb->chars[fps_y * fb->width + fps_x + box_width - 1] = L'│';
        fb->colors[fps_y * fb->width + fps_x + box_width - 1] = COLOR_FPS;

        // Volume content line directly under FPS
        int vol_y = 2;
        fb->chars[vol_y * fb->width + fps_x] = L'│';
        fb->colors[vol_y * fb->width + fps_x] = COLOR_FPS;

        char vol_str[16];
        int vol_percent = (int)(audio_get_volume() * 100.0f + 0.5f);
        if (vol_percent < 0) vol_percent = 0;
        if (vol_percent > 100) vol_percent = 100;
        snprintf(vol_str, sizeof(vol_str), "VOL:%3d%%", vol_percent);

        for (int i = 0; i < box_width - 2; i++) {
            wchar_t ch = L' ';
            if (i < (int)strlen(vol_str)) {
                ch = (wchar_t)vol_str[i];
            }
            fb->chars[vol_y * fb->width + fps_x + 1 + i] = ch;
            fb->colors[vol_y * fb->width + fps_x + 1 + i] = COLOR_FPS;
        }
        fb->chars[vol_y * fb->width + fps_x + box_width - 1] = L'│';
        fb->colors[vol_y * fb->width + fps_x + box_width - 1] = COLOR_FPS;

        // Controls line 1
        int ctl_y1 = 3;
        const char* ctl1 = "WASD: rotate   M: orbit";
        fb->chars[ctl_y1 * fb->width + fps_x] = L'│';
        fb->colors[ctl_y1 * fb->width + fps_x] = COLOR_FPS;
        for (int i = 0; i < box_width - 2; i++) {
            wchar_t ch = L' ';
            if (i < (int)strlen(ctl1)) {
                ch = (wchar_t)ctl1[i];
            }
            fb->chars[ctl_y1 * fb->width + fps_x + 1 + i] = ch;
            fb->colors[ctl_y1 * fb->width + fps_x + 1 + i] = COLOR_FPS;
        }
        fb->chars[ctl_y1 * fb->width + fps_x + box_width - 1] = L'│';
        fb->colors[ctl_y1 * fb->width + fps_x + box_width - 1] = COLOR_FPS;

        // Controls line 2
        int ctl_y2 = 4;
        const char* ctl2 = "Scroll/+/-: volume   Q: quit";
        fb->chars[ctl_y2 * fb->width + fps_x] = L'│';
        fb->colors[ctl_y2 * fb->width + fps_x] = COLOR_FPS;
        for (int i = 0; i < box_width - 2; i++) {
            wchar_t ch = L' ';
            if (i < (int)strlen(ctl2)) {
                ch = (wchar_t)ctl2[i];
            }
            fb->chars[ctl_y2 * fb->width + fps_x + 1 + i] = ch;
            fb->colors[ctl_y2 * fb->width + fps_x + 1 + i] = COLOR_FPS;
        }
        fb->chars[ctl_y2 * fb->width + fps_x + box_width - 1] = L'│';
        fb->colors[ctl_y2 * fb->width + fps_x + box_width - 1] = COLOR_FPS;

        // Bottom border
        fps_y = 5;
        fb->chars[fps_y * fb->width + fps_x] = L'╰';
        fb->colors[fps_y * fb->width + fps_x] = COLOR_FPS;
        for (int i = 1; i < box_width - 1; i++) {
            fb->chars[fps_y * fb->width + fps_x + i] = L'─';
            fb->colors[fps_y * fb->width + fps_x + i] = COLOR_FPS;
        }
        fb->chars[fps_y * fb->width + fps_x + box_width - 1] = L'╯';
        fb->colors[fps_y * fb->width + fps_x + box_width - 1] = COLOR_FPS;
    }
}

void framebuffer_display(Framebuffer* fb) {
    static const char* color_codes[] = {
        "\033[0m",          // COLOR_NONE - reset
        "\033[96m",         // COLOR_CUBE - bright cyan
        "\033[38;5;240m",   // COLOR_GROUND - dark gray
        "\033[38;5;67m",    // COLOR_MOUNTAIN - blue-gray
        "\033[93m",         // COLOR_BUILDING - bright yellow
        "\033[36m",         // COLOR_RAIN - cyan
        "\033[38;5;226m",   // COLOR_SUN - bright yellow/gold
        "\033[97m"          // COLOR_FPS - bright white
    };

    // Move to home position
    printf("\033[H");

    // Output entire framebuffer with colors in one pass
    unsigned char current_color = 255;  // Invalid initial color
    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            int idx = y * fb->width + x;
            unsigned char color = fb->colors[idx];
            wchar_t c = fb->chars[idx];

            // Only change color if needed
            if (color != current_color) {
                printf("%s", color_codes[color]);
                current_color = color;
            }
            printf("%lc", c);
        }
        if (y < fb->height - 1) {
            printf("\n");
        }
    }

    // Reset color at the end
    printf("\033[0m");
    fflush(stdout);
}
