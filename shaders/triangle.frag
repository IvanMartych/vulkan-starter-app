#version 450

layout(location = 0) in vec2 vertex_texcoord;
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


layout(set = 1, binding = 1) uniform sampler2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler2D specular_texture;
layout(set = 1, binding = 3) uniform sampler2D emissive_texture;

layout(push_constant) uniform SamplingParameters{
    float time;
    float strength;
    float uv_scale;
    uint flags;
    float emissive_strength;
} sampling;


void sampleMaterial(out vec3 albedo, out vec3 material_specular, out vec3 emission){

    vec2 uv = vertex_texcoord * sampling.uv_scale;
    vec2 first_uv = uv;
    vec2 second_uv = uv;
    float blend = 0.0;
    vec3 modulation = vec3(1.0);


    if ((sampling.flags & 1u) != 0u) {
        first_uv += sampling.strength * vec2(0.12 * sampling.time, 0.05 * sin(sampling.time));
        second_uv = uv * 1.7 + vec2(-0.08 * sampling.time, 0.1 * cos(sampling.time * 0.7));
        blend = sampling.strength * (0.5 + 0.5 * sin(sampling.time + uv.x * 6.28318));
        modulation = mix(vec3(1.0),
            0.65 + 0.35 * cos(vec3(0.0, 2.1, 4.2) + sampling.time + uv.y * 3.0),
            sampling.strength);
    }

    albedo = texture(albedo_texture, first_uv).rgb;
    material_specular = texture(specular_texture, first_uv).rgb;
    emission = texture(emissive_texture, first_uv).rgb;

    if ((sampling.flags & 1u) != 0u) {
        albedo = mix(albedo, texture(albedo_texture, second_uv).rgb, blend);
        material_specular = mix(material_specular, texture(specular_texture, second_uv).rgb, blend);
        emission = mix(emission, texture(emissive_texture, second_uv).rgb, blend);
    }
    
    albedo *= model_uniforms.base_color.rgb * modulation;
    material_specular *= model_uniforms.specular.rgb;
    emission *= sampling.emissive_strength;
    
}



struct PointLightData{

    vec4 position;
    vec4 color;

};

layout(std430, set = 0, binding = 1) readonly buffer PointLights{

    PointLightData lights[];

} point_light_buffer;

struct SpotLightData {
    vec4 position;
    vec4 color;
    vec4 direction;
    vec4 cone;
};

layout(std430, set = 0, binding = 2) readonly buffer SpotLights {
    SpotLightData lights[];
} spot_light_buffer;


vec3 calculatePointLight(PointLightData light, vec3 normal, vec3 view_direction,
                         vec3 albedo, vec3 material_specular) {

    vec3 to_light = light.position.xyz - vertex_world_position;

    float distance_squared = dot(to_light, to_light);

    if (distance_squared < 0.000001){
        return vec3(0.0);
    }

    vec3 light_direction = normalize(to_light);

    vec3 light_color = light.color.rgb * light.color.w / distance_squared;

    float diffuse_factor = max(dot(normal, light_direction), 0.0);

    float specular_factor = 0.0;

    if (diffuse_factor > 0.0 && dot(normal, view_direction) > 0.0){
        vec3 halfway_direction = normalize(light_direction + view_direction);

        specular_factor = pow(max(dot(normal, halfway_direction), 0.0), model_uniforms.specular.w);
    }

    return light_color * (albedo * diffuse_factor+ material_specular * specular_factor);

}


vec3 calculateSpotLight(SpotLightData light, vec3 normal, vec3 view_direction,
                        vec3 albedo, vec3 material_specular) {

    vec3 to_light = light.position.xyz - vertex_world_position;
    float distance_squared = dot(to_light, to_light);

    if (distance_squared < 0.000001) {
        return vec3(0.0);
    }

    vec3 light_direction = normalize(to_light);

    float cos_angle = dot(-light_direction, light.direction.xyz);
    float cos_inner = light.cone.x;
    float cos_outer = light.cone.y;

    float cone_factor;
    if (cos_inner > cos_outer) {
        cone_factor = smoothstep(cos_outer, cos_inner, cos_angle);
    } else {
        cone_factor = step(cos_outer, cos_angle);
    }

    if (cone_factor <= 0.0) {
        return vec3(0.0);
    }

    vec3 light_color = light.color.rgb * light.color.w * cone_factor / distance_squared;

    float diffuse_factor = max(dot(normal, light_direction), 0.0);
    float specular_factor = 0.0;

    if (diffuse_factor > 0.0 && dot(normal, view_direction) > 0.0) {
        vec3 halfway_direction = normalize(light_direction + view_direction);
        specular_factor = pow(
            max(dot(normal, halfway_direction), 0.0),
            model_uniforms.specular.w
        );
    }

    return light_color * (
        albedo * diffuse_factor + material_specular * specular_factor
    );
}


void main() {
    vec3 albedo;
    vec3 material_specular;
    vec3 emission;
    sampleMaterial(albedo, material_specular, emission);

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

    vec3 specular = light_color * material_specular * specular_factor;

    vec3 result = albedo * (ambient + diffuse) + specular;

    for (uint i = 0u; i < scene_uniforms.light_counts.x; ++i) {
        result += calculatePointLight(
            point_light_buffer.lights[i],
            normal,
            view_direction,
            albedo,
            material_specular
        );
    }

    for (uint i = 0u; i < scene_uniforms.light_counts.y; ++i) {
        result += calculateSpotLight(
            spot_light_buffer.lights[i],
            normal,
            view_direction,
            albedo,
            material_specular
        );
    }

    result += emission;
    out_color = vec4(result, 1.0);

}
