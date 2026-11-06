#include "application.hpp"
#include "graphics_internal.hpp"
#include "texture.hpp"

#include <cstdint>
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <limits>
#include <string>

namespace application {


VkShaderModule vertex_shader = VK_NULL_HANDLE;
VkShaderModule fragment_shader = VK_NULL_HANDLE;
VkShaderModule light_vertex_shader = VK_NULL_HANDLE;
VkShaderModule light_fragment_shader = VK_NULL_HANDLE;
VkShaderModule spot_light_vertex_shader = VK_NULL_HANDLE;
VkShaderModule spot_cone_vertex_shader = VK_NULL_HANDLE;

VkBuffer vertex_buffer = VK_NULL_HANDLE;
VkBuffer index_buffer = VK_NULL_HANDLE;

VmaAllocation vertex_allocation = VK_NULL_HANDLE;
VmaAllocation index_allocation = VK_NULL_HANDLE;

VkBuffer scene_uniform_buffer = VK_NULL_HANDLE;
VkBuffer model_uniform_buffer = VK_NULL_HANDLE;

VmaAllocation scene_uniform_allocation = VK_NULL_HANDLE;
VmaAllocation model_uniform_allocation = VK_NULL_HANDLE;

VkDescriptorSetLayout scene_descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorSetLayout model_descriptor_set_layout = VK_NULL_HANDLE;

VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

VkDescriptorSet scene_descriptor_set = VK_NULL_HANDLE;
VkDeviceSize model_uniform_stride = 0;
VkSampler texture_sampler = VK_NULL_HANDLE;
texture::Texture black_texture;

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline graphics_pipeline = VK_NULL_HANDLE;
VkPipeline light_pipeline = VK_NULL_HANDLE;
VkPipeline spot_light_pipeline = VK_NULL_HANDLE;
VkPipeline spot_cone_pipeline = VK_NULL_HANDLE;

VkBuffer point_light_buffer = VK_NULL_HANDLE;
VmaAllocation point_light_allocation = VK_NULL_HANDLE;
VkBuffer spot_light_buffer = VK_NULL_HANDLE;
VmaAllocation spot_light_allocation = VK_NULL_HANDLE;


glm::vec3 camera_position{0.0f, 1.4f, 6.5f};
float camera_yaw = glm::radians(-90.0f);
float camera_pitch = glm::radians(-10.0f);
float camera_fov_degrees = 45.0f;

int projection_type = 0;
struct SceneObject {
    const char* name;
    glm::vec3 position;
    glm::vec3 rotation_degrees;
    glm::vec3 scale;
    int material_index = 0;
    bool animated_sampling = false;
    bool animate_transform = false;
    glm::vec3 base_color{1.0f};
    glm::vec3 specular_color{0.8f};
    float shininess = 32.0f;
    float uv_scale = 1.0f;
    float sampling_strength = 0.65f;
    float sampling_speed = 1.0f;
    float emissive_strength = 1.5f;
};

std::vector<SceneObject> scene_objects{
    {"Cube 1", {-1.5f, 0.0f, 0.0f}, {0.0f, 20.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0, false, true},
    {"Cube 2", {0.0f, 0.0f, 0.0f}, {0.0f, -15.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 1},
    {"Cube 3", {1.5f, 0.0f, 0.0f}, {0.0f, 25.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 2, true},
    {"Floor", {0.0f, -0.65f, -0.6f}, {0.0f, 0.0f, 0.0f}, {6.5f, 0.15f, 5.0f}, 1}
};
const std::vector<SceneObject> initial_scene_objects = scene_objects;
int selected_object = 0;

struct Material {
    const char* name;
    const char* albedo_path;
    const char* specular_path;
    const char* emissive_path;
    texture::Texture albedo;
    texture::Texture specular;
    texture::Texture emissive;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

std::vector<Material> materials{
    {"Brick (albedo only)", "brick_albedo.png", nullptr, nullptr, {}, {}, {}},
    {"Wood (albedo + specular)", "wood_albedo.png", "wood_specular.png", nullptr, {}, {}, {}},
    {"Circuit (all maps)", "circuit_albedo.png", "circuit_specular.png", "circuit_emissive.png", {}, {}, {}}
};

// Sampling controls go through push constants, not ModelUniforms.
struct alignas(16) SamplingPushConstants {
    float time;
    float strength;
    float uv_scale;
    std::uint32_t flags;
    float emissive_strength;
    float padding[3]{};
};
static_assert(sizeof(SamplingPushConstants) == 32);
static_assert(offsetof(SamplingPushConstants, emissive_strength) == 16);
float sampling_time = 0.0f;
bool sampling_playing = true;


bool animation_playing = false;
float animation_time = 0.0f; 
float animation_speed = 1.0f;
glm::vec3 animation_amplitude(0.6f, 0.25f, 0.4f);
glm::vec3 animation_rotation_speed(20.0f, 40.0f, 10.0f);


glm::vec3 directional_light_direction{-0.5f, -1.0f, -0.5f};
glm::vec3 directional_light_color{1.0f};
float directional_light_intensity = 1.0f;

glm::vec3 ambient_light_color{1.0f};
float ambient_light_intensity = 0.15f;
bool show_light_sources = true;
bool show_spot_light_sources = true;
bool show_spot_light_cones = false;


struct PointLight {
    glm::vec3 position{1.0f, 0.6f, 1.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 5.0f;
};

struct alignas(16) PointLightData {
    float position[4];
    float color[4];
};

constexpr std::size_t MAX_POINT_LIGHTS = 100;

std::vector<PointLight> point_lights{PointLight{}};

struct SpotLight {
    glm::vec3 position{1.2f, 1.5f, 2.0f};
    glm::vec3 direction{-1.2f, -1.5f, -2.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 5.0f;


    float inner_angle_degrees = 15.0f;
    float outer_angle_degrees = 25.0f;
    float cone_display_length = 3.0f;
};

struct alignas(16) SpotLightData {
    float position[4];
    float color[4];
    float direction[4];
    float cone[4]; // cos(inner angle), cos(outer angle)
};

static_assert(sizeof(SpotLightData) == 64);
static_assert(offsetof(SpotLightData, cone) == 48);

constexpr std::size_t MAX_SPOT_LIGHTS = 100;
std::vector<SpotLight> spot_lights{SpotLight{}};


constexpr std::uint32_t SPOT_CONE_VERTEX_COUNT = 2 * 48 * 2 + 4 * 2 + 2;


struct Vertex {

    float position[3];
    float normal[3];
    float texcoord[2];

};


struct alignas(16) SceneUniforms {

    float view_projection[4][4];
    float camera_position[4];
    float camera_direction[4];

    float directional_light_direction[4];
    float directional_light_color[4];
    float ambient_light[4];

    std::uint32_t light_counts[4];

};


struct alignas(16) ModelUniforms {

    float model[4][4];
    float base_color[4];
    float specular[4];

};
static_assert(sizeof(ModelUniforms) == 96);
static_assert(sizeof(Vertex) == 32);


struct MeshData {

    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;

};

MeshData generateCube(float side_length){

    MeshData mesh;

    const float half_size = side_length * 0.5f;

    std::vector<glm::vec3> corners;

    for (int z = -1; z <= 1; z += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int x = -1; x <= 1; x += 2) {
                corners.push_back({
                    x * half_size,
                    y * half_size,
                    z * half_size
                });
            }
        }
    }

    const std::uint16_t faces[6][4] = {
        {4, 5, 7, 6}, // +Z
        {1, 0, 2, 3}, // -Z
        {0, 4, 6, 2}, // -X
        {5, 1, 3, 7}, // +X
        {6, 7, 3, 2}, // +Y
        {0, 1, 5, 4}  // -Y
    };

    for (const auto& face : faces){

        const glm::vec3 p0 = corners[face[0]];
        const glm::vec3 p1 = corners[face[1]];
        const glm::vec3 p2 = corners[face[2]];

        const glm::vec3 edge1 = p1 - p0;
        const glm::vec3 edge2 = p2 - p0;

        const glm::vec3 normal = glm::normalize(
            glm::cross(edge1, edge2)
        );

        const std::size_t first_vertex = mesh.vertices.size();

        const glm::vec2 texcoords[4] = {
            {0.0f, 1.0f}, {1.0f, 1.0f},
            {1.0f, 0.0f}, {0.0f, 0.0f}
        };

        for (std::size_t i = 0; i < 4; ++i) {
            const glm::vec3 position = corners[face[i]];
            mesh.vertices.push_back({
                {position.x, position.y, position.z},
                {normal.x, normal.y, normal.z},
                {texcoords[i].x, texcoords[i].y}
            });
        }

        const std::uint16_t local_indices[] = {
            0, 1, 2,
            2, 3, 0
        };

        for (const auto local_index : local_indices) {
            mesh.indices.push_back(
                static_cast<std::uint16_t>(
                    first_vertex + local_index
                )
            );
        }
    }

    return mesh;
}


const MeshData cube_mesh = generateCube(1.0f);


VkShaderModule loadShader(const char* path){

    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        std::cerr << "Cannot open shader: " << path << '\n';
        return VK_NULL_HANDLE;
    }

    const std::streamoff size = file.tellg();

    if (size <= 0 || size % 4 != 0) {
        std::cerr << "Invalid shader file: " << path << '\n';
        return VK_NULL_HANDLE;
    }

    std::vector<uint32_t> code(
        static_cast<size_t>(size) / sizeof(uint32_t)
    );

    file.seekg(0);

    if (!file.read(
            reinterpret_cast<char*>(code.data()),
            static_cast<std::streamsize>(size))) {
        std::cerr << "Cannot read shader: " << path << '\n';
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = static_cast<size_t>(size);
    info.pCode = code.data();

    VkShaderModule shader = VK_NULL_HANDLE;

    VkResult result = vkCreateShaderModule(
        graphics::internal::context.device,
        &info,
        nullptr,
        &shader
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create shader module\n";
        return VK_NULL_HANDLE;
    }

    return shader;

}


bool createVertexBuffer() {
    return texture::createDeviceBuffer(
        cube_mesh.vertices.data(),
        cube_mesh.vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        vertex_buffer,
        vertex_allocation
    );
}

bool createIndexBuffer() {
    return texture::createDeviceBuffer(
        cube_mesh.indices.data(),
        cube_mesh.indices.size() * sizeof(std::uint16_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        index_buffer,
        index_allocation
    );
}

bool createUniformBuffer(VkDeviceSize size, VkBuffer& buffer, VmaAllocation& allocation){

    auto& context = graphics::internal::context;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    const VkResult result = vmaCreateBuffer(
        context.allocator,
        &buffer_info,
        &allocation_info,
        &buffer,
        &allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create uniform buffer\n";
        return false;
    }

    return true;

}


bool createModelUniformBuffer() {

    auto& context = graphics::internal::context;
    
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(context.physical_device, &properties);

    // Every dynamic offset must be a multiple of the device alignment.
    const VkDeviceSize alignment = std::max<VkDeviceSize>(
        properties.limits.minUniformBufferOffsetAlignment, 16
    );

    model_uniform_stride = (sizeof(ModelUniforms) + alignment - 1) / alignment * alignment;

    const VkDeviceSize size = model_uniform_stride * scene_objects.size();
    if (size > std::numeric_limits<std::uint32_t>::max()) {
        std::cerr << "Model buffer offsets do not fit uint32_t\n";
        return false;
    }

    return createUniformBuffer(
        size, model_uniform_buffer, model_uniform_allocation
    );
}

bool createMaterials() {
    if (!texture::createBlack(black_texture)
        || !texture::createSampler(texture_sampler)) {
        return false;
    }

    for (auto& material : materials) {
        const std::string albedo_path = std::string(TEXTURE_DIR) + "/" + material.albedo_path;
        if (!texture::load(albedo_path.c_str(), true, material.albedo)) {
            return false;
        }
        if (material.specular_path) {
            const std::string path = std::string(TEXTURE_DIR) + "/" + material.specular_path;
            if (!texture::load(path.c_str(), false, material.specular)) return false;
        }
        if (material.emissive_path) {
            const std::string path = std::string(TEXTURE_DIR) + "/" + material.emissive_path;
            if (!texture::load(path.c_str(), true, material.emissive)) return false;
        }
    }
    return true;
}

bool createPointLightBuffer(){

    auto& context = graphics::internal::context;


    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = MAX_POINT_LIGHTS * sizeof(PointLightData);
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;


    const VkResult result = vmaCreateBuffer(
        context.allocator,
        &buffer_info,
        &allocation_info,
        &point_light_buffer,
        &point_light_allocation,
        nullptr
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create point light buffer\n";
        return false;
    }


    return true;

}


bool updatePointLightBuffer() {

    if (point_lights.size() > MAX_POINT_LIGHTS) {
        std::cerr << "Too many point lights\n";
        return false;
    }

    if (point_lights.empty()) {
        return true;
    }

    std::vector<PointLightData> data(point_lights.size());

    for (std::size_t i = 0; i < point_lights.size(); ++i){

        const auto& light = point_lights[i];

        data[i].position[0] = light.position.x;
        data[i].position[1] = light.position.y;
        data[i].position[2] = light.position.z;
        data[i].position[3] = 1.0f;

        data[i].color[0] = light.color.r;
        data[i].color[1] = light.color.g;
        data[i].color[2] = light.color.b;
        data[i].color[3] = light.intensity;

    }

    auto& context = graphics::internal::context;

    const VkResult result = vmaCopyMemoryToAllocation(
        context.allocator,
        data.data(),
        point_light_allocation,
        0,
        data.size() * sizeof(PointLightData)
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot update point light buffer\n";
        return false;
    }

    return true;

}


bool createSpotLightBuffer(){

    auto& context = graphics::internal::context;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = MAX_SPOT_LIGHTS * sizeof(SpotLightData);
    buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    const VkResult result = vmaCreateBuffer(
        context.allocator,
        &buffer_info,
        &allocation_info,
        &spot_light_buffer,
        &spot_light_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create spot light buffer\n";
        return false;
    }

    return true;
}


bool updateSpotLightBuffer() {
    
    if (spot_lights.size() > MAX_SPOT_LIGHTS) {
        std::cerr << "Too many spot lights\n";
        return false;
    }

    if (spot_lights.empty()) {
        return true;
    }

    std::vector<SpotLightData> data(spot_lights.size());

    for (std::size_t i = 0; i < spot_lights.size(); ++i) {
        const auto& light = spot_lights[i];

        glm::vec3 direction = light.direction;
        if (glm::dot(direction, direction) < 0.000001f) {
            direction = glm::vec3(0.0f, 0.0f, -1.0f);
        }
        direction = glm::normalize(direction);

        const float outer_angle = std::clamp(
            light.outer_angle_degrees, 0.0f, 89.0f
        );
        const float inner_angle = std::clamp(
            light.inner_angle_degrees, 0.0f, outer_angle
        );

        data[i].position[0] = light.position.x;
        data[i].position[1] = light.position.y;
        data[i].position[2] = light.position.z;
        data[i].position[3] = 1.0f;

        data[i].color[0] = light.color.r;
        data[i].color[1] = light.color.g;
        data[i].color[2] = light.color.b;
        data[i].color[3] = std::max(light.intensity, 0.0f);

        data[i].direction[0] = direction.x;
        data[i].direction[1] = direction.y;
        data[i].direction[2] = direction.z;
        data[i].direction[3] = 0.0f;

        data[i].cone[0] = std::cos(glm::radians(inner_angle));
        data[i].cone[1] = std::cos(glm::radians(outer_angle));
        data[i].cone[2] = std::clamp(light.cone_display_length, 0.2f, 10.0f);
    }

    auto& context = graphics::internal::context;

    const VkResult result = vmaCopyMemoryToAllocation(
        context.allocator,
        data.data(),
        spot_light_allocation,
        0,
        data.size() * sizeof(SpotLightData)
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot update spot light buffer\n";
        return false;
    }

    return true;
}

glm::vec3 getCameraForward(){

    const float cos_pitch = std::cos(camera_pitch);

    const glm::vec3 direction{
        std::cos(camera_yaw) * cos_pitch,
        std::sin(camera_pitch),
        std::sin(camera_yaw) * cos_pitch
    };

    return glm::normalize(direction);

}


bool updateUniformBuffers(){

    auto& context = graphics::internal::context;

    const auto extent = context.swapchain_extent;

    if (extent.width == 0 || extent.height == 0) {
        return false;
    }


    const auto aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const glm::vec3 camera_forward = getCameraForward();

    const glm::mat4 view = glm::lookAtRH(
        camera_position,
        camera_position + camera_forward,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 projection(1.0f);

    if (projection_type == 0){

        projection = glm::perspectiveRH_ZO(
            glm::radians(camera_fov_degrees), // вертикальный угол обзора
            aspect,             // ширина окна / высота окна
            0.1f,               // ближняя граница видимости
            100.0f              // дальняя граница видимости
        );

    } else {

        const float half_height = 1.5f;
        const float half_width = half_height * aspect;

        projection = glm::orthoRH_ZO(
            -half_width,   // левая граница
            half_width,   // правая граница
            -half_height,  // нижняя граница
            half_height,  // верхняя граница
            0.1f,         // ближняя плоскость
            100.0f        // дальняя плоскость
        );

    }

    projection[1][1] *= -1.0f;

    const glm::mat4 view_projection = projection * view;

    SceneUniforms scene_uniforms{};

    std::memcpy(
        scene_uniforms.view_projection,
        glm::value_ptr(view_projection),
        sizeof(scene_uniforms.view_projection)
    );

    scene_uniforms.camera_position[0] = camera_position.x;
    scene_uniforms.camera_position[1] = camera_position.y;
    scene_uniforms.camera_position[2] = camera_position.z;
    scene_uniforms.camera_position[3] = 1.0f;


    scene_uniforms.camera_direction[0] = camera_forward.x;
    scene_uniforms.camera_direction[1] = camera_forward.y;
    scene_uniforms.camera_direction[2] = camera_forward.z;
    scene_uniforms.camera_direction[3] = static_cast<float>(projection_type);



    glm::vec3 light_direction = directional_light_direction;

    if (glm::dot(light_direction, light_direction) < 0.000001f) {
        light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    }

    light_direction = glm::normalize(light_direction);


    scene_uniforms.directional_light_direction[0] = light_direction.x;
    scene_uniforms.directional_light_direction[1] = light_direction.y;
    scene_uniforms.directional_light_direction[2] = light_direction.z;
    scene_uniforms.directional_light_direction[3] = 0.0f;


    scene_uniforms.directional_light_color[0] = directional_light_color.r;
    scene_uniforms.directional_light_color[1] = directional_light_color.g;
    scene_uniforms.directional_light_color[2] = directional_light_color.b;
    scene_uniforms.directional_light_color[3] = directional_light_intensity;


    scene_uniforms.ambient_light[0] = ambient_light_color.r;
    scene_uniforms.ambient_light[1] = ambient_light_color.g;
    scene_uniforms.ambient_light[2] = ambient_light_color.b;
    scene_uniforms.ambient_light[3] = ambient_light_intensity;


    scene_uniforms.light_counts[0] = static_cast<std::uint32_t>(point_lights.size());
    scene_uniforms.light_counts[1] = static_cast<std::uint32_t>(spot_lights.size());


    VkResult result = vmaCopyMemoryToAllocation(
        context.allocator,
        &scene_uniforms,
        scene_uniform_allocation,
        0,
        sizeof(scene_uniforms)
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot update scene uniform buffer\n";
        return false;
    }

    // One host-visible dynamic UBO; padding separates device-aligned slots.
    std::vector<unsigned char> model_data(
        static_cast<std::size_t>(model_uniform_stride) * scene_objects.size(), 0
    );
    for (std::size_t i = 0; i < scene_objects.size(); ++i) {
        const auto& object = scene_objects[i];
        const float t = object.animate_transform ? animation_time : 0.0f;
        const glm::vec3 position = object.position + glm::vec3(
            animation_amplitude.x * std::sin(t),
            animation_amplitude.y * std::sin(2.0f * t),
            animation_amplitude.z * std::sin(3.0f * t)
        );
        const glm::vec3 angles = glm::radians(
            object.rotation_degrees + animation_rotation_speed * t
        );
        glm::mat4 rotation(1.0f);
        rotation = glm::rotate(rotation, angles.z, glm::vec3(0.0f, 0.0f, 1.0f));
        rotation = glm::rotate(rotation, angles.y, glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, angles.x, glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), position)
            * rotation * glm::scale(glm::mat4(1.0f), object.scale);

        ModelUniforms model_uniforms{};
        std::memcpy(model_uniforms.model, glm::value_ptr(model), sizeof(model_uniforms.model));
        std::memcpy(model_uniforms.base_color, glm::value_ptr(object.base_color), 3 * sizeof(float));
        model_uniforms.base_color[3] = 1.0f;
        std::memcpy(model_uniforms.specular, glm::value_ptr(object.specular_color), 3 * sizeof(float));
        model_uniforms.specular[3] = object.shininess;
        std::memcpy(
            model_data.data() + static_cast<std::size_t>(i * model_uniform_stride),
            &model_uniforms, sizeof(model_uniforms)
        );
    }

    result = vmaCopyMemoryToAllocation(
        context.allocator,
        model_data.data(),
        model_uniform_allocation,
        0,
        model_data.size()
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot update model uniform buffer\n";
        return false;
    }

    return true;

}

VkVertexInputBindingDescription getVertexBinding() {
    VkVertexInputBindingDescription binding{};

    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding;
}


VkVertexInputAttributeDescription getPositionAttribute() {
    VkVertexInputAttributeDescription attribute{};

    attribute.location = 0;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute.offset = offsetof(Vertex, position);

    return attribute;
}


VkVertexInputAttributeDescription getTexcoordAttribute() {
    VkVertexInputAttributeDescription attribute{};

    attribute.location = 1;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32_SFLOAT;
    attribute.offset = offsetof(Vertex, texcoord);

    return attribute;
}


VkVertexInputAttributeDescription getNormalAttribute() {
    VkVertexInputAttributeDescription attribute{};

    attribute.location = 2;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute.offset = offsetof(Vertex, normal);

    return attribute;
}


bool createPipeline(){
    auto& context = graphics::internal::context;

    VkPipelineShaderStageCreateInfo stages[2]{};


    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex_shader;
    stages[0].pName = "main";


    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment_shader;
    stages[1].pName = "main";


    auto binding = getVertexBinding();
    VkVertexInputAttributeDescription attributes[] = {
        getPositionAttribute(),
        getTexcoordAttribute(),
        getNormalAttribute()
    };


    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attributes;


    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;


    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };


    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;


    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization.lineWidth = 1.0f;


    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    
    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_TRUE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS;


    VkPipelineColorBlendAttachmentState color_attachment{};
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;


    VkPipelineColorBlendStateCreateInfo blending{};
    blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blending.attachmentCount = 1;
    blending.pAttachments = &color_attachment;


    VkDescriptorSetLayout set_layouts[] = {
        scene_descriptor_set_layout,
        model_descriptor_set_layout
    };

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    
    VkPushConstantRange sampling_range{};
    sampling_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    sampling_range.size = sizeof(SamplingPushConstants);
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &sampling_range;


    VkResult result = vkCreatePipelineLayout(
        context.device,
        &layout_info,
        nullptr,
        &pipeline_layout
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create pipeline layout\n";
        return false;
    }


    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertex_input;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &rasterization;
    info.pMultisampleState = &multisampling;
    info.pDepthStencilState = &depth;
    info.pColorBlendState = &blending;
    info.pDynamicState = &dynamic;
    info.layout = pipeline_layout;
    info.renderPass = context.render_pass;
    info.subpass = 0;


    result = vkCreateGraphicsPipelines(
        context.device,
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &graphics_pipeline
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create graphics pipeline\n";
        return false;
    }

    stages[0].module = light_vertex_shader;
    stages[1].module = light_fragment_shader;
    vertex_input.vertexAttributeDescriptionCount = 1;

    result = vkCreateGraphicsPipelines(
        context.device,
        VK_NULL_HANDLE,
        1,
        &info,
        nullptr,
        &light_pipeline
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create light pipeline\n";
        return false;
    }

    stages[0].module = spot_light_vertex_shader;

    result = vkCreateGraphicsPipelines(
        context.device, VK_NULL_HANDLE, 1, &info, nullptr, &spot_light_pipeline
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create spot light marker pipeline\n";
        return false;
    }

    stages[0].module = spot_cone_vertex_shader;
    // Cone vertices are generated by the shader using gl_VertexIndex.
    vertex_input.vertexBindingDescriptionCount = 0;
    vertex_input.vertexAttributeDescriptionCount = 0;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    depth.depthWriteEnable = VK_FALSE;

    result = vkCreateGraphicsPipelines(
        context.device, VK_NULL_HANDLE, 1, &info, nullptr, &spot_cone_pipeline
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create spot light cone pipeline\n";
        return false;
    }

    return true;

}

bool createDescriptorSetLayout(VkDescriptorSetLayout& result_layout, bool is_scene) {
    auto& context = graphics::internal::context;
    VkDescriptorSetLayoutBinding bindings[4]{};
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = is_scene
        ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    const std::uint32_t count = is_scene ? 3u : 4u;
    for (std::uint32_t i = 1; i < count; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].descriptorType = is_scene
            ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].stageFlags = is_scene
            ? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = count;
    info.pBindings = bindings;
    return vkCreateDescriptorSetLayout(
        context.device, &info, nullptr, &result_layout
    ) == VK_SUCCESS;
}

bool createDescriptorSets() {
    auto& context = graphics::internal::context;
    const auto material_count = static_cast<std::uint32_t>(materials.size());
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, material_count},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, material_count * 3}
    };
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = material_count + 1;
    pool_info.poolSizeCount = 4;
    pool_info.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(context.device, &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
        return false;
    }

    std::vector<VkDescriptorSetLayout> layouts(material_count + 1, model_descriptor_set_layout);
    layouts[0] = scene_descriptor_set_layout;
    std::vector<VkDescriptorSet> sets(material_count + 1);
    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = descriptor_pool;
    allocate_info.descriptorSetCount = material_count + 1;
    allocate_info.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(context.device, &allocate_info, sets.data()) != VK_SUCCESS) return false;
    scene_descriptor_set = sets[0];

