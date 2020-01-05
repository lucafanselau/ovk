#include "mesh.h"

#include <base/device.h>

#include <cmath>
#include <tiny_obj_loader.h>

#include <noise/noise.h>
#include <sstream>

#include <boost/pfr.hpp>

#include "..\world\world.h"

Mesh::Mesh(ovk::Buffer &&buffer, uint32_t count, uint32_t offset)
    : vertex(std::move(buffer)), vertices_count(count), dynamic_offset(offset) {
}

Model::Model(std::string n, std::vector<std::unique_ptr<Mesh>>&& m)
	: name(n), meshes(std::move(m)) {}

ModelManager::ModelManager(std::shared_ptr<ovk::Device>& d) : device(d) {

	// create materials buffer
	// Here are some variables to tweak
	// Maximum Number of Materials possible
	const uint32_t material_count = 128;
	{
		auto buffer_usage = vk::BufferUsageFlagBits::eUniformBuffer;

		size_t min_buffer_alignment = device->physical_device.getProperties().limits.minUniformBufferOffsetAlignment;

		dynamic_alignment = sizeof(Material);
		if (min_buffer_alignment > 0) {
			dynamic_alignment = (dynamic_alignment + min_buffer_alignment - 1) & ~(min_buffer_alignment - 1);
		}

		const uint32_t size = material_count * dynamic_alignment;

		materials = ovk::make_unique(device->create_buffer(
			buffer_usage,
			size,
			nullptr,
			{ ovk::QueueType::graphics },
			ovk::mem::MemoryType::cpu_coherent_and_cached
		));
		
	}
	
}

void ModelManager::load(std::string name, std::string filename) {
	if (auto it = models.find(name); it == models.end()) {
		models.insert_or_assign(name, Model(name, do_load(filename)));
	} else {
		spdlog::error("[ModelManager] (load) name {} already exists!", name);
	}
}

Model* ModelManager::get(std::string name) {
	if (auto it = models.find(name); it != models.end()) {
		return &it->second;
	}
	spdlog::error("[ModelManager] (get) model with name {} does not exists!", name);
	return nullptr;
}

glm::vec3 to_vec3(float* a) {
	return glm::vec3(*(a), *(a + 1), *(a + 2));
}


std::vector<std::unique_ptr<Mesh>> ModelManager::do_load(const std::string filename) {

	// Load TinyOBJ
	tinyobj::attrib_t attribute;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> obj_materials;
	std::string error, warn;
	const auto parse_result = tinyobj::LoadObj(
		&attribute,
		&shapes,
		&obj_materials,
		&warn,
		&error,
		filename.c_str(),
		"res/materials/"
	);

	if (!warn.empty())
		spdlog::warn("(load_mesh_from_obj) parse warning: {}", warn);
	if (!error.empty())
		spdlog::error("(load_mesh_from_obj) parse error: {}", error);
	if (!parse_result)
		spdlog::critical("(load_mesh_from_obj) failed to parse {}", filename);

	std::vector<std::unique_ptr<Mesh>> meshes;

	auto mem_pointer = (uint8_t*) materials->memory->map(*device);
	auto base_head = materials_head;
	for (auto& obj_m : obj_materials) {

		Material material {
											 to_vec3(obj_m.ambient),
											 to_vec3(obj_m.diffuse),
											 to_vec3(obj_m.specular),
											 obj_m.shininess
		};
			
		// Copy it to the plan data buffer

		memcpy(&mem_pointer[(int) materials_head], &material, sizeof(material));
			
		materials_head += dynamic_alignment;
	}
	materials->memory->unmap(*device);

	auto to_vertex = [&](tinyobj::index_t& idx) {
										 const auto vidx = idx.vertex_index;
										 const auto nidx = idx.normal_index;
										 const auto tidx = idx.texcoord_index;

										 // TODO: Ok we now that if any idx is -1 this value is not present but we dont handle that here
										 // DO THAT:
		
		
										 return MeshVertex {
											 glm::vec3(attribute.vertices[3 * vidx + 0], attribute.vertices[3 * vidx + 1], attribute.vertices[3 * vidx + 2]),
												 glm::vec3(attribute.normals[3 * nidx + 0], attribute.normals[3 * nidx + 1], attribute.normals[3 * nidx + 2]) * glm::vec3(-1.0f, 1.0f, -1.0f),
												 // TODO: should not be vec3
												 glm::vec3(0.0f) // glm::vec3(attribute.texcoords[ 2 * tidx + 0], attribute.texcoords[ 2 * tidx + 1], 0.0f)
												 };
									 };

	ovk_assert(!shapes.empty());
	for (auto& shape : shapes) {
		auto&  mesh = shape.mesh;
		
		std::vector<MeshVertex> vertices;

		auto push_mesh = [&](int idx) {
			meshes.push_back(std::make_unique<Mesh>(
		    std::move(device->create_vertex_buffer(vertices, ovk::mem::MemoryType::device_local)),
				vertices.size(),
				base_head + idx * dynamic_alignment
      ));

			vertices.clear();													
	  };
	 
	
		int last_material_idx = mesh.material_ids[0];
		for (size_t f = 0; f < mesh.indices.size() / 3; f++) {

			// We expect a face to have a consistent material
			auto& material_idx = mesh.material_ids[f];
			if (last_material_idx != material_idx) {
				push_mesh(last_material_idx);
				
				last_material_idx = material_idx;
			}
			
			auto idx0 = mesh.indices[f * 3 + 0];
			auto idx1 = mesh.indices[f * 3 + 1];
			auto idx2 = mesh.indices[f * 3 + 2];

			vertices.push_back(to_vertex(idx0));
			vertices.push_back(to_vertex(idx1));
			vertices.push_back(to_vertex(idx2));
		}


		push_mesh(last_material_idx);
	}
	
	return meshes;
}

