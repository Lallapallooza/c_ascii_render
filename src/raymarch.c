#include "raymarch.h"
#include "sdf.h"
#include <stdbool.h>

static Vec3 estimate_normal(Vec3 point, Vec3 cube_center, float cube_size, Mat3 cube_rotation) {
    const float h = 0.0001f;
    Vec3 n;

    n.x = sdf_cube(vec3_add(point, (Vec3){h, 0, 0}), cube_center, cube_size, cube_rotation) -
          sdf_cube(vec3_subtract(point, (Vec3){h, 0, 0}), cube_center, cube_size, cube_rotation);

    n.y = sdf_cube(vec3_add(point, (Vec3){0, h, 0}), cube_center, cube_size, cube_rotation) -
          sdf_cube(vec3_subtract(point, (Vec3){0, h, 0}), cube_center, cube_size, cube_rotation);

    n.z = sdf_cube(vec3_add(point, (Vec3){0, 0, h}), cube_center, cube_size, cube_rotation) -
          sdf_cube(vec3_subtract(point, (Vec3){0, 0, h}), cube_center, cube_size, cube_rotation);

    return vec3_normalize(n);
}

bool raymarch(Vec3 origin, Vec3 direction, RaymarchConfig config,
              Vec3* hit_point, Vec3* normal, Vec3 cube_center,
              float cube_size, Mat3 cube_rotation) {

    float t = 0.0f;
    Vec3 current_point;

    for (int i = 0; i < config.max_steps; i++) {
        current_point = vec3_add(origin, vec3_multiply(direction, t));
        float dist = sdf_cube(current_point, cube_center, cube_size, cube_rotation);

        if (dist < config.epsilon) {
            // Hit!
            *hit_point = current_point;
            *normal = estimate_normal(current_point, cube_center, cube_size, cube_rotation);
            return true;
        }

        t += dist;

        if (t > config.max_distance) {
            return false;  // Miss
        }
    }

    return false;  // Max steps exceeded
}