    VkDescriptorBufferInfo buffers[] = {
        {scene_uniform_buffer, 0, sizeof(SceneUniforms)},
        {point_light_buffer, 0, VK_WHOLE_SIZE},
        {spot_light_buffer, 0, VK_WHOLE_SIZE},
        // The descriptor range is ONE record, not the whole dynamic array.
        {model_uniform_buffer, 0, sizeof(ModelUniforms)}
    };
    VkWriteDescriptorSet scene_writes[3]{};
    for (std::uint32_t i = 0; i < 3; ++i) {
        scene_writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        scene_writes[i].dstSet = scene_descriptor_set;
        scene_writes[i].dstBinding = i;
        scene_writes[i].descriptorCount = 1;
        scene_writes[i].descriptorType = i == 0
            ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        scene_writes[i].pBufferInfo = &buffers[i];
    }
    vkUpdateDescriptorSets(context.device, 3, scene_writes, 0, nullptr);

    for (std::uint32_t i = 0; i < material_count; ++i) {
        auto& material = materials[i];
        material.descriptor_set = sets[i + 1];
        VkDescriptorImageInfo images[] = {
            {texture_sampler, material.albedo.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {texture_sampler, material.specular.view ? material.specular.view : black_texture.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
            {texture_sampler, material.emissive.view ? material.emissive.view : black_texture.view,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
        };
        VkWriteDescriptorSet writes[4]{};
        for (std::uint32_t binding = 0; binding < 4; ++binding) {
            writes[binding].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[binding].dstSet = material.descriptor_set;
            writes[binding].dstBinding = binding;
            writes[binding].descriptorCount = 1;
            writes[binding].descriptorType = binding == 0
                ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if (binding == 0) writes[binding].pBufferInfo = &buffers[3];
            else writes[binding].pImageInfo = &images[binding - 1];
        }
        vkUpdateDescriptorSets(context.device, 4, writes, 0, nullptr);
    }
    return true;
}

bool initialize() {
    scene_objects.back().uv_scale = 6.0f;
	
    vertex_shader = loadShader(SHADER_DIR "/triangle.vert.spv");
    // vertex_shader = loadShader("shaders/triangle.vert.spv");

    if (vertex_shader == VK_NULL_HANDLE) {
        return false;
    }

    fragment_shader = loadShader(SHADER_DIR "/triangle.frag.spv");
    // fragment_shader = loadShader("shaders/triangle.frag.spv");

    if (fragment_shader == VK_NULL_HANDLE) {
        vkDestroyShaderModule(
            graphics::internal::context.device,
            vertex_shader,
            nullptr
        );

        vertex_shader = VK_NULL_HANDLE;
        return false;
    }

    light_vertex_shader = loadShader(SHADER_DIR "/light.vert.spv");

    if (light_vertex_shader == VK_NULL_HANDLE) {
        shutdown();
        return false;
    }

    light_fragment_shader = loadShader(SHADER_DIR "/light.frag.spv");

    if (light_fragment_shader == VK_NULL_HANDLE) {
        shutdown();
        return false;
    }

    spot_light_vertex_shader = loadShader(SHADER_DIR "/spot_light.vert.spv");

    if (spot_light_vertex_shader == VK_NULL_HANDLE) {
        shutdown();
        return false;
    }

    spot_cone_vertex_shader = loadShader(SHADER_DIR "/spot_cone.vert.spv");

    if (spot_cone_vertex_shader == VK_NULL_HANDLE) {
        shutdown();
        return false;
    }

    if (!createVertexBuffer()) {
        shutdown();
        return false;
    }

    if (!createIndexBuffer()) {
        shutdown();
        return false;
    }

    if (!createUniformBuffer(
            sizeof(SceneUniforms),
            scene_uniform_buffer,
            scene_uniform_allocation)){
        shutdown();
        return false;
    }

    if (!createModelUniformBuffer()) {
        shutdown();
        return false;
    }

    if (!createPointLightBuffer()){
        shutdown();
        return false;
    }

    if (!createSpotLightBuffer()) {
        shutdown();
        return false;
    }

    if (!createMaterials()) {
        shutdown();
        return false;
    }

    if (!createDescriptorSetLayout(
            scene_descriptor_set_layout,
            true)) {
        shutdown();
        return false;
    }

    if (!createDescriptorSetLayout(
            model_descriptor_set_layout,
            false)) {
        shutdown();
        return false;
    }

    if (!createDescriptorSets()) {
        shutdown();
        return false;
    }

    if (!createPipeline()){
        shutdown();
        return false;
    }

    return true;
}


void shutdown() {

	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

    vkDestroyPipeline(context.device, spot_cone_pipeline, nullptr);
    vkDestroyPipeline(context.device, spot_light_pipeline, nullptr);
    vkDestroyPipeline(context.device, light_pipeline, nullptr);

    vkDestroyPipeline(
        context.device, 
        graphics_pipeline, 
        nullptr
    );

    vkDestroyPipelineLayout(
        context.device, 
        pipeline_layout, 
        nullptr
    );

    vkDestroyDescriptorPool(
        context.device,
        descriptor_pool,
        nullptr
    );

    vkDestroyDescriptorSetLayout(
        context.device,
        scene_descriptor_set_layout,
        nullptr
    );

    vkDestroyDescriptorSetLayout(
        context.device,
        model_descriptor_set_layout,
        nullptr
    );

    for (auto& material : materials) {
        texture::destroy(material.albedo);
        texture::destroy(material.specular);
        texture::destroy(material.emissive);
        material.descriptor_set = VK_NULL_HANDLE;
    }
    texture::destroy(black_texture);
    vkDestroySampler(context.device, texture_sampler, nullptr);
    texture_sampler = VK_NULL_HANDLE;

    if (scene_uniform_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
        context.allocator,
        scene_uniform_buffer,
        scene_uniform_allocation
        );
    }

    if (model_uniform_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
        context.allocator,
        model_uniform_buffer,
        model_uniform_allocation
        );
    }

    if (point_light_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            context.allocator,
            point_light_buffer,
            point_light_allocation
        );
    }

