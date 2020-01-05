#pragma once

#include <noise/noise.h>
#include "../graphics/mesh.h"

constexpr uint32_t chunk_size = 8;

enum class TileType {
	base,
	street
};

struct Chunk {

	Chunk(glm::vec2 p, noise::module::Module* noise_module, ovk::Device& device); 

	void build_mesh(ovk::Device& device);
	
	// Get height of chunk_local coordinates
	float get_height(int x, int z) const;
	TileType get_type(int x, int z) const;
	void set_height(int x, int z, float value);
	void set_type(int x, int z, TileType new_type);
	
	glm::vec2 pos;
	std::array<float, (chunk_size + 1) * (chunk_size + 1)> heightmap;
	std::array<TileType, chunk_size * chunk_size> types;
	std::unique_ptr<TerrainMesh> mesh, picker_mesh;
	
};
