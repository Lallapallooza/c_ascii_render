#ifndef RAYMARCH_H
#define RAYMARCH_H

#include "vec3.h"
#include "matrix.h"
#include <stdbool.h>

typedef struct {
    int max_steps;
    float epsilon;
    float max_distance;
} RaymarchConfig;

// Raymarch from origin in direction
// Returns true if hit, populates hit_point and normal
bool raymarch(Vec3 origin, Vec3 direction, RaymarchConfig config,
              Vec3* hit_point, Vec3* normal, Vec3 cube_center,
              float cube_size, Mat3 cube_rotation);

#endif // RAYMARCH_H