    if (spot_light_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(
            context.allocator,
            spot_light_buffer,
            spot_light_allocation
        );
    }

    vmaDestroyBuffer(
        context.allocator,
        vertex_buffer,
        vertex_allocation
    );

    vmaDestroyBuffer(
        context.allocator,
        index_buffer,
        index_allocation
    );


    vkDestroyShaderModule(context.device, vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, fragment_shader, nullptr);
    vkDestroyShaderModule(context.device, light_vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, light_fragment_shader, nullptr);
    vkDestroyShaderModule(context.device, spot_light_vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, spot_cone_vertex_shader, nullptr);

    graphics_pipeline = VK_NULL_HANDLE;
    light_pipeline = VK_NULL_HANDLE;
    spot_light_pipeline = VK_NULL_HANDLE;
    spot_cone_pipeline = VK_NULL_HANDLE;
    pipeline_layout = VK_NULL_HANDLE;

    scene_descriptor_set_layout = VK_NULL_HANDLE;
    model_descriptor_set_layout = VK_NULL_HANDLE;

    scene_uniform_buffer = VK_NULL_HANDLE;
    model_uniform_buffer = VK_NULL_HANDLE;

    scene_uniform_allocation = VK_NULL_HANDLE;
    model_uniform_allocation = VK_NULL_HANDLE;

