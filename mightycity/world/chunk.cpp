#include "chunk.h"

int heightmap_index(int x, int z) { return (z * (chunk_size + 1)) + x; }
int type_index(int x, int z) { return z * chunk_size + x; }

Chunk::Chunk(glm::vec2 p, noise::module::Module* noise_module, ovk::Device& device)
  : pos(p) {

	ovk_assert((uint32_t) p.x % chunk_size == 0);
	ovk_assert((uint32_t) p.y % chunk_size == 0);
	
	// Populate heightmap
	for (int z = 0; z <= chunk_size; z++) {
		for (int x = 0; x <= chunk_size; x++) {
			heightmap[heightmap_index(x, z)] = noise_module->GetValue(
				((float)x / (float)chunk_size) + pos.x / chunk_size,
				0.3425f,
				((float) z / (float) chunk_size) + pos.y / chunk_size
			);
		}
	}
	// Types
	types.fill(TileType::base);
}

void Chunk::build_mesh(ovk::Device& device) {
	// calculate_terrain(this, device);
}

float Chunk::get_height(int x, int z) const {
	return heightmap[heightmap_index(x, z)];
}

TileType Chunk::get_type(int x, int z) const {
	return types[type_index(x, z)];
}

void Chunk::set_height(int x, int z, float value) {
	heightmap[heightmap_index(x, z)] = value;
}

void Chunk::set_type(int x, int z, TileType new_type) {
	types[type_index(x, z)] = new_type;
}

