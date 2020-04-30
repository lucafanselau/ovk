#include "renderer.h"
#include "app/camera.h"
#include "gui/gui_renderer.h"
#include "vulkan/vulkan.hpp"
#include <base/surface.h>

Renderer::Renderer(ovk::Device *device, ovk::Surface *surface,
                   ovk::util::Model *render_model)
    : device(device), surface(surface), render_model(render_model) {

  create_const_objects();
  create_dynamic_objects();
};

void Renderer::recreate_swapchain() {
  device->wait_idle();

  int width = 0, height = 0;
  glfwGetFramebufferSize(surface->window.get(), &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(surface->window.get(), &width, &height);
    glfwWaitEvents();
  }

  depth = {};

  swapchain->handle.invalidate();
  swapchain = ovk::make_unique(device->create_swapchain(*surface));

  create_dynamic_objects();
}

void Renderer::update(float dt) {
  // Acquire new image
  device->wait_fences({sync.in_flight_fences[sync.current_frame]});

  auto [recreate, index] = device->acquire_image(
      *swapchain, sync.image_available[sync.current_frame]);
  if (recreate) {
    recreate_swapchain();
  }

  swapchain_index = index;

  // ImGui Update stage
  ImGui::NewFrame();
  imgui->update();

  // Camera Update
  camera->update(dt, true);

  const auto dir = camera->dir;
  // spdlog::info("camera dir: {}, {}, {}", dir.x, dir.y, dir.z);
}

