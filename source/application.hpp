#pragma once

#include "graphics_internal.hpp"

namespace application {

bool initialize();
void shutdown();

void update(double time, float camera_horizontal, float camera_vertical);
void render(const graphics::internal::FrameData& fd);

} // namespace application