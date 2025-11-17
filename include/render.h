#ifndef RENDER_H
#define RENDER_H

#include "vec3.h"
#include "matrix.h"
#include "physics.h"
#include <wchar.h>

typedef struct {
    Vec3 position;
    float ambient;
    float diffuse;
    float specular;
} Light;

typedef struct {
    int width;
    int height;
    wchar_t* chars;
    float* depth;
    unsigned char* colors;  // Color codes for each character
} Framebuffer;

typedef struct {
    float frame_time_ms;
    float fps;
    unsigned long frame_count;
} FrameStats;

// Create/destroy framebuffer
Framebuffer* framebuffer_create(int width, int height);
void framebuffer_destroy(Framebuffer* fb);

// Clear framebuffer
void framebuffer_clear(Framebuffer* fb);

// Render cube to framebuffer
void render_cube(Framebuffer* fb, CubeState* cube, Light light, FrameStats stats);

// Display framebuffer to terminal
void framebuffer_display(Framebuffer* fb);

// Map intensity to Unicode character
wchar_t intensity_to_char(float intensity, bool is_edge);

#endif // RENDER_H