    scene_descriptor_set = VK_NULL_HANDLE;

    vertex_buffer = VK_NULL_HANDLE;
    vertex_allocation = VK_NULL_HANDLE;

    index_buffer = VK_NULL_HANDLE;
    index_allocation = VK_NULL_HANDLE;

    fragment_shader = VK_NULL_HANDLE;
    vertex_shader = VK_NULL_HANDLE;
    light_vertex_shader = VK_NULL_HANDLE;
    light_fragment_shader = VK_NULL_HANDLE;
    spot_light_vertex_shader = VK_NULL_HANDLE;
    spot_cone_vertex_shader = VK_NULL_HANDLE;

    descriptor_pool = VK_NULL_HANDLE;

    point_light_buffer = VK_NULL_HANDLE;
    point_light_allocation = VK_NULL_HANDLE;
    spot_light_buffer = VK_NULL_HANDLE;
    spot_light_allocation = VK_NULL_HANDLE;

}

void update(double time, const CameraInput& camera_input){
    
    static double previous_time = time;

    const float delta_time = std::clamp(
        static_cast<float>(time - previous_time),
        0.0f,
        0.1f
    );

    previous_time = time;

    const float mouse_sensitivity = glm::radians(0.1f);

    camera_yaw += camera_input.mouse_delta_x * mouse_sensitivity;
    camera_pitch += camera_input.mouse_delta_y * mouse_sensitivity;

    camera_yaw = std::remainder(
        camera_yaw,
        glm::radians(360.0f)
    );

    camera_pitch = std::clamp(
        camera_pitch,
        glm::radians(-85.0f),
        glm::radians(85.0f)
    );

    const glm::vec3 world_up{0.0f, 1.0f, 0.0f};

    const glm::vec3 forward = getCameraForward();

    const glm::vec3 right = glm::normalize(
        glm::cross(forward, world_up)
    );

    glm::vec3 movement = forward * camera_input.move_forward + right * camera_input.move_right;

    if (glm::dot(movement, movement) > 0.0f) {
        movement = glm::normalize(movement);
    }

    const float movement_speed = 2.5f;

    camera_position += movement * movement_speed * delta_time;

    if (sampling_playing) {
        sampling_time += delta_time;
    }

    ImGui::SetNextWindowSize(ImVec2(410.0f, 880.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Scene settings")) {
        if (ImGui::BeginCombo("Object", scene_objects[selected_object].name)) {
            for (std::size_t i = 0; i < scene_objects.size(); ++i) {
                const bool selected = selected_object == static_cast<int>(i);
                if (ImGui::Selectable(scene_objects[i].name, selected)) {
                    selected_object = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
        auto& object = scene_objects[selected_object];
        auto& object_position = object.position;
        auto& object_rotation_degrees = object.rotation_degrees;
        auto& object_scale = object.scale;
        auto& object_color = object.base_color;
        auto& object_specular_color = object.specular_color;
        auto& object_shininess = object.shininess;

        const char* projection_names[] ={
            "Perspective",
            "Orthographic"
        };

        ImGui::Combo(
            "Projection",
            &projection_type,
            projection_names,
            2
        );


        ImGui::Separator();
        ImGui::Text("Camera");

        ImGui::DragFloat3(
            "Camera position",
            glm::value_ptr(camera_position),
            0.05f
        );

        ImGui::SliderAngle(
            "Camera yaw",
            &camera_yaw,
            -180.0f,
            180.0f
        );

        ImGui::SliderAngle(
            "Camera pitch",
            &camera_pitch,
            -85.0f,
            85.0f
        );

        ImGui::SliderFloat(
            "Camera FOV",
            &camera_fov_degrees,
            20.0f,
            100.0f,
            "%.1f",
            ImGuiSliderFlags_AlwaysClamp
        );

        if (ImGui::Button("Reset camera")) {
            camera_position = glm::vec3(0.0f, 1.4f, 6.5f);

            camera_yaw = glm::radians(-90.0f);
            camera_pitch = glm::radians(-10.0f);

            camera_fov_degrees = 45.0f;
        }


        ImGui::Separator();
        ImGui::Text("Object transform");

        ImGui::DragFloat3(
            "Position",
            glm::value_ptr(object_position),
            0.01f
        );

        ImGui::SliderFloat3(
            "Rotation (degrees)",
            glm::value_ptr(object_rotation_degrees),
            -180.0f,
            180.0f
        );

        ImGui::SliderFloat3(
            "Scale",
            glm::value_ptr(object_scale),
            0.1f,
            3.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        if (ImGui::Button("Reset transform")) {
            object_position = initial_scene_objects[selected_object].position;
            object_rotation_degrees = initial_scene_objects[selected_object].rotation_degrees;
            object_scale = initial_scene_objects[selected_object].scale;
        }

        ImGui::Separator();
        ImGui::Text("Animation");
        ImGui::Checkbox("Play animation", &animation_playing);
        ImGui::Checkbox("Animate selected object", &object.animate_transform);

        ImGui::SliderFloat(
            "Animation speed",
            &animation_speed,
            0.1f,
            3.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::SliderFloat3(
            "Movement amplitude",
            glm::value_ptr(animation_amplitude),
            0.0f,
            1.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::DragFloat3(
            "Rotation speed (deg/s)",
            glm::value_ptr(animation_rotation_speed),
            1.0f
        );

        if (ImGui::Button("Reset animation")) {
            animation_time = 0.0f;
            animation_playing = false;
        }

        ImGui::Separator();
        ImGui::Text("Material");

        if (ImGui::BeginCombo("Texture material", materials[object.material_index].name)) {
            for (std::size_t i = 0; i < materials.size(); ++i) {
                const bool selected = object.material_index == static_cast<int>(i);
                if (ImGui::Selectable(materials[i].name, selected)) {
                    object.material_index = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
        const auto& material = materials[object.material_index];
        ImGui::Text("Specular map: %s", material.specular_path ? "yes" : "none");
        ImGui::Text("Emissive map: %s", material.emissive_path ? "yes" : "none");
        ImGui::SliderFloat(
            "Texture repeat", &object.uv_scale, 0.25f, 10.0f,
            "%.2f", ImGuiSliderFlags_AlwaysClamp
        );
        ImGui::Checkbox("Animated texture sampling", &object.animated_sampling);
        ImGui::Checkbox("Play texture animation", &sampling_playing);
        ImGui::SliderFloat(
            "Effect strength", &object.sampling_strength, 0.0f, 1.0f,
            "%.2f", ImGuiSliderFlags_AlwaysClamp
        );
        ImGui::SliderFloat(
            "Effect speed", &object.sampling_speed, 0.1f, 3.0f,
            "%.2f", ImGuiSliderFlags_AlwaysClamp
        );
        ImGui::SliderFloat(
            "Emission strength", &object.emissive_strength, 0.0f, 5.0f,
            "%.2f", ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::ColorEdit3(
            "Base color",
            glm::value_ptr(object_color)
        );

        ImGui::ColorEdit3(
            "Specular color",
            glm::value_ptr(object_specular_color)
        );

        ImGui::SliderFloat(
            "Shininess",
            &object_shininess,
            1.0f,
            256.0f,
            "%.1f",
            ImGuiSliderFlags_AlwaysClamp
        );


        ImGui::Separator();
        ImGui::Text("Directional light");

        ImGui::DragFloat3(
            "Light ray direction",
            glm::value_ptr(directional_light_direction),
            0.02f
        );

        ImGui::ColorEdit3(
            "Light color",
            glm::value_ptr(directional_light_color)
        );

        ImGui::SliderFloat(
            "Light intensity",
            &directional_light_intensity,
            0.0f,
            3.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::Separator();
        ImGui::Text("Ambient light");

        ImGui::ColorEdit3(
            "Ambient color",
            glm::value_ptr(ambient_light_color)
        );

        ImGui::SliderFloat(
            "Ambient intensity",
            &ambient_light_intensity,
            0.0f,
            1.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::Separator();
        ImGui::Text("Point lights");

        ImGui::Checkbox("Show point light markers", &show_light_sources);

        ImGui::BeginDisabled(point_lights.size() >= MAX_POINT_LIGHTS);

        if (ImGui::Button("Add point light")) {
            point_lights.emplace_back();
        }

        ImGui::EndDisabled();

        int point_light_to_remove = -1;

        for (std::size_t i = 0; i < point_lights.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            auto& light = point_lights[i];

            ImGui::Separator();
            ImGui::Text("Point light %d", static_cast<int>(i + 1));

            ImGui::DragFloat3(
                "Position",
                glm::value_ptr(light.position),
                0.05f
            );

            ImGui::ColorEdit3(
                "Color",
                glm::value_ptr(light.color)
            );

            ImGui::SliderFloat(
                "Intensity",
                &light.intensity,
                0.0f,
                20.0f,
                "%.2f",
                ImGuiSliderFlags_AlwaysClamp
            );

            if (ImGui::Button("Delete point light")) {
                point_light_to_remove = static_cast<int>(i);
            }

            ImGui::PopID();
        }

        if (point_light_to_remove >= 0) {

            point_lights.erase(point_lights.begin() + point_light_to_remove);

        }

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Spot lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushID("spot_lights");

            ImGui::Checkbox("Show spot light markers", &show_spot_light_sources);
            ImGui::Checkbox("Show spot light cones", &show_spot_light_cones);
            ImGui::Text("Spot lights: %d / %d",
                static_cast<int>(spot_lights.size()),
                static_cast<int>(MAX_SPOT_LIGHTS));

            ImGui::BeginDisabled(spot_lights.size() >= MAX_SPOT_LIGHTS);
            if (ImGui::Button("Add spot light")) {
                spot_lights.emplace_back();
            }
            ImGui::EndDisabled();

            int spot_light_to_remove = -1;
            for (std::size_t i = 0; i < spot_lights.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                auto& light = spot_lights[i];

                if (ImGui::TreeNodeEx("settings", ImGuiTreeNodeFlags_DefaultOpen,
                        "Spot light %d", static_cast<int>(i + 1))) {
                    ImGui::DragFloat3("Position", glm::value_ptr(light.position), 0.05f);
                    ImGui::DragFloat3("Direction", glm::value_ptr(light.direction), 0.02f);

                    if (ImGui::Button("Aim at origin")) {
                        light.direction = -light.position;
                    }
                    if (ImGui::Button("Place at camera")) {
                        light.position = camera_position;
                        light.direction = getCameraForward();
                    }

                    ImGui::ColorEdit3("Color", glm::value_ptr(light.color));
                    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 20.0f,
                        "%.2f", ImGuiSliderFlags_AlwaysClamp);

                    ImGui::TextDisabled("Angles are measured from the cone axis.");
                    ImGui::SliderFloat("Outer angle (deg)", &light.outer_angle_degrees,
                        1.0f, 89.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
                    light.inner_angle_degrees = std::clamp(
                        light.inner_angle_degrees, 0.0f, light.outer_angle_degrees);
                    ImGui::SliderFloat("Inner angle (deg)", &light.inner_angle_degrees,
                        0.0f, light.outer_angle_degrees, "%.1f", ImGuiSliderFlags_AlwaysClamp);

                    ImGui::SliderFloat("Cone display length", &light.cone_display_length,
                        0.2f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::TextWrapped("Display length only changes the guide lines, not the light range.");

                    if (ImGui::Button("Delete spot light")) {
                        spot_light_to_remove = static_cast<int>(i);
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }

            if (spot_light_to_remove >= 0) {
                spot_lights.erase(spot_lights.begin() + spot_light_to_remove);
            }
            ImGui::PopID();
        }

    }

    ImGui::End();

    if (animation_playing){
        animation_time += delta_time * animation_speed;
    }

}

void render(const graphics::internal::FrameData& fd) {
	
    auto& context = graphics::internal::context;
    VkCommandBuffer command_buffer = fd.command_buffer;

    const bool point_lights_ready = updatePointLightBuffer();
    const bool spot_lights_ready = updateSpotLightBuffer();
    const bool uniforms_ready = point_lights_ready && spot_lights_ready && updateUniformBuffers();

    vkResetCommandBuffer(command_buffer, 0);
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;


    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkClearValue clear_values[2]{};
    clear_values[0].color.float32[0] = 0.05f;
    clear_values[0].color.float32[1] = 0.05f;
    clear_values[0].color.float32[2] = 0.10f;
    clear_values[0].color.float32[3] = 1.0f;
    clear_values[1].depthStencil.depth = 1.0f;


    VkRenderPassBeginInfo pass_info{};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_info.renderPass = context.render_pass;
    pass_info.framebuffer = fd.framebuffer;
    pass_info.renderArea.extent = context.swapchain_extent;
    pass_info.clearValueCount = 2;
    pass_info.pClearValues = clear_values;


    vkCmdBeginRenderPass(
        command_buffer,
        &pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );


    VkViewport viewport{};
    viewport.width = static_cast<float>(context.swapchain_extent.width);
    viewport.height = static_cast<float>(context.swapchain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;


    VkRect2D scissor{};
    scissor.extent = context.swapchain_extent;


    vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);


    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics_pipeline
    );


    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(
        command_buffer,
        0,
        1,
        &vertex_buffer,
        &offset
    );

    vkCmdBindIndexBuffer(
        command_buffer,
        index_buffer,
        0,
        VK_INDEX_TYPE_UINT16
    );


    if (uniforms_ready) {
        for (std::size_t i = 0; i < scene_objects.size(); ++i) {
            const auto& object = scene_objects[i];
            VkDescriptorSet sets[] = {
                scene_descriptor_set,
                materials[object.material_index].descriptor_set
            };
            const auto dynamic_offset = static_cast<std::uint32_t>(i * model_uniform_stride);
            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout,
                0,
                2,
                sets,
                1,
                &dynamic_offset
            );

            SamplingPushConstants sampling{};
            sampling.time = sampling_time * object.sampling_speed;
            sampling.strength = object.sampling_strength;
            sampling.uv_scale = object.uv_scale;
            sampling.flags = object.animated_sampling ? 1u : 0u;
            sampling.emissive_strength = object.emissive_strength;
            vkCmdPushConstants(
                command_buffer,
                pipeline_layout,
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(sampling),
                &sampling
            );

            vkCmdDrawIndexed(
                command_buffer,
                static_cast<std::uint32_t>(cube_mesh.indices.size()),
                1,
                0,
                0,
                0
            );
        }
    }

    if (uniforms_ready && show_light_sources && !point_lights.empty()) {
        vkCmdBindPipeline(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            light_pipeline
        );

        vkCmdBindDescriptorSets(
            command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout,
            0,
            1,
            &scene_descriptor_set,
            0,
            nullptr
        );

        vkCmdDrawIndexed(
            command_buffer,
            static_cast<std::uint32_t>(cube_mesh.indices.size()),
            static_cast<std::uint32_t>(point_lights.size()),
            0,
            0,
            0
        );
    }

    if (uniforms_ready && !spot_lights.empty()
        && (show_spot_light_sources || show_spot_light_cones)) {
        vkCmdBindDescriptorSets(
            command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline_layout, 0, 1, &scene_descriptor_set, 0, nullptr
        );

        if (show_spot_light_sources) {
            vkCmdBindPipeline(
                command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spot_light_pipeline
            );
            vkCmdDrawIndexed(
                command_buffer,
                static_cast<std::uint32_t>(cube_mesh.indices.size()),
                static_cast<std::uint32_t>(spot_lights.size()), 0, 0, 0
            );
        }

        if (show_spot_light_cones) {
            vkCmdBindPipeline(
                command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spot_cone_pipeline
            );
            vkCmdDraw(
                command_buffer, SPOT_CONE_VERTEX_COUNT,
                static_cast<std::uint32_t>(spot_lights.size()), 0, 0
            );
        }
    }

    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);

}

} // namespace application
