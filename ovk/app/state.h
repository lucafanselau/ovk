#pragma once

#include "def.h"

namespace ovk {

	class Application;


	class OVK_API GameState {
	public:

		virtual ~GameState() = default;

		// May be deferred
		virtual void on_init() = 0;

		virtual bool on_update(float dt) = 0;
		virtual void on_render(float dt) = 0;

		virtual void on_close(GameState* next_state) {};
		virtual void on_open(GameState* previous_state) {};

		virtual bool is_valid_transition(std::string_view  next) = 0;

		static std::string get_type() { return "undefined"; }

		Application* parent{};
	};

	class OVK_API StateManager {
	public:
		StateManager();

		template <typename T, typename... Args>
		T* add_state(Args&& ... args);

		bool is_valid(std::string_view next) const;

		bool load(std::string_view state_type);

		bool update(float dt) const;
		void render(float dt) const;

	private:
		std::unordered_map<std::string, GameState*> states;
		GameState* active = nullptr;
	};


	template <typename T, typename ... Args>
	T* StateManager::add_state(Args&& ... args) {
		static_assert(std::is_base_of_v<GameState, T>, "[StateManager] (add_state) T must be derived from GameState");
		// Check if state map already contains that entry
		if (states.find(T::get_type()) != states.end() && T::get_type() == "undefined") {
			spdlog::error("[StateManager] (add_state) State already existed and Derived Class must override fn get_type()");
			return nullptr;
		}

		// Add to map
		auto [it, result] = states.insert(std::make_pair(T::get_type(), new T(std::forward<Args>(args)...)));
		if (!result) spdlog::error("[StateManager] (add_state) Insertion failed!");

		return reinterpret_cast<T*>(it->second);

	}
}