void calculate_terrain(const Terrain* terrain, Chunk* chunk, ovk::Device& device) {

	std::vector<TerrainVertex> vertices;
	// TODO: this must not be a TerrainVertex but we dont care atm
	// TODO: Possible Optimization
	std::vector<TerrainVertex> picker_vertices;

	// TODO: Make a function out of this
	auto get_color = [](float height, float angle /*expected to be in rad*/) {

		enum ColorType {
			grass,
			dirt,
			stone,
			water
		};

		ColorType type;
		if (height < 0.0f) {
			type = water;
		} else {			
			if (height > 15.0f) {
				type = stone;
			} else {
				if (angle < glm::radians(45.0)) {
					type = grass;
				} else if (angle < glm::radians(80.0)) {
					type = dirt;
				} else {
					type = stone;
				}
			}
		}
		
		switch (type) {
		case grass: return glm::vec3(143.0f / 255.0f, 203.0f / 255.0f, 98.0f / 255.0f);
		case dirt: return glm::vec3(155.f / 255.f, 116.f / 255.f, 82.f / 255.f);
		case stone: return glm::vec3(160.0f / 255.0f, 171.0f / 255.0f, 177.0f / 255.0f);
		case water: return glm::vec3(67.0f / 255.0f, 94.0f / 255.0f, 219.0f / 255.0f);
		default: return glm::vec3(1.0f);
		}
	};
	
	for (int z = 0; z < chunk_size; z++) {
		for (int x = 0; x < chunk_size; x++) {
	
			glm::vec3 current(x, 0, z);
			glm::vec3 a1(0, chunk->get_height(x, z), 0);
			glm::vec3 a2(1, chunk->get_height(x + 1, z), 0);
			glm::vec3 a3(0, chunk->get_height(x, z + 1), 1);
			glm::vec3 a4(1, chunk->get_height(x + 1, z + 1), 1);

			auto dx1 = a2 - a1;
			auto dx2 = a4 - a3;
			auto dz1 = a3 - a1;
			auto dz2 = a4 - a2;

			auto dd1 = a4 - a1;
			auto dd2 = a3 - a2;
			
			auto raw_normal1 = glm::normalize(glm::cross(a4 - a1, a2 - a1));
			auto raw_normal2 = glm::normalize(glm::cross(a3 - a1, a4 - a1));

			auto interpolation = 0.1f;
			auto normal1 = raw_normal1 + (raw_normal2 - raw_normal1) * interpolation;
			auto normal2 = raw_normal2 + (raw_normal1 - raw_normal2) * interpolation;

			auto middle_normal = raw_normal1 + (raw_normal2 - raw_normal1) * 0.5f;

			auto avg1 = (a1.y + a2.y + a4.y) / 3.0f;
			auto avg2 = (a1.y + a4.y + a3.y) / 3.0f;

			const glm::vec3 up(0.0f, 1.0f, 0.0f);
			auto angle1 = glm::acos(glm::dot(raw_normal1, up));
			auto angle2 = glm::acos(glm::dot(raw_normal2, up));

			auto color1 = get_color(avg1, angle1);
			auto color2 = get_color(avg2, angle2);

			std::vector<TerrainVertex> current_vertices;
			std::vector<TerrainVertex> current_picker_vertices;
			
			if (chunk->get_type(x, z) == TileType::street) {
				//color1 = glm::vec3(1.0f, 1.0f, 1.0f);
				//color2 = glm::vec3(1.0f, 1.0f, 1.0f);
				const auto inset = 0.2f;
				const auto hypinset = sqrt(2 * inset * inset);
				// Really ugly name for one minus inset
				const auto ominset = 1.0f - inset;
				// Inner vertices
				auto a5 = a1 + inset * dd1;
				auto a6 = a1 + ominset * dx1 + inset * dz2;
				auto a7 = a1 + inset * dx2 + ominset * dz1;
				auto a8 = a1 + ominset * dd1;

				// the other ones
				// NOTE: see OneNote for information
				auto a9 = a1 + inset * dx1;
				auto a10 = a1 + ominset * dx1;
				auto a11 = a1 + dx1 + inset * dz2;
				auto a12 = a1 + dx1 + ominset * dz2;
				auto a13 = a1 + ominset * dx2 + dz1;
				auto a14 = a1 + inset * dx2 + dz1;
				auto a15 = a1 + ominset * dz1;
				auto a16 = a1 + inset * dz1;

				glm::vec3 street_color(0.5020f, 0.4941f, 0.4706f);

				int world_pos_x = chunk->pos.x + x;
				int world_pos_z = chunk->pos.y + z;
				
				glm::vec3 street_top = color1;
				if (z == 0) { if (auto type = terrain->get_type(world_pos_x, world_pos_z - 1); type.has_value() && type.value() == TileType::street) street_top = street_color; }
				else { if (chunk->get_type(x, z - 1) == TileType::street) street_top = street_color; }

				glm::vec3 street_bottom = color2;
				if (z == 7) { if (terrain->get_type(world_pos_x, world_pos_z + 1) == TileType::street) street_bottom = street_color; }
				else { if (chunk->get_type(x, z + 1) == TileType::street) street_bottom = street_color; }
				
				
				glm::vec3 street_right = color1;
				if (x == 7) { if(terrain->get_type(world_pos_x + 1, world_pos_z) == TileType::street) street_right = street_color; }
				else { if (chunk->get_type(x + 1, z) == TileType::street) street_right = street_color; }
				
				glm::vec3 street_left = color2;
				if (x == 0) { if(terrain->get_type(world_pos_x - 1, world_pos_z) == TileType::street) street_left = street_color; }
				else { if (chunk->get_type(x - 1, z) == TileType::street) street_left = street_color; }
				
				
				// TOP LEFT Triangles
				current_vertices.push_back(TerrainVertex { current + a1, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a9, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a5, raw_normal1, color1 });

				current_vertices.push_back(TerrainVertex { current + a1, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a5, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a16, raw_normal2, color2 });

				// TOP MIDDLE
				current_vertices.push_back(TerrainVertex { current + a9, raw_normal1, street_top });
				current_vertices.push_back(TerrainVertex { current + a10, raw_normal1, street_top });
				current_vertices.push_back(TerrainVertex { current + a6, raw_normal1, street_top });

				current_vertices.push_back(TerrainVertex { current + a9, raw_normal1, street_top });
				current_vertices.push_back(TerrainVertex { current + a6, raw_normal1, street_top });
				current_vertices.push_back(TerrainVertex { current + a5, raw_normal1, street_top });

				// TOP RIGHT
				current_vertices.push_back(TerrainVertex { current + a10, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a2, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a11, raw_normal1, color1 });

				current_vertices.push_back(TerrainVertex { current + a10, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a11, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a6, raw_normal1, color1 });

				// RIGHT MIDDLE
				current_vertices.push_back(TerrainVertex { current + a6, raw_normal1, street_right });
				current_vertices.push_back(TerrainVertex { current + a11, raw_normal1, street_right });
				current_vertices.push_back(TerrainVertex { current + a12, raw_normal1, street_right });

				current_vertices.push_back(TerrainVertex { current + a6, raw_normal1, street_right });
				current_vertices.push_back(TerrainVertex { current + a12, raw_normal1, street_right });
				current_vertices.push_back(TerrainVertex { current + a8, raw_normal1, street_right });

				// BOTTOM RIGHT
				current_vertices.push_back(TerrainVertex { current + a8, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a12, raw_normal1, color1 });
				current_vertices.push_back(TerrainVertex { current + a4, raw_normal1, color1 });

				current_vertices.push_back(TerrainVertex { current + a8, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a4, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a13, raw_normal2, color2 });

				// BOTTOM MIDDLE
				current_vertices.push_back(TerrainVertex { current + a7, raw_normal2, street_bottom });
				current_vertices.push_back(TerrainVertex { current + a8, raw_normal2, street_bottom });
				current_vertices.push_back(TerrainVertex { current + a13, raw_normal2, street_bottom });

				current_vertices.push_back(TerrainVertex { current + a7, raw_normal2, street_bottom });
				current_vertices.push_back(TerrainVertex { current + a13, raw_normal2, street_bottom });
				current_vertices.push_back(TerrainVertex { current + a14, raw_normal2, street_bottom });

				// BOTTOM LEFT
				current_vertices.push_back(TerrainVertex { current + a15, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a7, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a14, raw_normal2, color2 });

				current_vertices.push_back(TerrainVertex { current + a15, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a14, raw_normal2, color2 });
				current_vertices.push_back(TerrainVertex { current + a3, raw_normal2, color2 });

				// LEFT MIDDLE
				current_vertices.push_back(TerrainVertex { current + a16, raw_normal2, street_left });
				current_vertices.push_back(TerrainVertex { current + a5, raw_normal2, street_left });
				current_vertices.push_back(TerrainVertex { current + a7, raw_normal2, street_left });

				current_vertices.push_back(TerrainVertex { current + a16, raw_normal2, street_left });
				current_vertices.push_back(TerrainVertex { current + a7, raw_normal2, street_left });
				current_vertices.push_back(TerrainVertex { current + a15, raw_normal2, street_left });

				// MIDDLE MIDDLE
				current_vertices.push_back(TerrainVertex { current + a5, raw_normal1, street_color });
				current_vertices.push_back(TerrainVertex { current + a6, raw_normal1, street_color });
				current_vertices.push_back(TerrainVertex { current + a8, raw_normal1, street_color });

				current_vertices.push_back(TerrainVertex { current + a5, raw_normal2, street_color });
				current_vertices.push_back(TerrainVertex { current + a8, raw_normal2, street_color });
				current_vertices.push_back(TerrainVertex { current + a7, raw_normal2, street_color });				
				
			} else {
				// Normal Tile
				current_vertices = {
				  TerrainVertex{ current + a1, raw_normal1, color1},
					TerrainVertex{ current + a2, raw_normal1, color1},
					TerrainVertex{ current + a4, raw_normal1, color1},
					TerrainVertex{ current + a1, raw_normal2, color2},
					TerrainVertex{ current + a4, raw_normal2, color2},
					TerrainVertex{ current + a3, raw_normal2, color2},		
				};

			}

			// Since the picker figures out the pos by dividing gl_VertexID / 3
			// currently the most simple solution is to have a mesh that does not have the data
			current_picker_vertices = {
				TerrainVertex{ current + a1, raw_normal1, color1},
				TerrainVertex{ current + a2, raw_normal1, color1},
				TerrainVertex{ current + a4, raw_normal1, color1},
				TerrainVertex{ current + a1, raw_normal2, color2},
				TerrainVertex{ current + a4, raw_normal2, color2},
				TerrainVertex{ current + a3, raw_normal2, color2},		
			};
					
			vertices.insert(vertices.end(), current_vertices.begin(), current_vertices.end());
			picker_vertices.insert(picker_vertices.end(),
														 current_picker_vertices.begin(),
														 current_picker_vertices.end());
			
		}
	}

	chunk->mesh = std::make_unique<TerrainMesh>(
		std::move(TerrainMesh{
			std::move(device.create_vertex_buffer(vertices, ovk::mem::MemoryType::device_local)),
			static_cast<uint32_t>(vertices.size())
		})
	);

	chunk->picker_mesh = std::make_unique<TerrainMesh>(
  	std::move(TerrainMesh{
		  std::move(device.create_vertex_buffer(picker_vertices, ovk::mem::MemoryType::device_local)),
			static_cast<uint32_t>(picker_vertices.size())
		})
	);
	
}

