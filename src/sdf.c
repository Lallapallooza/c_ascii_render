#include "sdf.h"
#include <math.h>

static float fmaxf3(float a, float b, float c) {
    return fmaxf(fmaxf(a, b), c);
}

float sdf_cube(Vec3 point, Vec3 cube_center, float half_extent, Mat3 rotation) {
    // Transform point to cube's local space
    Vec3 local_point = vec3_subtract(point, cube_center);

    // Apply inverse rotation (transpose for orthonormal matrix)
    Mat3 inv_rot = mat3_transpose(rotation);
    local_point = mat3_multiply_vec3(inv_rot, local_point);

    // Box SDF
    Vec3 d = {fabsf(local_point.x) - half_extent,
              fabsf(local_point.y) - half_extent,
              fabsf(local_point.z) - half_extent};

    // Distance from outside + distance from inside
    float outside = vec3_length((Vec3){
        fmaxf(d.x, 0.0f),
        fmaxf(d.y, 0.0f),
        fmaxf(d.z, 0.0f)
    });

    float inside = fminf(fmaxf3(d.x, d.y, d.z), 0.0f);

    return outside + inside;
}
