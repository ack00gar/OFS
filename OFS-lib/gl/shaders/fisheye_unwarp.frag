#version 330 core

// Fisheye (190°/200°) to flat perspective unwarp shader
// Adapted from flatten-fisheye-2.glsl

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D inputTexture;
uniform float u_yaw;         // Horizontal rotation in degrees
uniform float u_pitch;       // Vertical rotation in degrees
uniform float u_fov;         // Fisheye FOV in degrees (190 or 200)
uniform float u_output_fov;  // Output flat view FOV in degrees (default: 120)
uniform float u_aspect;      // Output aspect ratio
uniform int u_use_right_eye; // 0 = left eye, 1 = right eye (for SBS)

// Rotation matrix around Y axis (yaw)
mat3 rotationY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        c, 0.0, s,
        0.0, 1.0, 0.0,
        -s, 0.0, c
    );
}

// Rotation matrix around X axis (pitch)
mat3 rotationX(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        1.0, 0.0, 0.0,
        0.0, c, -s,
        0.0, s, c
    );
}

vec2 flat2fish(vec2 uv) {
    const float PI = 3.14159265359;

    // Convert FOV to radians
    float fov_rad = PI * u_fov / 180.0;
    float yaw_rad = PI * u_yaw / 180.0;
    float pitch_rad = PI * u_pitch / 180.0;
    float output_fov_rad = PI * u_output_fov / 180.0;

    // Convert screen UV (0..1) to NDC (-1..1)
    vec2 ndc = uv * 2.0 - 1.0;

    // Apply aspect ratio
    ndc.x *= u_aspect;

    // Create 3D ray direction for flat perspective projection
    float focal_length = 1.0 / tan(output_fov_rad * 0.5);
    vec3 ray_dir = normalize(vec3(ndc.x, ndc.y, focal_length));

    // Apply rotations (yaw around Y-axis, pitch around X-axis)
    mat3 rotation = rotationY(yaw_rad) * rotationX(pitch_rad);
    ray_dir = rotation * ray_dir;

    // Convert 3D direction to fisheye coordinates
    float p_x = ray_dir.x;
    float p_y = ray_dir.z;  // Using z for depth
    float p_z = ray_dir.y;  // Using y for height

    // Calculate fisheye projection parameters
    float p_xz = sqrt(p_x * p_x + p_z * p_z);
    float r = 2.0 * atan(p_xz, p_y) / fov_rad;
    float theta = atan(p_z, p_x);

    // Convert to normalized fisheye coordinates
    float x_src_norm = r * cos(theta);
    float y_src_norm = r * sin(theta);

    // Map from NDC (-1..1) to UV (0..1)
    vec2 fish_uv = (vec2(x_src_norm, y_src_norm) + 1.0) * 0.5;

    // Handle stereo (SBS) eye selection
    if (u_use_right_eye == 1) {
        // Right eye is in the right half
        fish_uv.x = fish_uv.x * 0.5 + 0.5;
    } else {
        // Left eye is in the left half
        fish_uv.x = fish_uv.x * 0.5;
    }

    // Clamp to valid range
    return clamp(fish_uv, 0.0, 1.0);
}

void main() {
    vec2 src_uv = flat2fish(TexCoords);
    FragColor = texture(inputTexture, src_uv);
}
