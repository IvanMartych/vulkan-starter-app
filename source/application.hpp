#pragma once

#include "graphics_internal.hpp"

namespace application {

struct CameraInput {

    float move_forward = 0.0f;
    float move_right = 0.0f;

    float mouse_delta_x = 0.0f;
    float mouse_delta_y = 0.0f;
    
};

bool initialize();
void shutdown();

void update(double time, const CameraInput& camera_input);
void render(const graphics::internal::FrameData& fd);

} // namespace application