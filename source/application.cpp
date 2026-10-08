#include "application.hpp"
#include "graphics_internal.hpp"

#include <cstdint>
#include <imgui.h>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>


#include <array>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <string>

namespace application {

VkShaderModule vertex_shader = VK_NULL_HANDLE;
VkShaderModule fragment_shader = VK_NULL_HANDLE;
VkBuffer vertex_buffer = VK_NULL_HANDLE;
VkBuffer index_buffer = VK_NULL_HANDLE;
VmaAllocation vertex_allocation = VK_NULL_HANDLE;
VmaAllocation index_allocation = VK_NULL_HANDLE;
VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline graphics_pipeline = VK_NULL_HANDLE;
VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;



float camera_yaw = glm::radians(45.0f);
float camera_pitch = glm::radians(28.0f);
float camera_distance = 3.2f;
int projection_type = 0;


// glm::vec3 object.position(0.0f);
// glm::vec3 object.rotation_degrees(0.0f);
// glm::vec3 object.scale(1.0f);

// bool object.animation_playing = false;
// float object.animation_time = 0.0f;
// float object.animation_speed = 1.0f;
// glm::vec3 object.animation_amplitude(0.6f, 0.25f, 0.4f);
// glm::vec3 object.animation_rotation_speed(20.0f, 40.0f, 10.0f);


struct SceneObject {
    glm::vec3 position{0.0f};
    glm::vec3 rotation_degrees{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec3 color{1.0f, 1.0f, 1.0f};

    bool animation_playing = false;
    float animation_time = 0.0f;
    float animation_speed = 1.0f;

    glm::vec3 animation_amplitude{0.6f, 0.25f, 0.4f};
    glm::vec3 animation_rotation_speed{20.0f, 40.0f, 10.0f};

    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VmaAllocation uniform_allocation = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
};

constexpr std::size_t MAX_OBJECTS = 100;

std::array<SceneObject, MAX_OBJECTS> objects{};
std::size_t object_count = 2;

int selected_object = 0;


struct Vertex {

    float position[3];
    float color[3];

};


struct alignas(16) GlobalUniforms{
    
    float matrix[4][4];
    float base_color[4];

};


// const Vertex vertices[] = {
//     {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}}, // 0: чёрный
//     {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // 1: красный
//     {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}, // 2: жёлтый
//     {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // 3: зелёный
//     {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // 4: синий
//     {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}}, // 5: пурпурный
//     {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}, // 6: белый
//     {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}}  // 7: голубой
// };


// const std::uint16_t indices[] = {
//     4, 5, 6,  6, 7, 4, // передняя грань
//     1, 0, 3,  3, 2, 1, // задняя грань
//     0, 4, 7,  7, 3, 0, // левая грань
//     5, 1, 2,  2, 6, 5, // правая грань
//     7, 6, 2,  2, 3, 7, // верхняя грань
//     0, 1, 5,  5, 4, 0  // нижняя грань 
// };


struct MeshData {

    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;

};


MeshData generateCube(float side_length) {

    MeshData mesh;

    const float half_size = side_length * 0.5f;


    for (int z = -1; z <= 1; z += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int x = -1; x <= 1; x += 2) {

                mesh.vertices.push_back({
                    {
                        x * half_size,
                        y * half_size,
                        z * half_size
                    },
                    {
                        (x + 1) * 0.5f,
                        (y + 1) * 0.5f,
                        (z + 1) * 0.5f
                    }
                });

            }
        }
    }


    const std::uint16_t faces[6][4] = {
        {4, 5, 7, 6}, // передняя
        {1, 0, 2, 3}, // задняя
        {0, 4, 6, 2}, // левая
        {5, 1, 3, 7}, // правая
        {6, 7, 3, 2}, // верхняя
        {0, 1, 5, 4}  // нижняя
    };


