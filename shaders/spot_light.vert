#version 450

layout(location = 0) in vec3 in_position;
layout(location = 0) out vec3 light_color;

layout(std140, set = 0, binding = 0) uniform SceneUniforms {
    mat4 view_projection;
    vec4 camera_position;
    vec4 camera_direction;
    vec4 directional_light_direction;
    vec4 directional_light_color;
    vec4 ambient_light;
    uvec4 light_counts;
} scene_uniforms;

struct SpotLightData {
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 cone;
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLights {
    SpotLightData lights[];
} spot_light_buffer;

void main() {
    SpotLightData light = spot_light_buffer.lights[gl_InstanceIndex];
    const float marker_size = 0.12;
    vec3 world_position = light.position.xyz + in_position * marker_size;

    gl_Position = scene_uniforms.view_projection * vec4(world_position, 1.0);
    light_color = light.color.rgb;
}
