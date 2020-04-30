#include "game.h"

#include "world/chunk.h"
#include <imgui.h>
#include <unordered_map>

#include <filesystem>

enum BrushType {
	plane = 0,
	lower = 1,
	raise = 2,
	street = 3
};

Game::Game(std::shared_ptr<ovk::Device> d, std::shared_ptr<ovk::SwapChain> sc, std::shared_ptr<MasterRenderer> r)
	: device(d), swapchain(sc), renderer(r),
		models(device),
    world_extent(3), brush_size(1),
    terrain(device, renderer) {
		
	create_const_objects();
	create_sc_objects();
	
}


void Game::create_const_objects() {

	//	meshes = load_meshes_from_obj("res/models/better_house.obj", *device);
	models.load("tree", "res/models/Lowpoly_tree_sample.obj");
	models.load("house", "res/models/better_house.obj");
	terrain.calculate_terrain(noise.get_module(), world_extent);
}

void Game::create_sc_objects() {
	// TODO: This is not obvious that that needs to be here
	// but actucally we should have some Material System to handle that shit
	renderer->post_init(models.materials.get());
}

void Game::render() {
	terrain.render();

	for (auto& e : entities) {
		renderer->draw_mesh(e.model, e.transform);
	}
	
}

void Game::update(float dt, int index) {

	ImGui::Begin("Game");

	if (ImGui::Button("Reset")) {
		entities.clear();
	}

	ImGui::InputFloat("scale", &scale, 0.01, 0.1);
	auto changed = ImGui::SliderInt("world_extent", &world_extent, 4, 32);

	const char* items[] = { "plane", "lower", "raise", "street" };
	ImGui::Combo("brush_type", (int*) &brush_type, items, IM_ARRAYSIZE(items));

	ImGui::SliderInt("brush_size", &brush_size, 1, 3);

	ImGui::Separator();

	ImGui::Text("Current Model: %s", current_model.c_str());
	
	for (auto &[key, value] : models.models) {
		if (ImGui::Button(key.c_str())) {
			current_model = key;
		}
	}
	
	ImGui::End();

	if (noise.update() || changed) {
		terrain.calculate_terrain(noise.get_module(), world_extent);
	}
}

void Game::recreate_swapchain() {
	create_sc_objects();
}

void Game::on_mouse_button(int button, int action, int mods) {
	if ((button == GLFW_MOUSE_BUTTON_LEFT || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS) {
		std::optional<PickInfo> pick_info = renderer->pick(ovk::EventManager::get().mouse_pos, terrain);
		if (!pick_info.has_value()) return;
		
		if (button == GLFW_MOUSE_BUTTON_LEFT) {

			if (current_model.empty()) {
				spdlog::info("You need to select a building first!");
				return;
			}
			
			// TEMP: This should not be here
			std::unordered_map<std::string, float> scales {
				{ "tree", 0.05f },
				{ "house", 0.5f }
			};
			
			entities.push_back(
				Entity{
					.model = models.get(current_model),
					.transform = Transform {
					  pick_info.value().hit,
					  glm::vec3(scales[current_model]),
						0.0f
					}
				}
			);
		 
		} else {

			auto& pi = pick_info.value();
			auto x = (int) pi.triangle_base.x;
			auto z = (int) pi.triangle_base.z;

			float terraform_speed = 0.25f;

			glm::ivec2 range;
			if (brush_size == 1) range = glm::ivec2(0, 2);
			else if (brush_size == 2) range = glm::ivec2(0, 3);			
			else if (brush_size == 3) range = glm::ivec2(-1, 3);

			auto width = range.y - range.x;
			std::vector<float> new_heights(width * width, 0.0f);
			bool height_changed = false;
			auto height = [&](int i, int j) -> float& {
				height_changed = true;
				return new_heights[(i - range.x) * width + (j - range.x)];
			};
			
			if (brush_type == plane) {
				// Find minimum height and set that for all triangles
				float small_height = pi.triangle_base.y;
				for (auto i = range.x; i < range.y; i++) {
					for (auto j = range.x; j < range.y; j++) {
						// height = std::min(pi.chunk->get_height(x + i, z + j), height);
						small_height = std::min(
							terrain.get_height(x + i, z + j).value_or(small_height),
							small_height
						);
					}
				}
				for (auto i = range.x; i < range.y; i++) {
					for (auto j = range.x; j < range.y; j++) {
						height(i, j) = small_height;
					}
				}
			} else if (brush_type == lower) {
				for (auto i = range.x; i < range.y; i++) {
					for (auto j = range.x; j < range.y; j++) {
						auto h = terrain.get_height(x + i, z + j);
						if (h.has_value())
							height(i, j) = h.value() - terraform_speed;
					}
				}				
			} else if (brush_type == raise) {
				for (auto i = range.x; i < range.y; i++) {
					for (auto j = range.x; j < range.y; j++) {
						auto h = terrain.get_height(x + i, z + j);
						if (h.has_value())
							height(i, j) = h.value() + terraform_speed;
					}
				}				
			} else if (brush_type == street) {
				// This Brush Type will not accept the range argument
				// This will just set the currently clicked tile
				terrain.set_type(x, z, TileType::street);
			}

			if (height_changed) {
				for (auto i = range.x; i < range.y; i++) {
					for (auto j = range.x; j < range.y; j++) {
						// Ok this call needs to be done on the terrain
						// pi.chunk->set_height(x + i, z + j, height);
						terrain.set_height(x + i, z + j, height(i, j));
					}
				}
			}
			// pi.chunk->build_mesh(*device);
			terrain.dispatch_changes();
		}

	}
}
