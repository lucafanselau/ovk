#include "model_loader.h"
#include "pch.h"

#include "loader/obj_loader.h"

namespace ovk::util {

using BufferPtr = std::unique_ptr<ovk::Buffer>;

Model::Model(std::unique_ptr<ovk::Buffer> &&vertex,
             std::unique_ptr<ovk::Buffer> &&index, uint64_t draw_count)
    : vertex(std::forward<BufferPtr>(vertex)),
      index(std::forward<BufferPtr>(index)), draw_count(draw_count) {}

void OutputBuffer::push_data(void *data, size_t data_size) {
#ifdef DEBUG
  // Only do sanity checking in debug builds
  if (size + data_size > max_size) {
    panic("[OutputBuffer] Operation will trigger overflow");
    return;
  }
#endif
  memcpy(start + size, data, data_size);
  size += data_size;
}

Model load_model(const std::string &filepath, ParseOptions options,
                 ovk::Device &device) {
  const auto extension = filepath.substr(filepath.find_last_of(".") + 1);

  if (extension == "obj") {
    spdlog::trace("Parsing OBJ File");
    return impl_load_model_obj(filepath, options, device);
  }

  // NOTE: Maybe we should signal not success
  spdlog::warn("Parsing Model: {} unknown extension", extension);
  return Model(nullptr, nullptr, 0);
}

} // namespace ovk::util
