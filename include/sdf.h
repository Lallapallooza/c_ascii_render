#ifndef SDF_H
#define SDF_H

#include "vec3.h"
#include "matrix.h"

// Signed distance function for a cube
// Returns negative inside, positive outside, zero on surface
float sdf_cube(Vec3 point, Vec3 cube_center, float half_extent, Mat3 rotation);

#endif // SDF_H
