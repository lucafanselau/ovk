#include "pch.h"
#include "camera.h"

#include "base/surface.h"
#include "base/swapchain.h"
#include <imgui.h>

namespace ovk {

	const glm::vec3 up(0.f, 1.f, 0.f);
	
	FirstPersonCamera::FirstPersonCamera(glm::vec3 position, SwapChain &swap_chain)
		: pos(position),
			dir(0.0f),
			data{},
			aspect(static_cast<float>(swap_chain.swap_extent.width) / static_cast<float>(swap_chain.swap_extent.height)),
			last_mouse(swap_chain.swap_extent.width, swap_chain.swap_extent.height),
			swap_extent(swap_chain.swap_extent.width, swap_chain.swap_extent.height) {

		auto& event_manager = EventManager::get();
		ovk_assert(event_manager.window, "EventManager::init() must be called before creating the camera");
		
		window = EventManager::get().window;
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		
		calculate_data();
	}

	CameraData & FirstPersonCamera::get_data() {
		return data;
	}

	void FirstPersonCamera::on_key(int key_code, int scancode, int action, int mods) {
		
		if (key_code == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
			focus = !focus;
			if (focus) {
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				glfwSetCursorPos(window, last_mouse.x, last_mouse.y);
			}
			else {
				last_mouse = swap_extent / 2.f;
				glfwSetCursorPos(window, last_mouse.x, last_mouse.y);
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
		}

		
	
	}

	void FirstPersonCamera::on_cursor(double xpos, double ypos) {

		glm::vec2 mouse(xpos, ypos);

		static auto first_mouse = true;
		if (first_mouse) {
			last_mouse = mouse;
			first_mouse = false;
		}
		
		if (focus) {
			auto offset = mouse - last_mouse;
			offset.y *= -1.f;

			const float sensitivity = 0.05f;
			offset *= sensitivity;

			yaw += offset.x; pitch += offset.y;

			if (pitch > 89.f) pitch = 89.f;
			if (pitch < -89.f) pitch = -89.f;
		}

		last_mouse = mouse;
	}

	void FirstPersonCamera::on_scroll(double xoffset, double yoffset) {

		if (!focus) return;
		
		if (fov >= 1.0f && fov <= 45.f)
			fov -= yoffset;
		if (fov <= 1.0) fov = 1.0f;
		if (fov >= 45.f) fov = 45.f;
	}

	void FirstPersonCamera::calculate_data() {
 

		glm::vec3 front;
		front.x = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
		front.y = sin(glm::radians(pitch));
		front.z = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
		dir = glm::normalize(front);
		
		data.view = glm::lookAt(pos, pos + dir, up);
		data.projection = glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.f);

		data.projection[1][1] *= -1.f;
		
	}


	void FirstPersonCamera::update(float delta_time, bool draw_imgui) {
		// Com
		const auto speed = 15.f * delta_time;
		

		if (focus) {
			auto& keys = EventManager::get().keys;
			if (keys[GLFW_KEY_W]) pos += speed * dir;
			if (keys[GLFW_KEY_S]) pos -= speed * dir;
			if (keys[GLFW_KEY_A]) pos -= glm::normalize(glm::cross(dir, up)) * speed;
			if (keys[GLFW_KEY_D]) pos += glm::normalize(glm::cross(dir, up)) * speed;

			if (keys[GLFW_KEY_SPACE]) pos += up * speed;
			if (keys[GLFW_KEY_LEFT_SHIFT]) pos -= up * speed;
			
		}

		calculate_data();

		// TODO: Maybe abstract that to some sort of (reflection?) ImGuiDrawer
		if (draw_imgui) {
			ImGui::Begin("FirstPersonCamera");
			{
				ImGui::Text("yaw: %f, pitch %f", yaw, pitch);
				ImGui::Text("pos: {%f, %f, %f}", pos.x, pos.y, pos.z);
				ImGui::Text("dir: {%f, %f, %f}", dir.x, dir.y, dir.z);

				ImGui::Text("mouse_pos: {%f, %f}", last_mouse.x, last_mouse.y);
				
			}
			ImGui::End();
		}
		
	}
	
}
