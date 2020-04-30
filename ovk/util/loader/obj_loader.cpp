#include "obj_loader.h"
#include "base/mem.h"
#include "def.h"
#include "handle.h"
#include "pch.h"

#include "base/device.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

namespace ovk::util {

Model impl_load_model_obj(const std::string &filepath, ParseOptions options,
                          ovk::Device &device) {

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;

  std::string warn;
  std::string error;

  // tinyobj_opt::LoadOption option;
  // option.req_num_threads = -1; // All of them
  // option.verbose = verbose;
  // bool ret = parseObj(&attrib, &shapes, &materials, data, data_len, option);

  // if (!ret) spdlog::error("[TinyObjLoader] Failed to parse obj");

  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error,
                              filepath.c_str());

  if (!warn.empty()) {
    spdlog::warn("[TinyObjLoader] Warning: ");
    spdlog::warn(warn);
  }

  if (!error.empty()) {
    spdlog::error("[TinyObjLoader] Erroring: ");
    spdlog::error(error);
  }

  // Ok here we are going to be a bit memory wasting but it will later give user
  // huge perfomance benefits
  // we need to figure out the maximum size in bytes the output buffer can get
  // if the user decides to use all data (position, normal, texcoord) which is
  // honestly probable
  ovk_assert(sizeof(VertexData) == 8 * sizeof(float));
  const auto vertex_size = sizeof(VertexData);

  // -> We need to figure out the vertex_count before hand
  uint64_t vertex_count = 0;
  for (auto &shape : shapes)
    // Since we now that num_face_vertices will always be 3 we can just
    vertex_count += shape.mesh.num_face_vertices.size() * 3;
  const auto buffer_size = vertex_count * vertex_size;

  // Now we need to create the (memory coherent) buffer
  uint8_t *head = new uint8_t[buffer_size];

  // Then we are going to produce the output buffer to be passed to the callback
  OutputBuffer output_buffer{.start = head, .size = 0, .max_size = buffer_size};

  // Sanity Checking
  if (!options.disable_callback) {
    ovk_assert(options.callback != nullptr,
               "You did not disable callback for parsing, but no "
               "callback was provided");
  }

  // spdlog::info("Length's: vertices - {}, normals - {}, texcoords - {}",
  //              attrib.vertices.size(), attrib.normals.size(),
  //              attrib.texcoords.size());

  const auto use_normals = attrib.normals.size() != 0;
  const auto use_texcoords = attrib.texcoords.size() != 0;

  // Loop over shapes
  for (size_t s = 0; s < shapes.size(); s++) {
    // Loop over faces(polygon)
    size_t index_offset = 0;

    for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
      int fv = shapes[s].mesh.num_face_vertices[f];

      // -> Because triangulate was true
      ovk_assert(fv == 3);

      // Loop over vertices in the face.
      for (size_t v = 0; v < fv; v++) {
        // access to vertex
        tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
        // Vertices are required to be present
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
        // Normals and texcoords are optional
        glm::vec3 normal(0.0f);
        if (use_normals) {
          tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
          tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
          tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
          normal = glm::vec3(nx, ny, nz);
        }
        glm::vec2 texcoord(0.0f);
        if (use_texcoords) {
          tinyobj::real_t tx = attrib.texcoords[2 * idx.texcoord_index + 0];
          tinyobj::real_t ty = attrib.texcoords[2 * idx.texcoord_index + 1];
          texcoord = glm::vec2(tx, ty);
        }
        // Construct Vertex
        VertexData data{.pos = glm::vec3(vx, vy, vz),
                        .normal = normal,
                        .texcoord = texcoord};

        if (options.disable_callback) {
          // -> Directly push back the data
          memcpy(output_buffer.start + output_buffer.size, &data,
                 sizeof(VertexData));
          output_buffer.size += sizeof(VertexData);
        } else {
          // -> Call the Callback
          options.callback(data, output_buffer);
        }
      }
      index_offset += fv;

      // per-face material
      // shapes[s].mesh.material_ids[f];
    }
  }

  if (output_buffer.size > output_buffer.max_size)
    panic("Buffer overflow! Probably destroyed something");

  // -> But that into a buffer
  auto vertex_buffer = ovk::make_unique(device.create_buffer(
      vk::BufferUsageFlagBits::eVertexBuffer, output_buffer.size,
      output_buffer.start, {ovk::QueueType::graphics},
      ovk::mem::MemoryType::device_local));

	// delete output buffer
	delete[] output_buffer.start;
	
  spdlog::info("finished loading: {} vertices", vertex_count);
  return Model(std::move(vertex_buffer), nullptr, vertex_count);
}

} // namespace ovk::util
