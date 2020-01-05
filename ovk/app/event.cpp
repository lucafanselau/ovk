#include "pch.h"
#include "event.h"

#include "base/surface.h"

namespace ovk {
	EventManager::EventManager() : keys() {}

	EventManager& EventManager::get()
	{
		static EventManager manager;
		return manager;
	}

	EventListener::EventListener() {
		EventManager::get().add_listener(this);
	}

	EventListener::EventListener(const EventListener &other) {
		EventManager::get().add_listener(this);
	}

	EventListener::EventListener(EventListener &&other) noexcept {
		auto& manager = EventManager::get();
		manager.remove_listener(&other);
		manager.add_listener(this);
	}

	EventListener & EventListener::operator=(const EventListener &other) {
		EventManager::get().add_listener(this);
		return *this;
	}

	EventListener & EventListener::operator=(EventListener &&other) noexcept {
		auto& manager = EventManager::get();
		manager.remove_listener(&other);
		manager.add_listener(this);
		return *this;
	}

	void EventManager::add_listener(EventListener *listener) {
		listeners.push_back(listener);
	}

	void EventManager::remove_listener(EventListener *listener) {
		listeners.erase(std::remove(std::begin(listeners), std::end(listeners), listener), std::end(listeners));
	}

	void EventManager::init(Surface &s) {
		
		GLFWwindow* w = s.window.get();
		window = w;


		glfwSetKeyCallback(w, callback_key);
		glfwSetCharCallback(w, callback_char);
		glfwSetCursorPosCallback(w, callback_cursor);
		glfwSetCursorEnterCallback(w, callback_cursor_enter);
		glfwSetMouseButtonCallback(w, callback_mouse_button);
		glfwSetScrollCallback(w, callback_scroll);
	}


	void EventManager::callback_key(GLFWwindow *window, int key_code, int scancode, int action, int mods) {
		auto& manager = get();
		ovk_assert(manager.window == window, "Event Manager can currently only support one window!");
		manager.keys[key_code] = action != GLFW_RELEASE;
		for (auto l : manager.listeners) {
			l->on_key(key_code, scancode, action, mods);
		}
	}

	void EventManager::callback_char(GLFWwindow *window, uint32_t codepoint) {
		auto& manager = get();
		ovk_assert(manager.window == window, "Event Manager can currently only support one window!");
		for (auto l : manager.listeners) {
			l->on_char(codepoint);
		}
	}

	void EventManager::callback_cursor(GLFWwindow *window, double xpos, double ypos) {
		auto& manager = get();
		manager.mouse_pos = glm::vec2(xpos, ypos);
		ovk_assert(manager.window == window, "Event Manager can currently only support one window!");
		for (auto l : manager.listeners) {
			l->on_cursor(xpos, ypos);
		}
	}

	void EventManager::callback_cursor_enter(GLFWwindow *window, int entered) {
		auto& manager = get();
		ovk_assert(manager.window == window, "Event Manager can currently only support one window!");
		for (auto l : manager.listeners) {
			l->on_cursor_enter(entered);
		}
	}

	void EventManager::callback_mouse_button(GLFWwindow *window, int button, int action, int mods) {
		auto& manager = get();
		ovk_assert(manager.window == window, "Event Manager can currently only support one window!");
		for (auto l : manager.listeners) {
			l->on_mouse_button(button, action, mods);
		}
	}

	void EventManager::callback_scroll(GLFWwindow *window, double xoffset, double yoffset) {
		auto& manager = get();
		ovk_assert(manager.window == window, "Event Manager can currently only support one window!");
		for (auto l : manager.listeners) {
			l->on_scroll(xoffset, yoffset);
		}
	}
}
