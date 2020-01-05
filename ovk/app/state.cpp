#include "pch.h"
#include "state.h"

namespace ovk {

	StateManager::StateManager() {

	}

	bool StateManager::is_valid(const std::string_view next) const {
		if (active == nullptr) return true;
		return active->is_valid_transition(next);
	}

	bool StateManager::load(const std::string_view state_type) {
		if (!is_valid(state_type)) return false;
		const auto it = states.find(std::string(state_type));
		if (it == states.end()) {
			spdlog::info("[StateManager] (load) Tried to load State that was not added");
		}
		// Get new State
		const auto new_state = it->second;
		// Callback on old State
		if (active) active->on_close(new_state);
		// Swap States
		auto* old = active;
		active = new_state;
		// Callback on new State
		active->on_open(old);
		// Return
		return true;
	}

	bool StateManager::update(const float dt) const {
		return active->on_update(dt);
	}

	void StateManager::render(const float dt) const {
		active->on_render(dt);
	}

}
