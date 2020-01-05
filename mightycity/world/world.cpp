#include "world.h"

#include "chunk.h"

Terrain::Terrain(std::shared_ptr<ovk::Device>& d, std::shared_ptr<MasterRenderer>& r)
  : device(d), renderer(r), world_extent(0) {

}

void Terrain::render() {
	for (auto& [idx, chunk] : chunks) {
		renderer->draw_terrain(&chunk);
	}
}

void Terrain::calculate_terrain(noise::module::Module* noise_module, int32_t we) {
	world_extent = we;
	renderer->wait_last_frame();
	chunks.clear();
	for (int z = 0; z < world_extent; z++) {
		for (int x = 0; x < world_extent; x++) {
			auto [it, insertion] = chunks.try_emplace(
				glm::ivec2(x, z),
				glm::vec2(x * chunk_size, z * chunk_size), noise_module, *device
			);
			ovk_assert(insertion);				
		}
	}

	// After all have been created we need to build them
	for (auto &[pos, chunk] : chunks) {
		::calculate_terrain(this, &chunk, *device);
	}
}

Chunk* Terrain::get_chunk(int x, int z) {
	auto search = chunks.find(glm::ivec2(x, z));
	if (search == chunks.end()) return nullptr;
	return &search->second;
}


const Chunk* Terrain::get_chunk(int x, int z) const {
	auto search = chunks.find(glm::ivec2(x, z));
	if (search == chunks.end()) return nullptr;
	return &search->second;
}

std::pair<glm::ivec2, glm::ivec2> Terrain::parse_location(int x, int z) const {

	int chunk_x = x / chunk_size;
	int chunk_z = z / chunk_size;

	int local_x = x - (chunk_x * chunk_size);
	int local_z = z - (chunk_z * chunk_size);

	if (x == world_extent * chunk_size || z == world_extent * chunk_size) {
		// If we are on the edge of the world
		auto world_size = world_extent * chunk_size;
		if (x == world_size && z == world_size) {
			chunk_x -= 1;
			chunk_z -= 1;
			local_x = chunk_size;
			local_z = chunk_size;
		} else if (x == world_size) {
			chunk_x -= 1;
			local_x = chunk_size;
		} else if (z == world_size) {
			chunk_z -= 1;
			local_z = chunk_size;
		}
	}
	
	return std::make_pair(
		glm::ivec2(chunk_x, chunk_z), glm::ivec2(local_x, local_z)
	);
}

std::optional<float> Terrain::get_height(int x, int z) const {
	// Get the chunk this point lays in

	auto [chunk_loc, local] = parse_location(x, z);
	
	auto *chunk = get_chunk(chunk_loc.x, chunk_loc.y);
	if (!chunk) return std::nullopt;

	return chunk->get_height(local.x, local.y);
}

void Terrain::set_height(int x, int z, float value) {

	auto [chunk_loc, local] = parse_location(x, z);

	// Just get the chunk and set the height there
	auto *chunk = get_chunk(chunk_loc.x, chunk_loc.y);
	if (!chunk) return;


	// if we arent then just proceed
	chunk->set_height(local.x, local.y, value);
	changed.insert(chunk);
	

	// in case we are on the lower edge of the chunk x | z == 0
	// we need to address the neighboring chunk to perform updates on its data
	if (local.x == 0) {
		// Get chunk at chunk_loc.x - 1
		chunk = get_chunk(chunk_loc.x - 1, chunk_loc.y);
		if (chunk) {
			chunk->set_height(chunk_size, local.y, value);
			changed.insert(chunk);
		}
	}
	if (local.y == 0) {
		// Get chunk at chunk_loc.z - 1
		chunk = get_chunk(chunk_loc.x, chunk_loc.y - 1);
		if (chunk) {
			chunk->set_height(local.x, chunk_size, value);
			changed.insert(chunk);
		}
	}

	// if we landed at (0, 0) of the current chunk we need to also update the chunk at (-1, -1)
	if (local.x == 0 && local.y == 0) {
		chunk = get_chunk(chunk_loc.x - 1, chunk_loc.y - 1);
		if (chunk) {
			chunk->set_height(chunk_size, chunk_size, value);
			changed.insert(chunk);
		}
	}
}

std::optional<TileType> Terrain::get_type(int x, int z) const {
	// Get the chunk this point lays in
	auto [chunk_loc, local] = parse_location(x, z);
	
	auto *chunk = get_chunk(chunk_loc.x, chunk_loc.y);
	if (!chunk) return std::nullopt;

	return chunk->get_type(local.x, local.y);
}


void Terrain::set_type(int x, int z, TileType new_type) {
	auto [chunk_loc, local] = parse_location(x, z);

	// Just get the chunk and set the height there
	auto *chunk = get_chunk(chunk_loc.x, chunk_loc.y);
	if (!chunk) return;

	chunk->set_type(local.x, local.y, new_type);
	changed.insert(chunk);

	// as always the edge cases are there were we need
	// to also update the neighboring chunks
	if (local.x == 0) {
		chunk = get_chunk(chunk_loc.x - 1, chunk_loc.y);
		if (chunk) changed.insert(chunk);
	}
	if (local.y == 0) {
		chunk = get_chunk(chunk_loc.x, chunk_loc.y - 1);
		if (chunk) changed.insert(chunk);
	}
	if (local.x == 7) {
		chunk = get_chunk(chunk_loc.x + 1, chunk_loc.y);
		if (chunk) changed.insert(chunk);
	}
	if (local.y == 7) {
		chunk = get_chunk(chunk_loc.x, chunk_loc.y + 1);
		if (chunk) changed.insert(chunk);
	}

	
}

void Terrain::dispatch_changes() {
	if (changed.empty()) return;

	for (auto* chunk : changed) {
		// TODO: this right here would be a perfekt scenario for
		// multithreading
		// chunk->build_mesh(*device);
		::calculate_terrain(this, chunk, *device);
		
	}

	changed.clear();
}
 