void Renderer::render() {

  // Update Camera Buffer
  auto &camera_data = camera->get_data();
  device->update_buffer(descriptor.uniform_buffers[swapchain_index],
                        camera_data);

  // Create ImGui Draw Data
  ImGui::Render();

  device->reset_fences({sync.in_flight_fences[sync.current_frame]});

  if (commands[swapchain_index]) {
    static auto &pool = device->get_command_pool(ovk::QueueType::graphics);
    device->device->freeCommandBuffers(pool,
                                       {commands[swapchain_index]->cmd_handle});
  }

  commands[swapchain_index] =
      std::make_unique<ovk::RenderCommand>(device->create_render_commands(
          1, [&](ovk::RenderCommand &cmd, const int) {
            build_cmd_buffers(cmd, swapchain_index);
          })[0]);

  device->submit(
      {ovk::WaitInfo{sync.image_available[sync.current_frame],
                     vk::PipelineStageFlagBits::eColorAttachmentOutput}},
      {commands[swapchain_index]->cmd_handle},
      {sync.render_finished[sync.current_frame]},
      sync.in_flight_fences[sync.current_frame]);

  const auto recreate = device->present_image(
      *swapchain, swapchain_index, {sync.render_finished[sync.current_frame]});
  if (recreate)
    recreate_swapchain();

  sync.current_frame = (sync.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::finish() {
	device->wait_idle();
}

void Renderer::create_const_objects() {
  // Create the Swapchain
  swapchain = ovk::make_unique(device->create_swapchain(*surface));

  camera = std::make_unique<ovk::FirstPersonCamera>(glm::vec3(0.0f, 0.0f, 3.0f),
                                                    *swapchain);

  // The Render Pass
  // atm just forward rendering
  // if you want more information about the api see triangle example
  const auto color_attachment_description =
      swapchain->get_color_attachment_description();

  const vk::AttachmentDescription depth_attachment{
      {},
      device->default_depth_format().value(),
      vk::SampleCountFlagBits::e1,
      vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eDontCare,
      vk::AttachmentLoadOp::eDontCare,
      vk::AttachmentStoreOp::eDontCare,
      vk::ImageLayout::eUndefined,
      vk::ImageLayout::eDepthStencilAttachmentOptimal};

  const ovk::GraphicSubpass main_pass(
      {},                             // input attachments
      {color_attachment_description}, // color attachments
      {},                             // resolve attachments
      {},                             // preserve attachments
      depth_attachment                // depth attachment
  );

  render_pass = ovk::make_unique(device->create_render_pass(
      {color_attachment_description, depth_attachment}, {main_pass}, true));

  // Create ImGui Renderer
  ovk::ImGuiSetupProps imgui_props{
      .use_extern_font = true, .font_path = "res/fonts/FiraCode-Regular.ttf"};
  imgui = std::make_unique<ovk::ImGuiRenderer>(*render_pass, *swapchain,
                                               *surface, *device, imgui_props);
  {
    // ImGui Settings
    ImGui::SetCurrentContext(imgui->context);
    auto &io = ImGui::GetIO();
    io.FontGlobalScale = 1.4f;
  }

  // Descriptor Template
  descriptor.temp = ovk::make_unique(
      device->build_descriptor_template()
			.add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
          .build());

  // Sync Primitives
  sync.image_available = device->create_semaphores(MAX_FRAMES_IN_FLIGHT);
  sync.render_finished = device->create_semaphores(MAX_FRAMES_IN_FLIGHT);
  sync.in_flight_fences = device->create_fences(
      MAX_FRAMES_IN_FLIGHT, vk::FenceCreateFlagBits::eSignaled);

  spdlog::info("created const objects");
}

void Renderer::create_dynamic_objects() {

  const auto depth_format = device->default_depth_format();

  depth.image = ovk::make_unique(device->create_image(
      vk::ImageType::e2D, depth_format.value(),
      vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height,
                   1),
      vk::ImageUsageFlagBits::eDepthStencilAttachment,
      vk::ImageTiling::eOptimal, ovk::mem::MemoryType::device_local));

  depth.view = ovk::make_unique(
      device->view_from_image(*depth.image, vk::ImageAspectFlagBits::eDepth));

  pipeline = ovk::make_unique(
      device->build_pipeline()
          .set_render_pass(*render_pass, /*subpass index: */ 0)
          // This will add a Spir-V Shader from a File
          .add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex,
                                      "res/shaders/triangle.vert.spv")
          .add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment,
                                      "res/shaders/triangle.frag.spv")
          .set_vertex_layout<RenderVertex>()
          // Will add depth test to this pipeline (using our depth target)
          .set_depth_stencil()
          .add_descriptor_set_layouts({descriptor.temp->handle.get()})
          .add_viewport(glm::vec2(0, 0),
                        glm::vec2(swapchain->swap_extent.width,
                                  swapchain->swap_extent.height),
                        0.0f, 1.0f)
          .add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
          .build()

  );

  // Framebuffers
  framebuffers.clear();
  framebuffers.reserve(swapchain->image_count);
  for (auto i = 0; i < swapchain->image_count; i++) {
    auto &swap_image = swapchain->image_views[i];
    framebuffers.push_back(
        std::forward<ovk::Framebuffer>(device->create_framebuffer(
            *render_pass,
            vk::Extent3D(swapchain->swap_extent.width,
                         swapchain->swap_extent.height, 1),
            {swap_image, *depth.view})));
  }

  // Descriptor Pool and Camera Buffers
  descriptor.pool = ovk::make_unique(device->create_descriptor_pool(
      {descriptor.temp.get()}, {swapchain->image_count}));

  descriptor.sets = device->make_descriptor_sets(
      *descriptor.pool, swapchain->image_count, *descriptor.temp);

  // -> Create enough buffers
  descriptor.uniform_buffers.clear();
  descriptor.uniform_buffers.reserve(swapchain->image_count);

  // Demo Data
  auto &camera_data = camera->get_data();

  for (size_t i = 0; i < swapchain->image_count; ++i) {
    descriptor.uniform_buffers.push_back(device->create_uniform_buffer(
        camera_data, ovk::mem::MemoryType::cpu_coherent_and_cached,
        {ovk::QueueType::graphics}));
    descriptor.sets[i].write(descriptor.uniform_buffers[i], 0, 0);
  }

  // Command Buffers
  commands.clear();
  commands.resize(swapchain->image_count);

  // commands = device->create_render_commands(
  // swapchain->image_count,
  // [&](ovk::RenderCommand &cmd, const int i) { build_cmd_buffers(cmd, i); });
}

void Renderer::build_cmd_buffers(ovk::RenderCommand &cmd, const int i) {
  cmd.begin_render_pass(*render_pass, framebuffers[i], swapchain->swap_extent,
                        {glm::vec4(1.0), glm::vec2(1.0f, 0.0f)}, vk::SubpassContents::eInline);

  {
    cmd.begin_region("Game Rendering", glm::vec4(0.23f, 0.34f, 0.87f, 1.00f));

    cmd.bind_graphics_pipeline(*pipeline);
    cmd.bind_descriptor_sets(*pipeline, 0, {descriptor.sets[i]});

    cmd.bind_vertex_buffers(0, {ovk::RenderCommand::BufferDescription{
                                   std::ref(*render_model->vertex), 0}});
    cmd.draw(render_model->draw_count, 1, 0, 0);

    cmd.end_region();
  }

  {
    cmd.begin_region("ImGui Rendering", glm::vec4(0.87f, 0.21f, 0.11f, 1.00f));
    cmd.draw_imgui(*imgui, i, ImGui::GetDrawData());
    cmd.end_region();
  }

  cmd.end_render_pass();
}
