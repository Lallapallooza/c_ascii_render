#ifndef PHYSICS_H
#define PHYSICS_H

#include "vec3.h"
#include "matrix.h"
#include "input.h"

typedef struct {
    float acceleration;
    float damping;
    float max_velocity;
} PhysicsConfig;

typedef struct {
    Mat3 rotation;
    Vec3 angular_velocity;
    Vec3 position;
    float size;
    bool motion_mode;      // Whether cube is flying in a pattern
    float motion_phase;    // Current phase of the motion (0 to 2*PI)
} CubeState;

void physics_step(CubeState* state, InputState input, PhysicsConfig config, float dt);

#endif // PHYSICS_H
