#pragma once

#include "def.h"
#include "event.h"

namespace ovk {
	class SwapChain;


	// This is the GLSL Compatible Camera Data
	struct OVK_API CameraData {
		glm::mat4 view;
		glm::mat4 projection;

		friend bool operator==(const CameraData &lhs, const CameraData &rhs) {
			return lhs.view == rhs.view
				&& lhs.projection == rhs.projection;
		}

		friend bool operator!=(const CameraData &lhs, const CameraData &rhs) {
			return !(lhs == rhs);
		}
	};

	class OVK_API FirstPersonCamera : public EventListener {
	public:
		FirstPersonCamera(glm::vec3 pos, SwapChain& swap_chain);

		CameraData& get_data();
		~FirstPersonCamera() override = default;

		void update(float delta_time, bool draw_imgui = false);

		glm::vec3 pos, dir;
		
	private:

		float yaw = -90.f, pitch = 0.f;
		float fov = 45.0f;
		float aspect;

		bool focus = true;

		glm::vec2 last_mouse;
		glm::vec2 swap_extent;

		GLFWwindow* window;
		
		void on_key(int key_code, int scancode, int action, int mods) override;
		void on_cursor(double xpos, double ypos) override;
		void on_scroll(double xoffset, double yoffset) override;
		
		void calculate_data();
		
		
		CameraData data;
	};
	
}
