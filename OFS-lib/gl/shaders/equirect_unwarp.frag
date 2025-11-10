#version 330 core

// Equirectangular (180°/360°) to flat perspective unwarp shader
// Adapted from flatten-equirectangular.glsl

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D inputTexture;
uniform float u_yaw;         // Horizontal rotation in radians
uniform float u_pitch;       // Vertical rotation in radians
uniform float u_fov;         // Output field of view in radians (default: PI/2 = 90°)
uniform float u_aspect;      // Output aspect ratio

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

vec2 equirect2flat(vec2 uv) {
    const float PI = 3.14159265359;

    // Convert screen UV (0..1) to NDC (-1..1)
    vec2 ndc = uv * 2.0 - 1.0;

    // Apply aspect ratio
    ndc.x *= u_aspect;

    // Create ray direction for perspective projection
    float focal_length = 1.0 / tan(u_fov * 0.5);
    vec3 ray_dir = normalize(vec3(-ndc.x, ndc.y, focal_length));

    // Apply rotations (yaw around Y-axis, pitch around X-axis)
    mat3 rotation = rotationY(u_yaw) * rotationX(u_pitch);
    ray_dir = rotation * ray_dir;

    // Convert 3D direction to equirectangular coordinates
    float longitude = atan(ray_dir.z, ray_dir.x);
    float latitude = asin(clamp(ray_dir.y, -1.0, 1.0));

    // Normalize to texture coordinates (0..1)
    vec2 equirect_uv;
    equirect_uv.x = 1.0 - (longitude + PI) / (2.0 * PI);
    equirect_uv.y = 0.5 - (latitude / PI);

    // Clamp to valid range
    return clamp(equirect_uv, 0.0, 1.0);
}

void main() {
    vec2 src_uv = equirect2flat(TexCoords);
    FragColor = texture(inputTexture, src_uv);
}
