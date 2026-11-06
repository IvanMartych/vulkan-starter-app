#include "texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace texture {
namespace {

// Upload commands finish before their staging allocations are released.
bool immediateSubmit(const std::function<void(VkCommandBuffer)>& record){

    auto& context = graphics::internal::context;

    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pool_info{};

    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = context.graphics_queue_index;

    if (vkCreateCommandPool(context.device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
        return false;
    }

    VkCommandBuffer command = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo alloc{};

    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    bool success = vkAllocateCommandBuffers(context.device, &alloc, &command) == VK_SUCCESS;

    if (success){

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        success = vkBeginCommandBuffer(command, &begin) == VK_SUCCESS;

        if (success){
            record(command);
            success = vkEndCommandBuffer(command) == VK_SUCCESS;
        }
    }

    if (success) {
        VkFenceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        success = vkCreateFence(context.device, &info, nullptr, &fence) == VK_SUCCESS;
    }

    if (success) {
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        success = vkQueueSubmit(context.graphics_queue, 1, &submit, fence) == VK_SUCCESS;
        if (success) success = vkWaitForFences(context.device, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    }

    vkDestroyFence(context.device, fence, nullptr);
    vkDestroyCommandPool(context.device, pool, nullptr);

    if (!success) {
        std::cerr << "GPU upload failed\n";
    }

    return success;
}


bool createStaging(const void* pixels, VkDeviceSize size, VkBuffer& buffer, VmaAllocation& allocation){

    auto& context = graphics::internal::context;

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    memory.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    if (vmaCreateBuffer(context.allocator, &info, &memory, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        return false;
    }

    if (vmaCopyMemoryToAllocation(context.allocator, pixels, allocation, 0, size) != VK_SUCCESS) {
        vmaDestroyBuffer(context.allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        return false;
    }

    return true;

}


void imageBarrier(VkCommandBuffer command, VkImage image, std::uint32_t first,
                  std::uint32_t count, VkImageLayout old_layout, VkImageLayout new_layout,
                  VkAccessFlags src_access, VkAccessFlags dst_access,
                  VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage){
        
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, first, count, 0, 1};
    vkCmdPipelineBarrier(command, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
}



bool uploadImage(const unsigned char* pixels, std::uint32_t width, std::uint32_t height,
                 bool srgb, Texture& result) {

    auto& context = graphics::internal::context;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(context.physical_device, &properties);
    if (!width || !height || width > properties.limits.maxImageDimension2D
        || height > properties.limits.maxImageDimension2D) return false;

    const VkFormat format = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    VkFormatProperties support{};
    vkGetPhysicalDeviceFormatProperties(context.physical_device, format, &support);
    const VkFormatFeatureFlags mip_features = VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        | VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
    const bool can_blit = (support.optimalTilingFeatures & mip_features) == mip_features;
    result.width = width;
    result.height = height;
    result.mip_levels = can_blit
        ? static_cast<std::uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1 : 1;

    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation staging_memory = VK_NULL_HANDLE;
    const VkDeviceSize size = VkDeviceSize(width) * height * 4;

    if (!createStaging(pixels, size, staging, staging_memory)) {
        return false;
    }

    VkImageCreateInfo image{};
    image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image.imageType = VK_IMAGE_TYPE_2D;
    image.extent = {width, height, 1};
    image.mipLevels = result.mip_levels;
    image.arrayLayers = 1;
    image.format = format;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    if (result.mip_levels > 1) image.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    memory.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bool success = vmaCreateImage(context.allocator, &image, &memory,
        &result.image, &result.allocation, nullptr) == VK_SUCCESS;

    if (success) success = immediateSubmit([&](VkCommandBuffer command) {
        imageBarrier(command, result.image, 0, result.mip_levels,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(command, staging, result.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        std::int32_t mip_width = static_cast<std::int32_t>(width);
        std::int32_t mip_height = static_cast<std::int32_t>(height);
        for (std::uint32_t level = 1; level < result.mip_levels; ++level) {

            imageBarrier(command, result.image, level - 1, 1,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
            const auto next_width = std::max(mip_width / 2, 1);
            const auto next_height = std::max(mip_height / 2, 1);

            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, 1};
            blit.srcOffsets[1] = {mip_width, mip_height, 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
            blit.dstOffsets[1] = {next_width, next_height, 1};

            vkCmdBlitImage(command, result.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                result.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            imageBarrier(command, result.image, level - 1, 1,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            mip_width = next_width;
            mip_height = next_height;
        }

        imageBarrier(command, result.image, result.mip_levels - 1, 1,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    });
    vmaDestroyBuffer(context.allocator, staging, staging_memory);

    if (success) {
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = result.image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, result.mip_levels, 0, 1};
        success = vkCreateImageView(context.device, &view, nullptr, &result.view) == VK_SUCCESS;
    }
    if (!success) {
        destroy(result);
    }
    return success;
}

} // namespace


bool createDeviceBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                        VkBuffer& buffer, VmaAllocation& allocation) {

    auto& context = graphics::internal::context;

    VkBuffer staging = VK_NULL_HANDLE;
    VmaAllocation staging_memory = VK_NULL_HANDLE;
    if (!createStaging(data, size, staging, staging_memory)) {
        return false;
    }

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo memory{};
    memory.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    memory.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    
    bool success = vmaCreateBuffer(context.allocator, &info, &memory, &buffer, &allocation, nullptr) == VK_SUCCESS;
    if (success) success = immediateSubmit([&](VkCommandBuffer command) {
        VkBufferCopy copy{0, 0, size};
        vkCmdCopyBuffer(command, staging, buffer, 1, &copy);
        VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
            ? VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT : VK_ACCESS_INDEX_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = buffer;
        barrier.size = size;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
    });

    vmaDestroyBuffer(context.allocator, staging, staging_memory);
    if (!success && buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(context.allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
    return success;
}

bool load(const char* path, bool srgb, Texture& result){

    int width = 0, height = 0, channels = 0;
    auto* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
        std::cerr << "Cannot load texture " << path << ": " << stbi_failure_reason() << '\n';
        return false;
    }

    const bool success = uploadImage(pixels, static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height), srgb, result);
    stbi_image_free(pixels);
    
    return success;
}


bool createBlack(Texture& result) {
    const unsigned char pixel[] = {0, 0, 0, 255};
    return uploadImage(pixel, 1, 1, false, result);
}


bool createSampler(VkSampler& result) {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = info.addressModeV = info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.maxLod = VK_LOD_CLAMP_NONE;
    return vkCreateSampler(graphics::internal::context.device, &info, nullptr, &result) == VK_SUCCESS;
}

void destroy(Texture& texture) {
    auto& context = graphics::internal::context;
    vkDestroyImageView(context.device, texture.view, nullptr);
    if (texture.image != VK_NULL_HANDLE) {
        vmaDestroyImage(context.allocator, texture.image, texture.allocation);
    }
    texture = {};
}

} // namespace texture
