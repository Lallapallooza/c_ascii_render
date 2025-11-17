#ifndef VEC3_H
#define VEC3_H

typedef struct {
    float x, y, z;
} Vec3;

Vec3 vec3_create(float x, float y, float z);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_subtract(Vec3 a, Vec3 b);
Vec3 vec3_multiply(Vec3 v, float scalar);
float vec3_dot(Vec3 a, Vec3 b);
Vec3 vec3_cross(Vec3 a, Vec3 b);
float vec3_length(Vec3 v);
Vec3 vec3_normalize(Vec3 v);
Vec3 vec3_reflect(Vec3 v, Vec3 normal);

#endif // VEC3_H
