#pragma once

#include "def.h"
#include <GLFW/glfw3.h>

struct GLFWwindow;

namespace ovk {
	class Surface;

	class OVK_API EventListener {
	public:
		virtual ~EventListener() = default;

		EventListener();
		
		EventListener(const EventListener& other);
		EventListener(EventListener&& other) noexcept;

		EventListener& operator=(const EventListener& other);
		EventListener& operator=(EventListener&& other) noexcept;

	private:
		friend class EventManager;
		
		virtual void on_key(int key_code, int scancode, int action, int mods) {}
		virtual void on_char(uint32_t codepoint) {}
		virtual void on_cursor(double xpos, double ypos) {}
		virtual void on_cursor_enter(bool entered) {}
		virtual void on_mouse_button(int button, int action, int mods) {}
		virtual void on_scroll(double xoffset, double yoffset) {}
	};
	
	class OVK_API EventManager {
	private:
		EventManager();
	public:
		EventManager(const EventManager &other) = delete;
		EventManager(EventManager &&other) noexcept = delete;
		EventManager & operator=(const EventManager &other) = delete;
		EventManager & operator=(EventManager &&other) noexcept = delete;
		
		static EventManager& get();

		void init(Surface &s);

		void add_listener(EventListener* listener);
		void remove_listener(EventListener* listener);

		std::array<bool, GLFW_KEY_LAST> keys;
		glm::vec2 mouse_pos;
		
		GLFWwindow* window = nullptr;
		
	protected:
		static void callback_key(GLFWwindow* window, int key_code, int scancode, int action, int mods);
		static void callback_char(GLFWwindow* window, uint32_t codepoint);
		static void callback_cursor(GLFWwindow* window, double xpos, double ypos);
		static void callback_cursor_enter(GLFWwindow* window, int entered);
		static void callback_mouse_button(GLFWwindow* window, int button, int action, int mods);
		static void callback_scroll(GLFWwindow* window, double xoffset, double yoffset);

		std::vector<EventListener*> listeners;

		
	};
	
}
