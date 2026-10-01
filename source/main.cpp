#include <cstdint>
#include <climits>

#include <iostream>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>

#include "graphics_internal.hpp"
#include "application.hpp"

namespace {

constexpr int32_t default_window_width = 1920;
constexpr int32_t default_window_height = 1080;

constexpr char default_window_title[] = "Vulkan Starter App";

GLFWwindow* glfw_window;

} // namespace

int main() {

	int status = EXIT_SUCCESS;

	if (!glfwInit()) {
		std::cerr << "Failed to initialize GLFW\n";
		return EXIT_FAILURE;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	glfw_window = glfwCreateWindow(default_window_width, default_window_height,
	                               default_window_title, nullptr, nullptr);
	if (glfw_window == nullptr) {
		status = EXIT_FAILURE;
		goto err_null_window;
	}

	glfwSetFramebufferSizeCallback(glfw_window, [](GLFWwindow*, int width, int height){
		if (width == 0 || height == 0) {
			return;
		}

		graphics::internal::resize(width, height);
	});

	if (ImGui::CreateContext() == nullptr) {
		std::cerr << "Failed to create ImGUI context\n";
		status = EXIT_FAILURE;
		goto err_imgui_init;
	}

	if (!ImGui_ImplGlfw_InitForVulkan(glfw_window, true)) {
		std::cerr << "Failed to initialize ImGUI GLFW backend for Vulkan renderer\n";
		status = EXIT_FAILURE;
		goto err_imgui_glfw_init;
	}

	if (!graphics::internal::initialize(glfw_window)) {
		std::cerr << "Failed to initialize graphics\n";
		status = EXIT_FAILURE;
		goto err_graphics_init;
	}

	if (!application::initialize()) {
		std::cerr << "Failed to initialize application\n";
		status = EXIT_FAILURE;
		goto err_application_init;
	}

	while (!glfwWindowShouldClose(glfw_window)) {
		const double time = glfwGetTime();

		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();

		ImGui::NewFrame();

        float camera_horizontal = 0.0f;
        float camera_vertical = 0.0f;


        if (!ImGui::GetIO().WantCaptureKeyboard) {
            if (glfwGetKey(glfw_window, GLFW_KEY_A) == GLFW_PRESS) {
                camera_horizontal -= 1.0f;
            }

            if (glfwGetKey(glfw_window, GLFW_KEY_D) == GLFW_PRESS) {
                camera_horizontal += 1.0f;
            }

            if (glfwGetKey(glfw_window, GLFW_KEY_W) == GLFW_PRESS) {
                camera_vertical += 1.0f;
            }

            if (glfwGetKey(glfw_window, GLFW_KEY_S) == GLFW_PRESS) {
                camera_vertical -= 1.0f;
            }
        }


        application::update(time, camera_horizontal, camera_vertical);
        ImGui::Render();

		graphics::internal::FrameData fd = graphics::internal::prepare();
		application::render(fd);
		graphics::internal::submitAndPresent();
	}

	application::shutdown();
err_application_init:
	graphics::internal::shutdown();
err_graphics_init:
	ImGui_ImplGlfw_Shutdown();
err_imgui_glfw_init:
	ImGui::DestroyContext();
err_imgui_init:
	glfwDestroyWindow(glfw_window);
err_null_window:
	glfwTerminate();

	return 0;
}