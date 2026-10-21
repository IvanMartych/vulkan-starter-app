#version 450

layout(location = 0) in vec3 vertex_color;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec3 vertex_world_position;

layout(location = 0) out vec4 out_color;

layout(std140, set = 0, binding = 0) uniform SceneUniforms{

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


struct PointLightData{

    vec4 position;
    vec4 color;

};

layout(std430, set = 0, binding = 1) readonly buffer PointLights{

    PointLightData lights[];

} point_light_buffer;


vec3 calculatePointLight(PointLightData light, vec3 normal, vec3 view_direction){

    vec3 to_light = light.position.xyz - vertex_world_position;

    float distance_squared = dot(to_light, to_light);

    if (distance_squared < 0.000001){
        return vec3(0.0);
    }

    vec3 light_direction = normalize(to_light);

    vec3 light_color =light.color.rgb * light.color.w / distance_squared;

    float diffuse_factor = max(dot(normal, light_direction), 0.0);

    float specular_factor = 0.0;

    if (diffuse_factor > 0.0 && dot(normal, view_direction) > 0.0){
        vec3 halfway_direction = normalize(light_direction + view_direction);

        specular_factor = pow(max(dot(normal, halfway_direction), 0.0), model_uniforms.specular.w);
    }

    return light_color * (vertex_color * diffuse_factor+ model_uniforms.specular.rgb * specular_factor);

}


void main() {
    vec3 normal = normalize(vertex_normal);

    vec3 light_direction = -scene_uniforms.directional_light_direction.xyz;

    float diffuse_factor = max(dot(normal, light_direction), 0.0);

    vec3 light_color = scene_uniforms.directional_light_color.rgb * scene_uniforms.directional_light_color.a;

    vec3 ambient = scene_uniforms.ambient_light.rgb * scene_uniforms.ambient_light.a;

    vec3 diffuse = light_color * diffuse_factor;

    vec3 view_direction = -scene_uniforms.camera_direction.xyz;

    if (scene_uniforms.camera_direction.w < 0.5) {

        vec3 to_camera = scene_uniforms.camera_position.xyz - vertex_world_position;

        if (dot(to_camera, to_camera) > 0.00000001){
            view_direction = normalize(to_camera);
        }

    }

    float specular_factor = 0.0;

    if (diffuse_factor > 0.0 && dot(normal, view_direction) > 0.0){

        vec3 halfway_direction = normalize(
            light_direction + view_direction
        );

        specular_factor = pow(
            max(dot(normal, halfway_direction), 0.0),
            model_uniforms.specular.w
        );

    }

    vec3 specular = light_color * model_uniforms.specular.rgb * specular_factor;

    vec3 result = vertex_color * (ambient + diffuse) + specular;

    for (uint i = 0u; i < scene_uniforms.light_counts.x; ++i) {
        result += calculatePointLight(
            point_light_buffer.lights[i],
            normal,
            view_direction
        );
    }

    out_color = vec4(result, 1.0);

}