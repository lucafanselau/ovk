#pragma once
#include <glm/vec3.hpp>
#include "base/buffer.h"
#include <noise/module/modulebase.h>

struct Chunk;
struct Terrain;

struct TerrainVertex {

  glm::vec3 pos;
  glm::vec3 normal;
	glm::vec3 color;
};

struct TerrainMesh {
	ovk::Buffer vertex;
	uint32_t vertices_count;
};

struct MeshVertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 tex_coord;
};

struct Material {
	alignas(16) glm::vec3 ambient;
	alignas(16) glm::vec3 diffuse;
	alignas(16) glm::vec3 specular;
	alignas(4) float shininess;
};

struct Mesh {
	Mesh(ovk::Buffer&& buffer, uint32_t count, uint32_t offset);
	ovk::Buffer vertex;
	uint32_t vertices_count;
	uint32_t dynamic_offset;
};

struct Model {
	Model(std::string name, std::vector<std::unique_ptr<Mesh>>&& meshes);
	
	std::string name;
	std::vector<std::unique_ptr<Mesh>> meshes;
};

struct ModelManager {
	ModelManager(std::shared_ptr<ovk::Device>& device);

	void load(std::string name, std::string filename);

	Model* get(std::string name);

	std::shared_ptr<ovk::Device> device;
	std::unordered_map<std::string, Model> models;

	std::unique_ptr<ovk::Buffer> materials;
	uint32_t materials_head = 0, dynamic_alignment = 0;

	std::vector<std::unique_ptr<Mesh>> do_load(const std::string filename);
};

// during this function the mesh field of the chunk will be set
void calculate_terrain(const Terrain* terrain, Chunk* chunk, ovk::Device& device);

std::unique_ptr<TerrainMesh> calculate_terrain(noise::module::Module* _noise, ovk::Device& device);
std::vector<std::unique_ptr<Mesh>> load_meshes_from_obj(const std::string& filename, ovk::Device& device);