// TODO: Remove
#if 0

std::vector<std::unique_ptr<Mesh>> load_meshes_from_obj(const std::string& filename, ovk::Device& device) {





	// Ok we will need a dynamic uniform buffer for the materials
	// this is currently not part of ovk so we are going to do it here
	// TODO: This should be done in ovk
	std::shared_ptr<ovk::Buffer> materials_buffer;
	size_t dynamic_alignment;
	{
		auto buffer_usage = vk::BufferUsageFlagBits::eUniformBuffer;
		// Figure out the size
		// Calculate required alignment based on minimum device offset alignment
		size_t min_buffer_alignment = device.physical_device.getProperties().limits.minUniformBufferOffsetAlignment;
		dynamic_alignment = sizeof(Material);
		if (min_buffer_alignment > 0) {
			dynamic_alignment = (dynamic_alignment + min_buffer_alignment - 1) & ~(min_buffer_alignment - 1);
		}
		const size_t whole_size = dynamic_alignment * materials.size();
		std::vector<uint8_t> data(whole_size, 0);
		size_t current_offset = 0;
		for (auto& obj_m : materials) {

			Material material {
			 to_vec3(obj_m.ambient),
			 to_vec3(obj_m.diffuse),
			 to_vec3(obj_m.specular),
			 obj_m.shininess
			};
			
			// Copy it to the plan data buffer
			memcpy(&data[current_offset], &material, sizeof(material));
			
			current_offset += dynamic_alignment;
		}

		materials_buffer = ovk::make_shared(device.create_buffer(
      buffer_usage,
			whole_size,
			data.data(),
			{ ovk::QueueType::graphics },
			ovk::mem::MemoryType::device_local
    ));
	}
 
	auto to_vertex = [&](tinyobj::index_t& idx) {
		const auto vidx = idx.vertex_index;
		const auto nidx = idx.normal_index;
		const auto tidx = idx.texcoord_index;

		// TODO: Ok we now that if any idx is -1 this value is not present but we dont handle that here
		// DO THAT:
		
		
		return MeshVertex {
			glm::vec3(attribute.vertices[3 * vidx + 0], attribute.vertices[3 * vidx + 1], attribute.vertices[3 * vidx + 2]),
				glm::vec3(attribute.normals[3 * nidx + 0], attribute.normals[3 * nidx + 1], attribute.normals[3 * nidx + 2]) * glm::vec3(-1.0f, 1.0f, -1.0f),
			// TODO: should not be vec3
				glm::vec3(0.0f) // glm::vec3(attribute.texcoords[ 2 * tidx + 0], attribute.texcoords[ 2 * tidx + 1], 0.0f)
		};
	};

	ovk_assert(!shapes.empty());
	for (auto& shape : shapes) {
		auto&  mesh = shape.mesh;
		
		std::vector<MeshVertex> vertices;

		auto push_mesh = [&](int idx) {
			meshes.push_back(std::make_unique<Mesh>(
		    std::move(device.create_vertex_buffer(vertices, ovk::mem::MemoryType::device_local)),
				vertices.size(),
				std::move(materials_buffer),
				idx * dynamic_alignment
      ));

			vertices.clear();													
	  };
	 
	
		int last_material_idx = mesh.material_ids[0];
		for (size_t f = 0; f < mesh.indices.size() / 3; f++) {

			// We expect a face to have a consistent material
			auto& material_idx = mesh.material_ids[f];
			if (last_material_idx != material_idx) {
				push_mesh(last_material_idx);
				
				last_material_idx = material_idx;
			}
			
			auto idx0 = mesh.indices[f * 3 + 0];
			auto idx1 = mesh.indices[f * 3 + 1];
			auto idx2 = mesh.indices[f * 3 + 2];

			vertices.push_back(to_vertex(idx0));
			vertices.push_back(to_vertex(idx1));
			vertices.push_back(to_vertex(idx2));
		}


		push_mesh(last_material_idx);
	}
	
	return meshes;
}

#endif
