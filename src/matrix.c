#include "matrix.h"
#include <math.h>
#include <string.h>

Mat3 mat3_identity(void) {
    Mat3 m;
    memset(&m, 0, sizeof(Mat3));
    m.m[0] = m.m[4] = m.m[8] = 1.0f;
    return m;
}

Mat3 mat3_multiply(Mat3 a, Mat3 b) {
    Mat3 result;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            result.m[i * 3 + j] = 0.0f;
            for (int k = 0; k < 3; k++) {
                result.m[i * 3 + j] += a.m[i * 3 + k] * b.m[k * 3 + j];
            }
        }
    }
    return result;
}

Mat3 mat3_rotate_x(float angle_rad) {
    Mat3 m = mat3_identity();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    m.m[4] = c;
    m.m[5] = -s;
    m.m[7] = s;
    m.m[8] = c;
    return m;
}

Mat3 mat3_rotate_y(float angle_rad) {
    Mat3 m = mat3_identity();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    m.m[0] = c;
    m.m[2] = s;
    m.m[6] = -s;
    m.m[8] = c;
    return m;
}

Mat3 mat3_rotate_z(float angle_rad) {
    Mat3 m = mat3_identity();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    m.m[0] = c;
    m.m[1] = -s;
    m.m[3] = s;
    m.m[4] = c;
    return m;
}

Vec3 mat3_multiply_vec3(Mat3 m, Vec3 v) {
    return (Vec3){
        m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z,
        m.m[3] * v.x + m.m[4] * v.y + m.m[5] * v.z,
        m.m[6] * v.x + m.m[7] * v.y + m.m[8] * v.z
    };
}

Mat3 mat3_orthonormalize(Mat3 m) {
    // Gram-Schmidt orthonormalization
    Vec3 x = {m.m[0], m.m[3], m.m[6]};
    Vec3 y = {m.m[1], m.m[4], m.m[7]};
    Vec3 z = {m.m[2], m.m[5], m.m[8]};

    x = vec3_normalize(x);
    y = vec3_subtract(y, vec3_multiply(x, vec3_dot(x, y)));
    y = vec3_normalize(y);
    z = vec3_cross(x, y);

    Mat3 result;
    result.m[0] = x.x; result.m[1] = y.x; result.m[2] = z.x;
    result.m[3] = x.y; result.m[4] = y.y; result.m[5] = z.y;
    result.m[6] = x.z; result.m[7] = y.z; result.m[8] = z.z;
    return result;
}

float mat3_determinant(Mat3 m) {
    return m.m[0] * (m.m[4] * m.m[8] - m.m[5] * m.m[7]) -
           m.m[1] * (m.m[3] * m.m[8] - m.m[5] * m.m[6]) +
           m.m[2] * (m.m[3] * m.m[7] - m.m[4] * m.m[6]);
}

Mat3 mat3_transpose(Mat3 m) {
    Mat3 result;
    result.m[0] = m.m[0];
    result.m[1] = m.m[3];
    result.m[2] = m.m[6];
    result.m[3] = m.m[1];
    result.m[4] = m.m[4];
    result.m[5] = m.m[7];
    result.m[6] = m.m[2];
    result.m[7] = m.m[5];
    result.m[8] = m.m[8];
    return result;
}
