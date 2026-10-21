#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec3 vertex_color;
layout(location = 1) out vec3 vertex_normal;
layout(location = 2) out vec3 vertex_world_position;


layout(std140, set = 0, binding = 0) uniform SceneUniforms {

    mat4 view_projection;
    vec4 camera_position;
    vec4 camera_direction;

    vec4 directional_light_direction;
    vec4 directional_light_color;
    vec4 ambient_light;

    uvec4 light_counts;

} scene_uniforms;


layout(std140, set = 1, binding = 0) uniform ModelUniforms {

    mat4 model;
    vec4 base_color;
    vec4 specular;
    
} model_uniforms;


void main() {

    vec4 world_position = model_uniforms.model * vec4(in_position, 1.0);

    gl_Position = scene_uniforms.view_projection * world_position;

    vertex_color = in_color * model_uniforms.base_color.rgb;

    mat3 normal_matrix = transpose(
        inverse(mat3(model_uniforms.model))
    );

    vertex_normal = normal_matrix * in_normal;

    vertex_world_position = world_position.xyz;

}