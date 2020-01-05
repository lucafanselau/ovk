#include "pch.h"
#include "surface.h"
#include "app/event.h"

namespace ovk {

	Surface::Surface(int width, int height, std::string title, bool init_events, vk::Instance* parent)
		: window(static_cast<GLFWwindow**>(nullptr), {}), surface(nullptr, ObjectDestroy<vk::SurfaceKHR>(*parent))
	{

		//GLFW INIT
		
		// GLFW: Settings
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		auto glfw_window = glfwCreateWindow(width, height, "vk-craft!", nullptr, nullptr);
		if (!glfw_window)
			spdlog::error("failed to created window");

		window.set(std::move(glfw_window));

		{
			VkSurfaceKHR tmp_surface;
			const auto err = glfwCreateWindowSurface(*parent, window, nullptr, &tmp_surface);
			if (err)
				spdlog::error("window surface creation failed");
			surface.set(std::move(tmp_surface));
		}

		if (init_events) {
			EventManager::get().init(*this);
		}
		

	}
	bool Surface::update() {
		glfwPollEvents();

		return !glfwWindowShouldClose(static_cast<GLFWwindow*>(window));
	}

	void Surface::wait_minimal() {
	}
}
