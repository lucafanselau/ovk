#include <base/instance.h>
#include <base/device.h>
#include "spdlog/sinks/ansicolor_sink.h"

#include <gui/gui_renderer.h>
#include "game.h"
#include "graphics/renderer.h"


struct Application {
	Application();
	void run();

	/*void create_const_objects();
	void create_sc_objects();

	void build_command_buffer(int index, ovk::RenderCommand& cmd);

	void recreate_swapchain();*/

	ovk::Instance instance;
	std::shared_ptr<ovk::Surface> surface;
	std::shared_ptr<ovk::Device> device;
	std::shared_ptr<ovk::SwapChain> swapchain;

	std::shared_ptr<MasterRenderer> renderer;

	std::unique_ptr<Game> game;
	
	// Const Objects
	//std::shared_ptr<ovk::RenderPass> render_pass;
	//std::unique_ptr<ovk::ImGuiRenderer> imgui;
	
	//
	//// Swapchain dependant Objects
	//struct {
	//	std::vector<ovk::Framebuffer> swapchain_framebuffers;
	//	std::vector<std::unique_ptr<ovk::RenderCommand>> command_buffers;
	//} dynamic;
	//struct {
	//	std::unique_ptr<ovk::Image> image;
	//	std::unique_ptr<ovk::ImageView> view;
	//} depth;



	//glm::vec4 color = glm::vec4(62. / 255.f, 102 / 255.f, 60 / 255.f, 1.0f);

};



Application::Application()
	: instance(ovk::AppInfo{ "MightCity", 0, 0, 1 }, {}),
		surface(ovk::make_shared(instance.create_surface(2600, 1600, "Mighty City", true))),
		device(ovk::make_shared(instance.create_device(
			{ VK_KHR_SWAPCHAIN_EXTENSION_NAME}, 
			vk::PhysicalDeviceFeatures().setFillModeNonSolid(true).setSamplerAnisotropy(true),
			*surface))),
		swapchain(ovk::make_shared(device->create_swapchain(*surface))),
		renderer(std::make_shared<MasterRenderer>(device, swapchain, surface)) {	

	game = std::make_unique<Game>(device, swapchain, renderer);
	
}

void Application::run() {
	while(surface->update()) {

		// Figure out delta time
		static int fps = 0;
		float delta;
		{
		using namespace std::chrono;

			static auto timer = 0.0f;
			static int frames = 0;

			static auto startup = high_resolution_clock::now();

			auto now = high_resolution_clock::now();
			auto time = duration<float, seconds::period>(now - startup).count();
			static auto last_time = time;

			delta = time - last_time;
			last_time = time;

			frames++;
			
			timer += delta;
			if (timer >= 1.0f) {
				timer -= 1.0f;
				fps = frames;
				frames = 0;
			}

		}
		// Update the renderer (acquire new image etc.)
		if (renderer->update(delta)) continue;

		game->update(delta, renderer->get_index());

		ImGui::Begin("Stats");
		ImGui::Text("fps: %i", fps);
		ImGui::End();
		
		game->render();
		renderer->render();
	}

	device->wait_idle();
}

