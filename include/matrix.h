#ifndef MATRIX_H
#define MATRIX_H

#include "vec3.h"

typedef struct {
    float m[9];  // Row-major 3x3 matrix
} Mat3;

Mat3 mat3_identity(void);
Mat3 mat3_multiply(Mat3 a, Mat3 b);
Mat3 mat3_rotate_x(float angle_rad);
Mat3 mat3_rotate_y(float angle_rad);
Mat3 mat3_rotate_z(float angle_rad);
Vec3 mat3_multiply_vec3(Mat3 m, Vec3 v);
Mat3 mat3_orthonormalize(Mat3 m);
float mat3_determinant(Mat3 m);
Mat3 mat3_transpose(Mat3 m);

#endif // MATRIX_H
