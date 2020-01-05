#pragma once

#include <base/device.h>
#include "app/camera.h"
#include "graphics/mesh.h"
#include "utils/noise.h"
#include "graphics/renderer.h"
#include "world/world.h"

struct Game : ovk::EventListener {

	Game(std::shared_ptr<ovk::Device> d, std::shared_ptr<ovk::SwapChain> sc, std::shared_ptr<MasterRenderer> r);

	void create_const_objects();
	void create_sc_objects();

	void calculate_terrain();
	
	void render();
	
	void update(float dt, int index);

	[[ deprecated ]]
	void recreate_swapchain();

	~Game() override = default;
private:
	void on_mouse_button(int button, int action, int mods) override;
public:
	std::shared_ptr<ovk::Device> device;
	std::shared_ptr<ovk::SwapChain> swapchain;
	std::shared_ptr<MasterRenderer> renderer;

	// Const
	NoiseBuilder noise;
	ModelManager models;
	// std::vector<std::unique_ptr<Mesh>> meshes;
	// std::vector<Transform> tree_locations;
	std::vector<Entity> entities;
	float scale = 0.5f;
	int32_t world_extent;
	int8_t brush_type = 0;
	int brush_size = 1;

	std::string current_model = "";

	// Terrain
	Terrain terrain;
	//std::vector<Chunk> chunks;
	
};
