#pragma once

#include "../graphics/renderer.h"
#include "chunk.h"

#include <glm/gtx/hash.hpp>


struct Terrain {

	Terrain(std::shared_ptr<ovk::Device>& d, std::shared_ptr<MasterRenderer>& r);

	void render();
	
	void calculate_terrain(noise::module::Module* noise_module, int32_t world_extent);

	// NOTE: x and z are counters so the first chunk will be like (0, 0) after that (0, 1) etc.
	// So if u have chunk pos in world coordinates u will need to divide that by chunk_size
	const Chunk* get_chunk(int x, int z) const;
	Chunk* get_chunk(int x, int z);

	std::pair<glm::ivec2, glm::ivec2> parse_location(int x, int z) const;
	// Get height from world coordinates
	std::optional<float> get_height(int x, int z) const;

	// Set height in heightmap and add chunk to (changed) list
	// U need to call dispatch_changes() after u set all the height
	// values, for the changes to take effect
	void set_height(int x, int z, float value);

	std::optional<TileType> get_type(int x, int z) const;
	void set_type(int x, int z, TileType new_type);

	// Recreate all ChunkMeshes where the chunk has changed
	void dispatch_changes();
	
	std::shared_ptr<ovk::Device> device;
	std::shared_ptr<MasterRenderer> renderer;

	std::unordered_map<glm::ivec2, Chunk> chunks;
	int world_extent;
	std::set<Chunk*> changed;
	
};
