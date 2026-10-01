#include "application.hpp"
#include "graphics_internal.hpp"

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

namespace application {

VkShaderModule vertex_shader = VK_NULL_HANDLE;
VkShaderModule fragment_shader = VK_NULL_HANDLE;
VkBuffer vertex_buffer = VK_NULL_HANDLE;
VkBuffer index_buffer = VK_NULL_HANDLE;
VkBuffer uniform_buffer = VK_NULL_HANDLE;
VmaAllocation vertex_allocation = VK_NULL_HANDLE;
VmaAllocation index_allocation = VK_NULL_HANDLE;
VmaAllocation uniform_allocation = VK_NULL_HANDLE;
VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline graphics_pipeline = VK_NULL_HANDLE;
VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
VkDescriptorSet descriptor_set = VK_NULL_HANDLE;


float camera_yaw = glm::radians(45.0f);
float camera_pitch = glm::radians(28.0f);
float camera_distance = 3.2f;


struct Vertex {

    float position[3];

};


struct alignas(16) GlobalUniforms{
    float matrix[4][4];
};


const Vertex vertices[] = {
    {{-0.5f, -0.5f, -0.5f}}, // 0
    {{ 0.5f, -0.5f, -0.5f}}, // 1
    {{ 0.5f,  0.5f, -0.5f}}, // 2
    {{-0.5f,  0.5f, -0.5f}}, // 3
    {{-0.5f, -0.5f,  0.5f}}, // 4
    {{ 0.5f, -0.5f,  0.5f}}, // 5
    {{ 0.5f,  0.5f,  0.5f}}, // 6
    {{-0.5f,  0.5f,  0.5f}}  // 7
};


const std::uint16_t indices[] = {
    4, 5, 6,  6, 7, 4, // передняя грань
    1, 0, 3,  3, 2, 1, // задняя грань
    0, 4, 7,  7, 3, 0, // левая грань
    5, 1, 2,  2, 6, 5, // правая грань
    7, 6, 2,  2, 3, 7, // верхняя грань
    0, 1, 5,  5, 4, 0  // нижняя грань 
};


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
    buffer_info.size = sizeof(vertices);
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
        vertices,
        vertex_allocation,
        0,
        sizeof(vertices)
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
    buffer_info.size = sizeof(indices);
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
        indices,
        index_allocation,
        0,
        sizeof(indices)
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


bool createUniformBuffer(){

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
        &uniform_buffer,
        &uniform_allocation,
        nullptr
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create uniform buffer\n";
        return false;
    }


    return true;

}


bool updateUniformBuffer(){

    auto& context = graphics::internal::context;

    const auto extent = context.swapchain_extent;

    if (extent.width == 0 || extent.height == 0) {
        return false;
    }


    const auto aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const glm::mat4 model(1.0f);
    
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

    glm::mat4 projection = glm::perspectiveRH_ZO(
        glm::radians(45.0f), // вертикальный угол обзора
        aspect,             // ширина окна / высота окна
        0.1f,               // ближняя граница видимости
        100.0f              // дальняя граница видимости
    );
    
    projection[1][1] *= -1.0f;
    const glm::mat4 matrix = projection * view * model;

    GlobalUniforms uniforms{};
    std::memcpy(
        uniforms.matrix,
        glm::value_ptr(matrix),
        sizeof(uniforms.matrix)
    );


    VkResult result = vmaCopyMemoryToAllocation(
        context.allocator,
        &uniforms,
        uniform_allocation,
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
    auto attribute = getPositionAttribute();


    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &attribute;


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


bool createDescriptorSet(){

    auto& context = graphics::internal::context;


    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = 1;


    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;


    VkResult result = vkCreateDescriptorPool(
        context.device,
        &pool_info,
        nullptr,
        &descriptor_pool
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot create descriptor pool\n";
        return false;
    }


    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;


    result = vkAllocateDescriptorSets(
        context.device,
        &allocate_info,
        &descriptor_set
    );


    if (result != VK_SUCCESS) {
        std::cerr << "Cannot allocate descriptor set\n";
        return false;
    }


    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffer;
    buffer_info.offset = 0;
    buffer_info.range = sizeof(GlobalUniforms);


    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = descriptor_set;
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

    if (!createUniformBuffer()){
        shutdown();
        return false;
    }

    if (!createDescriptorSetLayout()){
        shutdown();
        return false;
    }

    if (!createDescriptorSet()) {
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

    vmaDestroyBuffer(
        context.allocator,
        uniform_buffer,
        uniform_allocation
    );

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
    uniform_buffer = VK_NULL_HANDLE;
    uniform_allocation = VK_NULL_HANDLE;
    vertex_buffer = VK_NULL_HANDLE;
    vertex_allocation = VK_NULL_HANDLE;
    index_buffer = VK_NULL_HANDLE;
    index_allocation = VK_NULL_HANDLE;
    fragment_shader = VK_NULL_HANDLE;
    vertex_shader = VK_NULL_HANDLE;
    descriptor_pool = VK_NULL_HANDLE;
    descriptor_set = VK_NULL_HANDLE;


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

    ImGui::ShowDemoWindow();

}

void render(const graphics::internal::FrameData& fd) {
	
    auto& context = graphics::internal::context;
    VkCommandBuffer command_buffer = fd.command_buffer;
    const bool uniforms_ready = updateUniformBuffer();

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

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline_layout,
        0,
        1,
        &descriptor_set,
        0,
        nullptr
    );

    if (uniforms_ready) {
        vkCmdDrawIndexed(command_buffer, 36, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);

}

} // namespace application