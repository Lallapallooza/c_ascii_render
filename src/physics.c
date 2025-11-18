#include "physics.h"
#include <math.h>

void physics_step(CubeState* state, InputState input, PhysicsConfig config, float dt) {
    // Toggle motion mode when M is pressed
    static bool m_was_pressed = false;
    if (input.m_pressed && !m_was_pressed) {
        state->motion_mode = !state->motion_mode;
    }
    m_was_pressed = input.m_pressed;

    // Update motion animation if active
    if (state->motion_mode) {
        // Advance motion phase
        state->motion_phase += dt * 0.8f;  // 0.8 rad/sec
        if (state->motion_phase > 6.28318530718f) {  // 2*PI
            state->motion_phase -= 6.28318530718f;
        }

        // 3D orbit: X/Y ellipse, Z oscillates
        float radius_xy = 2.5f;
        float depth_amp = 2.0f;
        float depth_offset = -2.5f;

        state->position.x = cosf(state->motion_phase) * radius_xy;
        state->position.y = sinf(state->motion_phase) * radius_xy * 0.6f;  // Ellipse
        state->position.z = depth_offset + cosf(state->motion_phase) * depth_amp;
    }

    // Apply input as angular acceleration
    Vec3 input_accel = {0, 0, 0};

    // W/S = rotate up/down, A/D = rotate left/right
    if (input.w_pressed) input_accel.x -= config.acceleration;
    if (input.s_pressed) input_accel.x += config.acceleration;
    if (input.a_pressed) input_accel.y -= config.acceleration;
    if (input.d_pressed) input_accel.y += config.acceleration;

    // Update angular velocity with acceleration
    state->angular_velocity = vec3_add(state->angular_velocity,
                                       vec3_multiply(input_accel, dt));

    // Apply damping (exponential decay)
    float damping_factor = expf(logf(config.damping) * dt * 60.0f);
    state->angular_velocity = vec3_multiply(state->angular_velocity, damping_factor);

    // Clamp velocity
    float speed = vec3_length(state->angular_velocity);
    if (speed > config.max_velocity) {
        state->angular_velocity = vec3_multiply(
            vec3_normalize(state->angular_velocity),
            config.max_velocity
        );
    }

    // Integrate rotation
    Vec3 angle_delta = vec3_multiply(state->angular_velocity, dt);

    Mat3 rot_x = mat3_rotate_x(angle_delta.x);
    Mat3 rot_y = mat3_rotate_y(angle_delta.y);
    Mat3 rot_z = mat3_rotate_z(angle_delta.z);

    // Apply rotations in correct order: R_new = R_z * R_y * R_x * R_old
    Mat3 combined = mat3_multiply(rot_z, mat3_multiply(rot_y, rot_x));
    state->rotation = mat3_multiply(combined, state->rotation);

    // Orthonormalize every few frames to prevent drift
    static int frame_count = 0;
    if (++frame_count > 100) {
        state->rotation = mat3_orthonormalize(state->rotation);
        frame_count = 0;
    }
}
