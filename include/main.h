#ifndef MAIN_H
#define MAIN_H

typedef struct {
    float cube_size;
    float rotation_speed;
    float light_x;
    float light_y;
    float light_z;
    int max_raymarch_steps;
} Config;

// Parse command line arguments
int parse_args(int argc, char** argv, Config* config);

// Print usage information
void print_usage(const char* program_name);

#endif // MAIN_H