//void Application::run() {
//	// Sync Thingys
//	auto image_available = device->create_semaphores(MAX_FRAMES_IN_FLIGHT);
//	auto render_finished = device->create_semaphores(MAX_FRAMES_IN_FLIGHT);
//	auto in_flight_fences = device->create_fences(MAX_FRAMES_IN_FLIGHT, vk::FenceCreateFlagBits::eSignaled);
//	
//	uint32_t current_frame = 0;
//
//	while (surface.update()) {
//
//		device->wait_fences({ in_flight_fences[current_frame] });
//
//		auto [recreate, index] = device->acquire_image(*swapchain, image_available[current_frame]);
//		if (recreate) {
//			recreate_swapchain(); continue;
//		}
//
//		{
//			using namespace std::chrono;
//			static int frames = 0;
//			static auto timer = 0.0f;
//
//			static auto startup = high_resolution_clock::now();
//
//			auto now = high_resolution_clock::now();
//			auto time = duration<float, seconds::period>(now - startup).count();
//			static auto last_time = time;
//
//			auto delta = time - last_time;
//			last_time = time;
//
//			static auto fps = 0;
//
//			frames++;
//			timer += delta;
//			const auto update_rate = 0.5f;
//			if (timer >= update_rate) {
//				timer -= update_rate;
//				fps = frames / update_rate;
//				frames = 0;
//			}
//
//			ImGui::NewFrame();
//
//			ImGui::Begin("Stats");
//
//			ImGui::Text("fps: %i", fps);
//			ImGui::Text("frame time: %f", (1 / static_cast<float>(fps) * 1000));
//			ImGui::ColorPicker4("Clear Color", &color.x);
//			
//			ImGui::End();
//
//			ImGui::ShowDemoWindow();
//			
//			imgui->update();
//			game->update(delta, index);
//
//			ImGui::Render();
//		}
//
//		device->reset_fences({ in_flight_fences[current_frame] });
//
//		if (dynamic.command_buffers[index]) {
//			static auto& pool = device->get_command_pool(ovk::QueueType::graphics);
//			device->device->freeCommandBuffers(pool, { dynamic.command_buffers[index]->cmd_handle });
//		}
//
//		dynamic.command_buffers[index] = std::make_unique<ovk::RenderCommand>(device->create_render_commands(1, [&](ovk::RenderCommand& cmd, const int) {
//			build_command_buffer(index, cmd);
//		})[0]);
//
//
//		device->submit(
//			{ ovk::WaitInfo{ image_available[current_frame], vk::PipelineStageFlagBits::eColorAttachmentOutput } },
//			{ dynamic.command_buffers[index]->cmd_handle },
//			{ render_finished[current_frame] },
//			in_flight_fences[current_frame]
//		);
//
//		recreate = device->present_image(*swapchain, index, { render_finished[current_frame] });
//		if (recreate) recreate_swapchain();
//
//		current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
//
//	}
//
//	device->wait_idle();
//}
//
//void Application::create_const_objects() {
//
//	vk::AttachmentDescription depth_attachment {
//		{},
//		device->default_depth_format().value(),
//		vk::SampleCountFlagBits::e1,
//		vk::AttachmentLoadOp::eClear,
//		vk::AttachmentStoreOp::eDontCare,
//		vk::AttachmentLoadOp::eDontCare,
//		vk::AttachmentStoreOp::eDontCare,
//		vk::ImageLayout::eUndefined,
//		vk::ImageLayout::eDepthStencilAttachmentOptimal
//	};
//
//
//	const auto color_attachment = swapchain->get_color_attachment_description();
//
//	const ovk::GraphicSubpass subpass{
//		{},
//		{ color_attachment},
//		{},
//		{},
//		depth_attachment
//	};
//	
//	render_pass = ovk::make_shared(device->create_render_pass({ color_attachment, depth_attachment }, { subpass }, true));
//
//	imgui = std::make_unique<ovk::ImGuiRenderer>(*render_pass, *swapchain, surface, *device);
//	ImGui::SetCurrentContext(imgui->context);
//	ImGui::GetIO().FontGlobalScale = 1.4f;
//
//	game = std::make_unique<Game>(device, swapchain, render_pass);
//}
//
//void Application::create_sc_objects() {
//	// Create Depth Ressources
//	auto depth_format = device->default_depth_format();
//	depth.image = ovk::make_unique(device->create_image(
//		vk::ImageType::e2D,
//		depth_format.value(),
//		vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
//		vk::ImageUsageFlagBits::eDepthStencilAttachment,
//		ovk::mem::MemoryType::device_local
//	));
//
//	depth.view = ovk::make_unique(device->view_from_image(*depth.image, vk::ImageAspectFlagBits::eDepth));
//
//	for (auto& swap_image : swapchain->image_views) {
//		dynamic.swapchain_framebuffers.push_back(
//			std::forward<ovk::Framebuffer>(
//				device->create_framebuffer(
//					*render_pass,
//					vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
//					{ swap_image, *depth.view })));
//	}
//
//	dynamic.command_buffers.resize(swapchain->image_count);
//}
//
//void Application::build_command_buffer(int index, ovk::RenderCommand &cmd) {
//	cmd.begin_render_pass(
//		*render_pass,
//		dynamic.swapchain_framebuffers[index],
//		*swapchain,
//		{ color, glm::vec2(1.0f, 0.0f) },
//		vk::SubpassContents::eInline);
//
//	{
//		cmd.begin_region("Game Rendering", glm::vec4(0.23f, 0.34f, 0.87f, 1.00f));
//		game->render(index, cmd);
//		cmd.end_region();
//	}
//	
//	{
//		cmd.begin_region("ImGui Rendering", glm::vec4(0.87f, 0.21f, 0.11f, 1.00f));
//		cmd.draw_imgui(*imgui, index, ImGui::GetDrawData());
//		cmd.end_region();
//	}
//	
//	cmd.end_render_pass();
//}
//
//void Application::recreate_swapchain() {
//	spdlog::info("recreating swapchain");
//
//	device->wait_idle();
//
//	dynamic = {};
//
//	swapchain->handle.invalidate();
//	*swapchain = std::move(device->create_swapchain(surface));
//
//	create_sc_objects();
//
//	imgui->recreate(*swapchain, *render_pass, *device);
//	game->recreate_swapchain();
//}

int main(int argc, char** argv) {

	// auto console = spdlog::default_factory::create<spdlog::sinks::ansicolor_stdout_sink_mt>("console");
	// spdlog::set_default_logger(console);
	
	Application a;
	a.run();

	return 0;
}
