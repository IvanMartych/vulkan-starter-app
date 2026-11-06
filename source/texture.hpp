#pragma once

#include "graphics_internal.hpp"
#include <cstdint>

namespace texture {

struct Texture {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mip_levels = 1;
};

bool createDeviceBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                        VkBuffer& buffer, VmaAllocation& allocation);
bool load(const char* path, bool srgb, Texture& result);
bool createBlack(Texture& result);
bool createSampler(VkSampler& result);
void destroy(Texture& texture);

} // namespace texture
