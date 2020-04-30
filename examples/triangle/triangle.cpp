#include <base/device.h>
#include <base/instance.h>

void run_example() {

  ovk::Instance instance(ovk::AppInfo{"Triangle", 0, 0, 1}, {});

  auto surface =
      instance.create_surface(1960, 1080, "Triangle", /*events: */ false);
  // Device is pretty simple, no extra features and just Swapchain support
  // extension
  auto device = instance.create_device({VK_KHR_SWAPCHAIN_EXTENSION_NAME},
                                       vk::PhysicalDeviceFeatures(), surface);

	spdlog::info("Device created!");
	
  // Now we can create the swapchain
  auto swapchain = device.create_swapchain(surface);

  // Now that we have the basic setup of a vulkan program
  // we can start preparing the triangle
  // This program will be written completly inside of the main function
  // What we need to render the triangle
  // 1. Render Pass
  // The RenderPass will be static throughout the lifetime of the program and
  // describes what attachments are used for rendering Since we only want to
  // render a depth-less triangle to the swapchain this is pretty simple
  const auto color_attachment_description =
      swapchain.get_color_attachment_description();
  const ovk::GraphicSubpass main_pass(
      {},                             // input attachments
      {color_attachment_description}, // color attachments
      {},                             // resolve attachments
      {},                             // preserve attachments
      {}                              // depth attachment
  );

  // The last boolean adds external dependencies
  // This meands that the first subpass (in the vector of subpasses) must wait
  // for the swapchain to finished reading from the attachments and therefore
  // the image is ready to be rendered to
  auto render_pass = device.create_render_pass({color_attachment_description},
                                               {main_pass}, true);

  // 2. Framebuffers
  // They are also dependant on the swapchain
  std::vector<ovk::Framebuffer> framebuffers;

  // 3. Pipeline (The Pipeline is dynamic so we might destroy and rebuild it see
  // create_dynamic_objects())
  std::unique_ptr<ovk::GraphicsPipeline> pipeline;

  // 4. Vertex Buffer
  struct VertexLayout {
    glm::vec2 position;
  };
  // Creating a vertex buffer becomes really simple with ovk
  // since there is a default allocator in mem.h/mem.cpp that handles all of the
  // nasty memory managment of vulkan but if u want every function takes as the
  // last parameter a mem::Allocator* for u to provide ur own memory managment
  // facility (atm there is a default allocator (without fragmentation removal
  // (wip)), a linear allocator and a dedicated allocator
  std::array<VertexLayout, 3> vertices = {VertexLayout{glm::vec2(-0.5f, -0.5f)},
                                          {glm::vec2(0.0f, 0.5f)},
                                          {glm::vec2(0.5f, -0.5f)}};

  auto vertex_buffer = device.create_vertex_buffer(
      vertices, ovk::mem::MemoryType::cpu_coherent_and_cached);

  // 5. Command Buffers
  // Also dynamic because we will need as many a we have swapchain images
  std::vector<ovk::RenderCommand> commands;

  // 6. Sync Primitives
  constexpr auto MAX_FRAMES_IN_FLIGHT = 2;
  struct {
    std::vector<ovk::Semaphore> image_available, render_finished;
    std::vector<ovk::Fence> in_flight_fences;
    uint32_t current_frame = 0;
  } sync;

  sync.image_available = device.create_semaphores(MAX_FRAMES_IN_FLIGHT);
  sync.render_finished = device.create_semaphores(MAX_FRAMES_IN_FLIGHT);
  sync.in_flight_fences = device.create_fences(
      MAX_FRAMES_IN_FLIGHT, vk::FenceCreateFlagBits::eSignaled);

	spdlog::info("Static Objects created!");
	
  // Dynamic Objects (only pipeline, framebuffers and command buffers)
  const auto create_dynamic_objects = [&]() {
    // Dynamic meaning swapchain dependant
    // In this case this will just be the pipeline
    // ovk::make_unique is a helper function to remove distracting std::move s
    // build_pipeline() actually provides default values for most of the parts
    // of the pipeline like rasterizer
    pipeline = ovk::make_unique(
        device.build_pipeline()
            .set_render_pass(render_pass, /*subpass index: */ 0)
            // This will add a Spir-V Shader from a File
            .add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex,
                                        "res/shaders/triangle.vert.spv")
            .add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment,
                                        "res/shaders/triangle.frag.spv")
            // This actually does a bit of magic and figures out the Vertex
            // Layout through reflection
            .set_vertex_layout<VertexLayout>()
            .add_viewport(glm::vec2(0, 0),
                          glm::vec2(swapchain.swap_extent.width,
                                    swapchain.swap_extent.height),
                          0.0f, 1.0f)
            .add_scissor(vk::Offset2D(0, 0), swapchain.swap_extent)
            .build()

    );

    // Framebuffers
    framebuffers.clear();
    framebuffers.reserve(swapchain.image_count);
    for (auto i = 0; i < swapchain.image_count; i++) {
      auto &swap_image = swapchain.image_views[i];
      framebuffers.push_back(
          std::forward<ovk::Framebuffer>(device.create_framebuffer(
              render_pass,
              vk::Extent3D(swapchain.swap_extent.width,
                           swapchain.swap_extent.height, 1),
              {swap_image})));
    }

    // Command Buffers
    commands = device.create_render_commands(
        swapchain.image_count, [&](ovk::RenderCommand &cmd, const int i) {
          cmd.begin_render_pass(render_pass, framebuffers[i],
                                swapchain.swap_extent, {glm::vec4(1.0)},
                                vk::SubpassContents::eInline);

          cmd.bind_graphics_pipeline(*pipeline);
          cmd.bind_vertex_buffers(0, {ovk::RenderCommand::BufferDescription{
                                         std::ref(vertex_buffer), 0}});
          cmd.draw(3, 1, 0, 0);

          cmd.end_render_pass();
        });
  };

  // Actually call the create function
  create_dynamic_objects();

	spdlog::info("Dynamic Objects created!");
	
  // if the swapchain becomes invalid
  const auto recreate_swapchain = [&]() {
    device.wait_idle();

    int width = 0, height = 0;
    glfwGetFramebufferSize(surface.window.get(), &width, &height);
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(surface.window.get(), &width, &height);
      glfwWaitEvents();
    }

    swapchain.handle.invalidate();
    swapchain = std::move(device.create_swapchain(surface));

    create_dynamic_objects();
  };

  // To keep track of the current swapchain image
  auto swapchain_index = 0;
	spdlog::info("Entering Loop!");
  while (surface.update()) {
    // Acquire new image
    device.wait_fences({sync.in_flight_fences[sync.current_frame]});

    auto [recreate, index] = device.acquire_image(
        swapchain, sync.image_available[sync.current_frame]);
    if (recreate) {
      recreate_swapchain();
    }

    swapchain_index = index;

    device.reset_fences({sync.in_flight_fences[sync.current_frame]});

    device.submit(
        {ovk::WaitInfo{sync.image_available[sync.current_frame],
                       vk::PipelineStageFlagBits::eColorAttachmentOutput}},
        {commands[swapchain_index].cmd_handle},
        {sync.render_finished[sync.current_frame]},
        sync.in_flight_fences[sync.current_frame]);

    recreate = device.present_image(swapchain, swapchain_index,
                                    {sync.render_finished[sync.current_frame]});
    if (recreate)
      recreate_swapchain();

    sync.current_frame = (sync.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  device.wait_idle();
  // -> RAII does its stuff autmagically
}

int main(int argc, char** argv) {
	try {
		run_example();
	} catch(const std::exception& e) {
		spdlog::critical("Exception Thrown");
		spdlog::critical(e.what());
	}
}