    for (const auto& face : faces) {
        mesh.indices.insert(
            mesh.indices.end(),
            {
                face[0], face[1], face[2],
                face[2], face[3], face[0]
            }
        );
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


bool createVertexBuffer(){

    auto& context = graphics::internal::context;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = cube_mesh.vertices.size() * sizeof(Vertex);
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkResult result = vmaCreateBuffer(
        context.allocator,
        &buffer_info,
        &allocation_info,
        &vertex_buffer,
        &vertex_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create vertex buffer\n";
        return false;
    }

    result = vmaCopyMemoryToAllocation(
        context.allocator,
        cube_mesh.vertices.data(),
        vertex_allocation,
        0,
        buffer_info.size
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot copy vertices\n";

        vmaDestroyBuffer(
            context.allocator,
            vertex_buffer,
            vertex_allocation
        );

        vertex_buffer = VK_NULL_HANDLE;
        vertex_allocation = VK_NULL_HANDLE;

        return false;
    }

    return true;
}


bool createIndexBuffer(){

    auto& context = graphics::internal::context;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = cube_mesh.indices.size() * sizeof(std::uint16_t);
    buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;


    VkResult result = vmaCreateBuffer(
        context.allocator,
        &buffer_info,
        &allocation_info,
        &index_buffer,
        &index_allocation,
        nullptr
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create index buffer\n";
        return false;
    }


    result = vmaCopyMemoryToAllocation(
        context.allocator,
        cube_mesh.indices.data(),
        index_allocation,
        0,
        buffer_info.size
    );

    if (result != VK_SUCCESS) {
        std::cerr << "Cannot copy indices\n";
        vmaDestroyBuffer(context.allocator, index_buffer, index_allocation);
        index_buffer = VK_NULL_HANDLE;
        index_allocation = VK_NULL_HANDLE;
        return false;
    }

    return true;

}


bool createUniformBuffer(SceneObject& object){

    auto& context = graphics::internal::context;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = sizeof(GlobalUniforms);
    buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;


    VmaAllocationCreateInfo allocation_info{};
    allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
    allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkResult result = vmaCreateBuffer(
        context.allocator,
        &buffer_info,
        &allocation_info,
        &object.uniform_buffer,
        &object.uniform_allocation,
        nullptr
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create uniform buffer\n";
        return false;
    }


    return true;

}


bool updateUniformBuffer(const SceneObject& object){

    auto& context = graphics::internal::context;

    const auto extent = context.swapchain_extent;

    if (extent.width == 0 || extent.height == 0) {
        return false;
    }


    const auto aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    // const glm::mat4 model(1.0f);
    // const glm::mat4 translation = glm::translate(
    //     glm::mat4(1.0f),
    //     object.position
    // );

    const float t = object.animation_time;
    const glm::vec3 animation_offset(
        object.animation_amplitude.x * std::sin(t),
        object.animation_amplitude.y * std::sin(2.0f * t),
        object.animation_amplitude.z * std::sin(3.0f * t)
    );

    const glm::vec3 animated_position = object.position + animation_offset;
    const glm::mat4 translation = glm::translate(
        glm::mat4(1.0f),
        animated_position
    );


    // const glm::vec3 angles = glm::radians(object.rotation_degrees);
    const glm::vec3 animated_rotation_degrees = object.rotation_degrees + object.animation_rotation_speed * object.animation_time;
    const glm::vec3 angles = glm::radians(animated_rotation_degrees);

    glm::mat4 rotation(1.0f);

    rotation = glm::rotate(
        rotation,
        angles.z,
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    rotation = glm::rotate(
        rotation,
        angles.y,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    rotation = glm::rotate(
        rotation,
        angles.x,
        glm::vec3(1.0f, 0.0f, 0.0f)
    );

    const glm::mat4 scaling = glm::scale(
        glm::mat4(1.0f),
        object.scale
    );

    const glm::mat4 model = translation * rotation * scaling;
    
    const float horizontal_radius = camera_distance * std::cos(camera_pitch);
    const glm::vec3 camera_position(
        horizontal_radius * std::sin(camera_yaw),
        camera_distance * std::sin(camera_pitch),
        horizontal_radius * std::cos(camera_yaw)
    );

    const glm::mat4 view = glm::lookAtRH(
        camera_position,
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 projection(1.0f);

    if (projection_type == 0){

        projection = glm::perspectiveRH_ZO(
            glm::radians(45.0f), // вертикальный угол обзора
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
    const glm::mat4 matrix = projection * view * model;

    GlobalUniforms uniforms{};
    std::memcpy(
        uniforms.matrix,
        glm::value_ptr(matrix),
        sizeof(uniforms.matrix)
    );

    uniforms.base_color[0] = object.color.r;
    uniforms.base_color[1] = object.color.g;
    uniforms.base_color[2] = object.color.b;
    uniforms.base_color[3] = 1.0f;


    VkResult result = vmaCopyMemoryToAllocation(
        context.allocator,
        &uniforms,
        object.uniform_allocation,
        0,
        sizeof(uniforms)
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot update uniform buffer\n";
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


VkVertexInputAttributeDescription getColorAttribute() {
    VkVertexInputAttributeDescription attribute{};

    attribute.location = 1;
    attribute.binding = 0;
    attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute.offset = offsetof(Vertex, color);

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
        getColorAttribute()
    };


    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 2;
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


    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &descriptor_set_layout;


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

    return true;

}


bool createDescriptorSetLayout(){

    auto& context = graphics::internal::context;

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;


    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;


    VkResult result = vkCreateDescriptorSetLayout(
        context.device,
        &layout_info,
        nullptr,
        &descriptor_set_layout
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create descriptor set layout\n";
        return false;
    }


    return true;
}


bool createDescriptorPool() {

    auto& context = graphics::internal::context;

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = static_cast<std::uint32_t>(MAX_OBJECTS);

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = static_cast<std::uint32_t>(MAX_OBJECTS);
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    const VkResult result = vkCreateDescriptorPool(
        context.device,
        &pool_info,
        nullptr,
        &descriptor_pool
    );

    if (result != VK_SUCCESS){
        std::cerr << "Cannot create descriptor pool\n";
        return false;
    }

    return true;

}


bool createDescriptorSet(SceneObject& object) {

    auto& context = graphics::internal::context;

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;


    const VkResult result = vkAllocateDescriptorSets(
        context.device,
        &allocate_info,
        &object.descriptor_set
    );


    if (result != VK_SUCCESS){
        std::cerr << "Cannot allocate descriptor set\n";
        return false;
    }


    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = object.uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(GlobalUniforms);


    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = object.descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(
        context.device,
        1,
        &write,
        0,
        nullptr
    );

    return true;

}

bool initialize() {
	
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

    if (!createVertexBuffer()) {
        shutdown();
        return false;
    }

    if (!createIndexBuffer()) {
        shutdown();
        return false;
    }

    if (!createDescriptorSetLayout()) {
        shutdown();
        return false;
    }

    if (!createDescriptorPool()) {
        shutdown();
        return false;
    }


    objects[0].position = glm::vec3(-0.7f, 0.0f, 0.0f);
    objects[0].scale = glm::vec3(0.6f);
    objects[0].color = glm::vec3(1.0f, 0.3f, 0.3f);

    objects[1].position = glm::vec3(0.7f, 0.0f, 0.0f);
    objects[1].scale = glm::vec3(0.6f);
    objects[1].color = glm::vec3(0.3f, 0.5f, 1.0f);


    for (std::size_t i = 0; i < object_count; ++i) {
        auto& object = objects[i];

        if (!createUniformBuffer(object)) {
            shutdown();
            return false;
        }

        if (!createDescriptorSet(object)) {
            shutdown();
            return false;
        }
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
        descriptor_set_layout,
        nullptr
    );

    for (std::size_t i = 0; i < object_count; ++i) {
        auto& object = objects[i];

        if (object.uniform_buffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(
                context.allocator,
                object.uniform_buffer,
                object.uniform_allocation
            );
        }

        object.uniform_buffer = VK_NULL_HANDLE;
        object.uniform_allocation = VK_NULL_HANDLE;
        object.descriptor_set = VK_NULL_HANDLE;
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

    graphics_pipeline = VK_NULL_HANDLE;
    pipeline_layout = VK_NULL_HANDLE;
    descriptor_set_layout = VK_NULL_HANDLE;
    vertex_buffer = VK_NULL_HANDLE;
    vertex_allocation = VK_NULL_HANDLE;
    index_buffer = VK_NULL_HANDLE;
    index_allocation = VK_NULL_HANDLE;
    fragment_shader = VK_NULL_HANDLE;
    vertex_shader = VK_NULL_HANDLE;
    descriptor_pool = VK_NULL_HANDLE;

}

void update(double time, float camera_horizontal, float camera_vertical){
    
    static double previous_time = time;

    const float delta_time = std::clamp(
        static_cast<float>(time - previous_time),
        0.0f,
        0.1f
    );

    previous_time = time;

    const float camera_speed = glm::radians(90.0f);

    camera_yaw += camera_horizontal * camera_speed * delta_time;
    camera_pitch += camera_vertical * camera_speed * delta_time;

    camera_pitch = std::clamp(
        camera_pitch,
        glm::radians(-85.0f),
        glm::radians(85.0f)
    );

    if (ImGui::Begin("Scene settings")) {

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
        ImGui::Text("Select object");

        for (std::size_t i = 0; i < object_count; i++) {
            const std::string label =
                "Cube " + std::to_string(i + 1);

            if (ImGui::Selectable(
                    label.c_str(),
                    selected_object == static_cast<int>(i))) {
                selected_object = static_cast<int>(i);
            }
        }

        auto& object = objects[selected_object];

        ImGui::Separator();
        ImGui::Text("Object transform");

        ImGui::DragFloat3(
            "Position",
            glm::value_ptr(object.position),
            0.01f
        );

        ImGui::SliderFloat3(
            "Rotation (degrees)",
            glm::value_ptr(object.rotation_degrees),
            -180.0f,
            180.0f
        );

        ImGui::SliderFloat3(
            "Scale",
            glm::value_ptr(object.scale),
            0.1f,
            3.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        if (ImGui::Button("Reset transform")) {
            object.position = glm::vec3(0.0f);
            object.rotation_degrees = glm::vec3(0.0f);
            object.scale = glm::vec3(1.0f);
        }

        ImGui::Separator();
        ImGui::Text("Animation");
        ImGui::Checkbox("Play animation", &object.animation_playing);

        ImGui::SliderFloat(
            "Animation speed",
            &object.animation_speed,
            0.1f,
            3.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::SliderFloat3(
            "Movement amplitude",
            glm::value_ptr(object.animation_amplitude),
            0.0f,
            1.0f,
            "%.2f",
            ImGuiSliderFlags_AlwaysClamp
        );

        ImGui::DragFloat3(
            "Rotation speed (deg/s)",
            glm::value_ptr(object.animation_rotation_speed),
            1.0f
        );

        if (ImGui::Button("Reset animation")) {
            object.animation_time = 0.0f;
            object.animation_playing = false;
        }

        ImGui::Separator();
        ImGui::Text("Object color");

        ImGui::ColorEdit3(
            "Base color",
            glm::value_ptr(object.color)
        );


    }

    ImGui::End();

    for (std::size_t i = 0; i < object_count; ++i){

        auto& object = objects[i];

        if (object.animation_playing){
            object.animation_time +=delta_time * object.animation_speed;
        }
    }

}

void render(const graphics::internal::FrameData& fd) {
	
    auto& context = graphics::internal::context;
    VkCommandBuffer command_buffer = fd.command_buffer;
    bool uniforms_ready = true;

    for (std::size_t i = 0; i < object_count; ++i) {
        if (!updateUniformBuffer(objects[i])) {
            uniforms_ready = false;
        }
    }

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

    // vkCmdBindDescriptorSets(
    //     command_buffer,
    //     VK_PIPELINE_BIND_POINT_GRAPHICS,
    //     pipeline_layout,
    //     0,
    //     1,
    //     &objects[0].descriptor_set,
    //     0,
    //     nullptr
    // );

    // if (uniforms_ready) {
    //     vkCmdDrawIndexed(command_buffer, 36, 1, 0, 0, 0);
    // }

    if (uniforms_ready) {
        for (std::size_t i = 0; i < object_count; ++i) {
            const auto& object = objects[i];

            vkCmdBindDescriptorSets(
                command_buffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout,
                0,
                1,
                &object.descriptor_set,
                0,
                nullptr
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

    
    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);

}

} // namespace application
