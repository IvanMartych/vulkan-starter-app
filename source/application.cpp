#include "application.hpp"
#include "graphics_internal.hpp"

#include <imgui.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <cstddef>

namespace application {

VkShaderModule vertex_shader = VK_NULL_HANDLE;
VkShaderModule fragment_shader = VK_NULL_HANDLE;
VkBuffer vertex_buffer = VK_NULL_HANDLE;
VmaAllocation vertex_allocation = VK_NULL_HANDLE;
VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline graphics_pipeline = VK_NULL_HANDLE;


struct Vertex {

    float position[3];

};


const Vertex vertices[] = {
    {{0.0f, -0.5f, 0.5f}},
    {{0.5f, 0.5f, 0.5f}},
    {{-0.5f, 0.5f, 0.5f}}
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
        context.device, graphics_pipeline, nullptr
    );

    vkDestroyPipelineLayout(
        context.device, pipeline_layout, nullptr
    );

    vmaDestroyBuffer(
        context.allocator,
        vertex_buffer,
        vertex_allocation
    );


    vkDestroyShaderModule(context.device, vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, fragment_shader, nullptr);    


    graphics_pipeline = VK_NULL_HANDLE;
    pipeline_layout = VK_NULL_HANDLE;
    fragment_shader = VK_NULL_HANDLE;
    vertex_shader = VK_NULL_HANDLE;

}


void update([[maybe_unused]] double time) {
	ImGui::ShowDemoWindow();
}

void render(const graphics::internal::FrameData& fd) {
	
    auto& context = graphics::internal::context;
    VkCommandBuffer command_buffer = fd.command_buffer;


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


    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);
    vkEndCommandBuffer(command_buffer);

}

} // namespace application