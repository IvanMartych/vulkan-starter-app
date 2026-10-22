#version 450

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

// Keep in sync with SPOT_CONE_VERTEX_COUNT in application.cpp.
const int SEGMENTS = 48;
const int RING_VERTICES = SEGMENTS * 2;
const float TWO_PI = 6.28318530718;

vec3 ringPoint(vec3 center, vec3 right, vec3 up,
               float length, float cos_angle, float angle) {
    float radius = length * sqrt(max(1.0 - cos_angle * cos_angle, 0.0))
        / max(cos_angle, 0.0001);
    return center + radius * (cos(angle) * right + sin(angle) * up);
}

void main() {
    SpotLightData light = spot_light_buffer.lights[gl_InstanceIndex];
    vec3 forward = light.direction.xyz;

    // Use another reference axis when the cone points almost vertically.
    vec3 helper = abs(forward.y) < 0.99
        ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(forward, helper));
    vec3 up = cross(right, forward);

    float display_length = light.cone.z;
    vec3 center = light.position.xyz + forward * display_length;
    vec3 world_position;
    int vertex = gl_VertexIndex;
    light_color = light.color.rgb;

    if (vertex < RING_VERTICES * 2) {
        // First the outer circle, then the inner circle at the same distance.
        bool inner = vertex >= RING_VERTICES;
        int ring_vertex = vertex % RING_VERTICES;
        int segment = ring_vertex / 2;
        int endpoint = ring_vertex % 2;
        float angle = TWO_PI * float(segment + endpoint) / float(SEGMENTS);
        float cos_angle = inner ? light.cone.x : light.cone.y;
        world_position = ringPoint(
            center, right, up, display_length, cos_angle, angle
        );
        if (inner) {
            light_color *= 0.6;
        }
    } else if (vertex < RING_VERTICES * 2 + 8) {
        // Four lines connect the source to the outer circle.
        int side_vertex = vertex - RING_VERTICES * 2;
        float angle = TWO_PI * float(side_vertex / 2) / 4.0;
        world_position = side_vertex % 2 == 0 ? light.position.xyz
            : ringPoint(center, right, up, display_length, light.cone.y, angle);
    } else {
        // The last line is the central ray of the spotlight.
        world_position = vertex % 2 == 0 ? light.position.xyz : center;
    }

    gl_Position = scene_uniforms.view_projection * vec4(world_position, 1.0);
}
