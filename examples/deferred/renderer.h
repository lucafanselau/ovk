#include "app/camera.h"
#include "gui/gui_renderer.h"
#include <base/device.h>

#include <util/model_loader.h>

constexpr auto MAX_FRAMES_IN_FLIGHT = 2;

struct RenderVertex {
  glm::vec3 pos;
  glm::vec3 normal;
};

struct Renderable {
	ovk::Buffer* vertex_buffer;
	uint64_t draw_count;
	ovk::Buffer* material_buffer;
	uint64_t material_offset;
};

struct Renderer {

  ovk::Device *device;
  ovk::Surface *surface;
  ovk::util::Model *render_model;

  std::unique_ptr<ovk::FirstPersonCamera> camera;

  std::unique_ptr<ovk::SwapChain> swapchain;
  std::unique_ptr<ovk::RenderPass> render_pass;
  std::unique_ptr<ovk::GraphicsPipeline> pipeline;
  std::vector<ovk::Framebuffer> framebuffers;

  std::vector<std::unique_ptr<ovk::RenderCommand>> commands;
  uint32_t swapchain_index = 0;

  struct {
    std::unique_ptr<ovk::DescriptorTemplate> temp;
    std::unique_ptr<ovk::DescriptorPool> pool;
    std::vector<ovk::DescriptorSet> sets;
    std::vector<ovk::Buffer> uniform_buffers;
  } descriptor;

  struct {
    std::unique_ptr<ovk::Image> image;
    std::unique_ptr<ovk::ImageView> view;
  } depth;

  struct {
    std::vector<ovk::Semaphore> image_available, render_finished;
    std::vector<ovk::Fence> in_flight_fences;
    uint32_t current_frame = 0;
  } sync;

  std::unique_ptr<ovk::ImGuiRenderer> imgui;

  // PUBLIC API
  Renderer(ovk::Device *device, ovk::Surface *surface,
           ovk::util::Model *render_model);
  void recreate_swapchain();

  void update(float dt);
  void render();

  void finish();

  // PRIVATE API
  void create_const_objects();
  void create_dynamic_objects();

  void build_cmd_buffers(ovk::RenderCommand &cmd, const int i);
};
