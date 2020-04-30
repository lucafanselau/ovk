#pragma once

#include "../base/buffer.h"

namespace ovk::util {

// This sub-module of ovk will provide hassle free model loading into a
// specified vertex format Base Usage: const model =
// ovk::util::load_model<VertexLayout>("my_model.obj", [](ovk::util::VertexData
// data) -> VertexLayout {
//   return VertexLayout { pos: data.pos, normal: data.normal };
// });

struct OVK_API Model {

	Model(std::unique_ptr<ovk::Buffer>&& vertex, std::unique_ptr<ovk::Buffer>&& index, uint64_t draw_count);
	
  std::unique_ptr<ovk::Buffer> vertex, index;
  uint64_t draw_count = 0;
};

struct OVK_API VertexData {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 texcoord;
};

struct OVK_API OutputBuffer {
  uint8_t *start;
  size_t size, max_size;

	// Methods to use this buffer
	void push_data(void* data, size_t data_size);
	
};

struct OVK_API ParseOptions {
  bool use_index_buffer = false;
  bool disable_callback = true;
  void (*callback)(const VertexData &, OutputBuffer &) = nullptr;
};

// template <typename T, typename VertexType>
// concept VertexCallback = requires (T a, VertexType& b) {
// 	a(b);
// };

Model OVK_API load_model(const std::string &filepath, ParseOptions options,
                         ovk::Device &device);

} // namespace ovk::util
