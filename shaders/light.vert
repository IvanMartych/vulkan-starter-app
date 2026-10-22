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

struct PointLightData {
    vec4 position;
    vec4 color;
};

layout(std430, set = 0, binding = 1) readonly buffer PointLights {
    PointLightData lights[];
} point_light_buffer;

void main() {

    PointLightData light = point_light_buffer.lights[gl_InstanceIndex];

    const float marker_size = 0.12;
    vec3 world_position = light.position.xyz + in_position * marker_size;

    gl_Position = scene_uniforms.view_projection * vec4(world_position, 1.0);
    light_color = light.color.rgb;

}
