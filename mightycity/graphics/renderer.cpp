#include "renderer.h"

#include <gui/gui_renderer.h>
#include <base/surface.h>

#include "../world/chunk.h"
#include "../world/world.h"

// Render Defines and constants
#define MAX_FRAMES_IN_FLIGHT 2
constexpr auto picker_format = vk::Format::eB8G8R8A8Unorm;
const auto picker_blit_extent = 100; /*px across*/
const vk::Extent2D shadow_extent(3000, 3000);


// =======================================================================================================================
// Master Renderer

MasterRenderer::MasterRenderer(std::shared_ptr<ovk::Device>& d, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Surface>& s)
	: device(d), swapchain(sc), surface(s),
		camera(glm::vec3(-30.0f, 10.0f, 30.0f), *swapchain) {

	create_const_objects();
	create_dynamic_objects();

	create_renderer();

	create_objects();

	//TMP: Formats
	/*auto supports_blit = [&](vk::Format format) {
		auto props = device->physical_device.getFormatProperties(format);
		if (props.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			return true;
		return false;
	};

	for (auto i = 0; i < 185; i++) {
		vk::Format format = static_cast<vk::Format>(i);
		spdlog::info("{}: support: {}", to_string(format), supports_blit(format));
	}*/
}

void MasterRenderer::post_init(ovk::Buffer* materials_buffer) {
	mesh->post_init(materials_buffer);
}

bool MasterRenderer::update(float dt) {
	// Acquire new image
	device->wait_fences({ sync.in_flight_fences[sync.current_frame] });

	auto [recreate, index] = device->acquire_image(*swapchain, sync.image_available[sync.current_frame]);
	if (recreate) {
		recreate_swapchain(); return true;
	}

	swapchain_index = index;

	ImGui::NewFrame();

	
	// Renderer Updates
	imgui->update();

	// Scene update
	{
		// Uniform Update
		camera.update(dt, true);

		static auto counter = 0.0f;
		counter += dt;


		// Tweakables
		static float r = 40;
		static float height = 20.0f;
		static float extent = 20.0f;

		ImGui::Begin("Master Renderer");
		ImGui::SliderFloat("height", &height, 10.0f, 100.0f);
		ImGui::SliderFloat("r", &r, 10.0f, 100.0f);
		ImGui::SliderFloat("extent", &extent, 1.0f, 40.0f);
		
		glm::vec3 light_pos = glm::vec3(glm::cos(counter / 2.0f) * r, height, glm::abs(glm::sin(counter / 2.0f)) * r);
		
		lights.light_dir = glm::normalize(-light_pos);
		lights.view_pos = camera.pos;
		
		// light buffer update (for shadow map)
		glm::vec3 camera_vec = glm::vec3(camera.pos.x, 0.0f, camera.pos.z);
		float light_length = glm::length(light_pos);
		static float camera_height = 20.0f; //  = (height * light_length) / r;
		ImGui::SliderFloat("camera_height", &camera_height, 0.0f, 70.0f); // ImGui::Text("%f", camera_height);
		glm::vec3 destination(20.0f, 5.0f, 20.0f);


		static float near_factor = 0.5f;
		static float far_factor = 1.8f;
		ImGui::SliderFloat("near_factor", &near_factor, 0.0f, 3.0f);
		ImGui::SliderFloat("far_factor", &far_factor, 0.0f, 3.0f);
		light_uniform = LightUniform {
			glm::lookAt(light_pos + destination, destination, glm::vec3(0.0f, 1.0f, 0.0f)),
			glm::ortho(-extent, extent, -camera_height, camera_height, -near_factor * light_length, far_factor * light_length)
		};

		light_uniform.projection[1][1] *= -1.f;

		// debug draw shadow depth map
		static bool show_shadow_depth = true;
		ImGui::Checkbox("Show Shadow Map", &show_shadow_depth);
		
		ImGui::End();

	}
	
	// terrain->update(dt, swapchain_index);
	
	// Renderer ImGui Windows
	device->get_default_allocator()->debug_draw();

	
	return false;
	
}

void MasterRenderer::render() {

	// Prepare new uniform buffers
	auto& camera_data = camera.get_data();
	device->update_buffer(dynamic.camera_uniform_buffers[swapchain_index], camera_data);

	device->update_buffer(dynamic.light_uniform_buffers[swapchain_index], lights);
	device->update_buffer(shadow.light_buffer[swapchain_index], light_uniform);
	
	// TODO: Maybe this shouldn't be here
	ImGui::Render();

	device->reset_fences({ sync.in_flight_fences[sync.current_frame] });

		if (dynamic.command_buffers[swapchain_index]) {
			static auto& pool = device->get_command_pool(ovk::QueueType::graphics);
			device->device->freeCommandBuffers(pool, { dynamic.command_buffers[swapchain_index]->cmd_handle });
		}

		dynamic.command_buffers[swapchain_index] = std::make_unique<ovk::RenderCommand>(device->create_render_commands(1, [&](ovk::RenderCommand& cmd, const int) {
			build_command_buffer(swapchain_index, cmd);
		})[0]);


		device->submit(
			{ ovk::WaitInfo{ sync.image_available[sync.current_frame], vk::PipelineStageFlagBits::eColorAttachmentOutput } },
			{ dynamic.command_buffers[swapchain_index]->cmd_handle },
			{ sync.render_finished[sync.current_frame] },
			sync.in_flight_fences[sync.current_frame]
		);

	const auto recreate = device->present_image(*swapchain, swapchain_index, { sync.render_finished[sync.current_frame] });
		if (recreate) recreate_swapchain();

		sync.current_frame = (sync.current_frame + 1) % MAX_FRAMES_IN_FLIGHT;


}

void MasterRenderer::draw_terrain(Chunk *mesh) const {
	terrain->draw(mesh);
}

void MasterRenderer::draw_mesh(Model *m, Transform transform) const {
	mesh->draw(m, transform);
}

std::optional<PickInfo> MasterRenderer::pick(glm::vec2 mouse_pos, Terrain& terrain) {

	// NOTE: we blit from the frame before that one so that rendering must be completed
	int wait_frame = sync.current_frame - 1;
	if (wait_frame < 0) wait_frame = MAX_FRAMES_IN_FLIGHT - 1;
	device->wait_fences({ sync.in_flight_fences[wait_frame]});
	
	// we need to blit around the cursor
	auto cmd = device->create_single_submit_cmd(ovk::QueueType::transfer);


	int blit_image_idx = get_index() - 1;
	if (blit_image_idx < 0) blit_image_idx = swapchain->image_count - 1;

	// picker.blit_image->transition_layout(cmd, vk::ImageLayout::eTransferDstOptimal, ovk::QueueType::transfer, *device);

	// Ok Image was already rendered to so it should be in vk::ImageLayout::eColorAttachmentOptimal
	// NOTE: this only needs to be done because subpasses dont report layout changes to images
	picker.color_targets[blit_image_idx].set_layout(vk::ImageLayout::eColorAttachmentOptimal);
	
	picker.color_targets[blit_image_idx].transition_layout(cmd, vk::ImageLayout::eTransferSrcOptimal, { ovk::QueueType::transfer }, *device);

	auto in_range = [](float v, float min, float max) {
	  return v >= min && v <= max;
  };
	
	vk::Extent3D actual_extent;
	if (in_range(mouse_pos.x, picker_blit_extent / 2, swapchain->swap_extent.width - picker_blit_extent / 2) &&
			in_range(mouse_pos.y, picker_blit_extent / 2, swapchain->swap_extent.height - picker_blit_extent / 2)) {
		actual_extent = vk::Extent3D(picker_blit_extent, picker_blit_extent, 1);
	} else {
		auto min_distance = std::min(
			std::min(mouse_pos.x, swapchain->swap_extent.width - mouse_pos.x),
			std::min(mouse_pos.y, swapchain->swap_extent.height - mouse_pos.y)
		);
		actual_extent = vk::Extent3D(min_distance * 2, min_distance * 2, 1);
	}
	
	// spdlog::info("actual_extent: [{}, {}]", actual_extent.width, actual_extent.height);
	
	const vk::BufferImageCopy region {
		0, 0, 0,
		vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
		vk::Offset3D(mouse_pos.x - actual_extent.width / 2, mouse_pos.y - actual_extent.height / 2),
		actual_extent
	};

	cmd.copyImageToBuffer(
		picker.color_targets[blit_image_idx], vk::ImageLayout::eTransferSrcOptimal,
		*picker.pick_buffer,
		{ region }
	);
	
	device->flush(cmd, ovk::QueueType::transfer, true, true);

	// Now we should have some data in the buffer so lets map it and try to find out data
	// The middle pixel (eg. the mouse cursor) should be at the index = 4 (because 4 bytes of memory per pixel) * (picker_blit_extent.width + 1) * (picker_blit_extent / 2) 
	const auto buffer_data = static_cast<uint8_t*>(picker.pick_buffer->memory->map(*device));

	const uint8_t* middle_data = buffer_data + 4 * (actual_extent.width + 1) * (actual_extent.width / 2);

	const uint8_t b = *middle_data, g = *(middle_data + 1), r = *(middle_data + 2), a = *(middle_data + 3);
	
	picker.pick_buffer->memory->unmap(*device);

	// If white than there is nothing to click
	if (r == 255 && b == 255 && g == 255) return std::nullopt;

	// HARDCODE: Copy from terrain_picker.vert
	const auto max_chunks_1D = 32;

	glm::vec3 color = (1 / 255.0f) * glm::vec3(r, g, b);
	Chunk* found_chunk = nullptr;

	{
		float x = round(color.r * max_chunks_1D);
		float z = round(color.b * max_chunks_1D);

		found_chunk = terrain.get_chunk(x, z);

		ovk_assert(found_chunk);
		// spdlog::info("Clicked at chunk: [{}, 0, {}]", x, z);
	}
	
	auto triangle_index = static_cast<uint32_t>(round( color.g * (float)(chunk_size * chunk_size * 2)));
	auto tile_index = triangle_index / 2;

	auto triangle_type = triangle_index - 2 * tile_index;

	auto x = tile_index % chunk_size;
	auto z = tile_index / chunk_size;

	auto height = found_chunk->get_height(x, z);
	// auto triangle_type = triangle_index - (tile_index * 2);

	// calculate mouse ray
	float ray_x = (2.0f * mouse_pos.x) / swapchain->swap_extent.width - 1.0f;
	float ray_y = (2.0f * mouse_pos.y) / swapchain->swap_extent.height - 1.0f;

	auto &camera_data = camera.get_data();
	
	glm::vec3 ray_nds = glm::vec3(ray_x, ray_y, 1.0f);
	glm::vec4 ray_clip(ray_nds.x, ray_nds.y, -1.0f, 1.0f);

	glm::vec4 ray_eye = glm::inverse(camera_data.projection) * ray_clip;
	ray_eye.z = -1.0f; ray_eye.w = 0.0f;

	glm::vec4 ray_wor = glm::inverse(camera_data.view) * ray_eye;
	ray_wor = glm::normalize(ray_wor);

	// spdlog::info("Calculated Ray: [{}, {}, {}]", ray_wor.x, ray_wor.y, ray_wor.z);

	// Ok now we have a ray into the game world
	// We need to figure out where this ray intersects with the triangle
	// Our Triangles are created like this
	// Triangle I: 0, 1, 3
	// Triangle II: 0, 3, 2
	// where:
	// 0(0, 0), 1(1, 0), 2(0, 1), 3(1, 1)
	//
 
	auto base = glm::vec3(found_chunk->pos.x + x, 0.0, found_chunk->pos.y + z);
	auto zero = base + glm::vec3(0, found_chunk->get_height(x, z), 0);
	auto one = base + glm::vec3(1, found_chunk->get_height(x + 1, z), 0);
	auto two = base + glm::vec3(0, found_chunk->get_height(x, z + 1), 1);
	auto three = base + glm::vec3(1, found_chunk->get_height(x + 1, z + 1), 1);

	struct {
		glm::vec3 normal;
		float a;
	} plane;

	struct {
		glm::vec3 pos;
		glm::vec3 dir;
	} ray;
	
	if (triangle_type == 0) {
		// We are handling Triangle I
		// normal = 01 x 03
		// TODO: Think about direction (but honestly i du not care)
		plane.normal = glm::normalize(glm::cross(one - zero, three - zero));
		plane.a = glm::dot(plane.normal, zero);
	} else if (triangle_type == 1) {
		plane.normal = glm::normalize(glm::cross(three - zero, two - zero));
		plane.a = glm::dot(plane.normal, zero);
	}

	ray.pos = camera.pos;
	ray.dir = ray_wor;

	auto lambda = (plane.a - glm::dot(plane.normal, ray.pos)) / glm::dot(plane.normal, ray.dir);

	auto hit = ray.pos + lambda * ray.dir;

	// we assert that point to be (almost) on the plane
	ovk_assert(glm::dot(hit, plane.normal) - plane.a < 1e-4);
	// spdlog::info("diffrence: {}", glm::dot(hit, plane.normal) - plane.a);
	
	// spdlog::info("at tile: [{}, {}]", x, z);
	// spdlog::info("intersection: [{}, {}, {}]", hit.x, hit.y, hit.z);

	// return glm::vec3(found_chunk->pos.x + x, height, found_chunk->pos.y + z);
	/*
		struct PickInfo {
		Chunk* chunk;
		int local_x, local_z;
		uint8_t triangle_type;
		glm::vec3 triangle_base;
		glm::vec3 hit;
		};
	*/
	return PickInfo {
		found_chunk,
		(int) x, (int) z,
		(uint8_t) triangle_type,
		zero,
		hit
	};
	// So now we need to figure out which triangle was pressed
	
	// // NOTE: Knowledge of those to is required but we should not have a copy of those values here
	// // ERROR PRONE
	// const auto range = 128;
	// const auto scale = 0.5f;

	// auto sample_at = [&](int x, int z) {
	// 	//const auto offset = 0.5;
	// 	const auto dr2 = (double)range / 2.0;
	// 	return _noise->GetValue((double)x / dr2, 0.0, (double)z / dr2);
	// };
	// 	// convert the data back to world coordinates
	// auto x = round((static_cast<float>(r) / 255.f) * 128.0f);
	// auto z = round((static_cast<float>(b) / 255.f) * 128.0f);

	// x -= (range / 2);
	// z -= (range / 2);

	// float y = sample_at(x, z);
	
	// x *= scale;
	// z *= scale;
	
	// if (r == 0 && b == 0) return {};

	// // spdlog::info("picked_color: [{}, {}, {}, {}] therefore: [{}, {}]", r, g, b, a, x, z);
	
	// return glm::vec3(x, y, z);
	
}

//void MasterRenderer::update_picker() {
//
//	device->wait_fences({ picker.finished_fence->handle.get() });
//
//	auto cmd = device->create_render_commands(1, [this](ovk::RenderCommand& cmd, int index) {
//		cmd.begin_render_pass(
//			*picker.render_pass, 
//			*picker.framebuffer, 
//			swapchain->swap_extent, 
//			{ glm::vec4(0.0), glm::vec2(1.0f, 0.0f) }, 
//			vk::SubpassContents::eInline
//		);
//
//
//		cmd.end_render_pass();
//		
//	});
//
//	ovk_assert(cmd.size() == 1);
//
//	device->reset_fences({ *picker.finished_fence });
//	device->submit({}, { cmd[0].cmd_handle }, {}, picker.finished_fence->handle.get());
//	
//}

uint32_t MasterRenderer::get_index() {
	return swapchain_index;
}

void MasterRenderer::wait_last_frame() {
	uint32_t last_frame;
	if (sync.current_frame == 0)
		last_frame = MAX_FRAMES_IN_FLIGHT - 1;
	else
		last_frame = sync.current_frame - 1;
	
	device->wait_fences({ sync.in_flight_fences[last_frame] });
}

void MasterRenderer::create_const_objects() {

	const vk::AttachmentDescription depth_attachment {
		{},
		device->default_depth_format().value(),
		vk::SampleCountFlagBits::e1,
		vk::AttachmentLoadOp::eClear,
		vk::AttachmentStoreOp::eDontCare,
		vk::AttachmentLoadOp::eDontCare,
		vk::AttachmentStoreOp::eDontCare,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eDepthStencilAttachmentOptimal
	};																										 

	auto color_attachment = swapchain->get_color_attachment_description();

	
	const ovk::GraphicSubpass main_pass {
		{},
		{ color_attachment},
		{},
		{},
		depth_attachment
	};
	
	render_pass = ovk::make_unique(
		device->create_render_pass(
			{ color_attachment, depth_attachment },
			{ main_pass }, true
		)
	);


	// Shadow Setup
	{

		const vk::AttachmentDescription shadow_depth_attachment {
			{},
			device->default_depth_format().value(),
			vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear,
			vk::AttachmentStoreOp::eStore,
			vk::AttachmentLoadOp::eDontCare,
			vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthStencilAttachmentOptimal
		};
		
		const ovk::GraphicSubpass shadow_prepass {
			{},
			{},
			{},
			{},
			shadow_depth_attachment
		};

		shadow.renderpass = ovk::make_unique(
			device->create_render_pass(
				{ shadow_depth_attachment },
				{ shadow_prepass }, true
			)
		);

		shadow.descriptor_template = ovk::make_unique(device->build_descriptor_template()
		.add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
		.build());

	}
	
	lights = LightData { glm::vec3(100.0f, 100.f, 100.f), camera.pos };
	
	sync.image_available = device->create_semaphores(MAX_FRAMES_IN_FLIGHT);
	sync.render_finished = device->create_semaphores(MAX_FRAMES_IN_FLIGHT);
	sync.in_flight_fences = device->create_fences(MAX_FRAMES_IN_FLIGHT, vk::FenceCreateFlagBits::eSignaled);

	// Picker Const Things
	{
		color_attachment.format = picker_format;
		color_attachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
		
		const ovk::GraphicSubpass picker_subpass{
			{},
			{ color_attachment },
			{},
			{},
			depth_attachment
		};
		
		picker.render_pass = ovk::make_unique(device->create_render_pass({ color_attachment, depth_attachment}, { picker_subpass }, true));

	}
	
}

void MasterRenderer::create_dynamic_objects() {

	auto depth_format = device->default_depth_format();
	
	// Create Shadow Depth Targets
	shadow.depth_targets.clear();
	shadow.depth_views.clear();
	shadow.framebuffers.clear();
	for (int i = 0; i < swapchain->image_count; i++) {
		shadow.depth_targets.push_back(
			std::forward<ovk::Image>(
				device->create_image(
					vk::ImageType::e2D,
					depth_format.value(),
					vk::Extent3D(shadow_extent.width, shadow_extent.height, 1),
					vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
					vk::ImageTiling::eOptimal,
					ovk::mem::MemoryType::device_local)));
		
		shadow.depth_views.push_back(
			std::forward<ovk::ImageView>(
				device->view_from_image(
					shadow.depth_targets[i],
					vk::ImageAspectFlagBits::eDepth )));

		shadow.framebuffers.push_back(
			std::forward<ovk::Framebuffer>(
				device->create_framebuffer(
					*shadow.renderpass,
					vk::Extent3D(shadow_extent.width, shadow_extent.height, 1),
					{ shadow.depth_views[i] })));


	}

	shadow.descriptor_pool = ovk::make_unique(device->create_descriptor_pool(
																							{ shadow.descriptor_template.get() },
																							{ swapchain->image_count }
																						));

	shadow.descriptor_sets = device->make_descriptor_sets(*shadow.descriptor_pool, swapchain->image_count, *shadow.descriptor_template);
	
	light_uniform = LightUniform {
		glm::mat4(1.0),
		glm::mat4(1.0)
	};
	
	for (auto i = 0; i < swapchain->image_count; i++) {
		shadow.light_buffer.push_back(std::move(device->create_uniform_buffer(light_uniform, ovk::mem::MemoryType::cpu_coherent_and_cached, { ovk::QueueType::graphics })));
		
		shadow.descriptor_sets[i].write(shadow.light_buffer[i], 0, 0);
	}
		
	// Create Depth Ressources
	depth.image = ovk::make_unique(device->create_image(
		vk::ImageType::e2D,
		depth_format.value(),
		vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
		vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal, ovk::mem::MemoryType::device_local
	));

	depth.view = ovk::make_unique(device->view_from_image(*depth.image, vk::ImageAspectFlagBits::eDepth));

	for (auto i = 0; i < swapchain->image_count; i++) {
		auto &swap_image = swapchain->image_views[i];
		dynamic.swapchain_framebuffers.push_back(
			std::forward<ovk::Framebuffer>(
				device->create_framebuffer(
					*render_pass,
					vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
					{ swap_image, *depth.view })));
	}

	dynamic.command_buffers.resize(swapchain->image_count);

	auto& camera_data = camera.get_data();

	for (auto i = 0; i < swapchain->image_count; i++) {
		dynamic.camera_uniform_buffers.push_back(std::move(device->create_uniform_buffer(camera_data, ovk::mem::MemoryType::cpu_coherent_and_cached, { ovk::QueueType::graphics })));
		dynamic.light_uniform_buffers.push_back(std::move(device->create_uniform_buffer(lights, ovk::mem::MemoryType::cpu_coherent_and_cached, { ovk::QueueType::graphics })));
	}

	// Picker dynamic Stuff
	{
		picker.depth_image = ovk::make_unique(device->create_image(
			vk::ImageType::e2D,
			depth_format.value(),
			vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
			vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::ImageTiling::eOptimal, ovk::mem::MemoryType::device_local
		));

		picker.depth_view = ovk::make_unique(device->view_from_image(*picker.depth_image, vk::ImageAspectFlagBits::eDepth));

		picker.color_targets.clear();
		picker.color_target_views.clear();
		picker.framebuffers.clear();
		
		for (auto i = 0; i < swapchain->image_count; i++) {

			picker.color_targets.push_back(
				std::move(device->create_image(
					vk::ImageType::e2D, 
					picker_format, 
					vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
					vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment, vk::ImageTiling::eOptimal, ovk::mem::MemoryType::device_local
				)));

			picker.color_target_views.push_back(
				std::move(device->view_from_image(picker.color_targets[i], vk::ImageAspectFlagBits::eColor)));

			picker.framebuffers.push_back(std::move(
				device->create_framebuffer(
					*picker.render_pass,
					vk::Extent3D(swapchain->swap_extent.width, swapchain->swap_extent.height, 1),
					{picker.color_target_views[i], picker.depth_view->handle.get()}
			)));

			// because format required 4 bytes (maybe we should have a utility function in ovk that tells us the size of given format)
			const vk::DeviceSize buffer_size = picker_blit_extent * picker_blit_extent * 4;

			picker.pick_buffer = ovk::make_unique(device->create_buffer(
				vk::BufferUsageFlagBits::eTransferDst,
				buffer_size,
				nullptr,
				{ ovk::QueueType::transfer },
				ovk::mem::MemoryType::cpu_coherent_and_cached
			));
			
		}

	}

}

void MasterRenderer::create_renderer() {
	imgui = std::make_unique<ovk::ImGuiRenderer>(*render_pass, *swapchain, *surface, *device);
	ImGui::SetCurrentContext(imgui->context);
	ImGui::GetIO().FontGlobalScale = 1.4f;

	terrain = std::make_unique<TerrainRenderer>(this);
	mesh = std::make_unique<MeshRenderer>(this);
}

void MasterRenderer::create_objects() {

}

void MasterRenderer::recreate_swapchain() {

	device->wait_idle();

	int width = 0, height = 0;
	glfwGetFramebufferSize(surface->window.get(), &width, &height);
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(surface->window.get(), &width, &height);
		glfwWaitEvents();
	}
	
	dynamic = {};
	depth = {};

	swapchain->handle.invalidate();
	*swapchain = std::move(device->create_swapchain(*surface));

	create_dynamic_objects();

	imgui->recreate(*swapchain, *render_pass, *device);
	terrain->recreate_swapchain();
	mesh->recreate();

	create_objects();
	
}

void MasterRenderer::build_command_buffer(uint32_t index, ovk::RenderCommand &cmd) {

	// First we will render to the color picker target

	cmd.begin_region("Picker Pass", glm::vec4(0.12f, 0.76f, 0.82f, 1.0f));
	{
		cmd.begin_render_pass(
				*picker.render_pass, 
				picker.framebuffers[index], 
				swapchain->swap_extent, 
				{ glm::vec4(1.0), glm::vec2(1.0f, 0.0f) }, 
				vk::SubpassContents::eInline
			);

		terrain->on_picker_render(index, cmd);

		cmd.end_render_pass();
	}
	cmd.end_region();

	cmd.begin_render_pass(
		*shadow.renderpass,
		shadow.framebuffers[index],
		shadow_extent,
		{ glm::vec2(1.0f, 0.0f) },
		vk::SubpassContents::eInline);
	{
		// on shadow rendering
		terrain->on_shadow_render(index, cmd);
		mesh->on_shadow_render(index, cmd);
	}
	cmd.end_render_pass();


	// Transition the image layout of the shadow depth attachment
	{
		auto &shadow_attachment = shadow.depth_targets[index];
		shadow_attachment.set_layout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
		shadow_attachment.transition_layout(cmd.cmd_handle, vk::ImageLayout::eShaderReadOnlyOptimal, ovk::QueueType::graphics, *device);
	}
	// Main Render Pass
	cmd.begin_render_pass(
		*render_pass,
		dynamic.swapchain_framebuffers[index],
		swapchain->swap_extent,
		{ glm::vec4(1.0f), glm::vec2(1.0f, 0.0f) },
		vk::SubpassContents::eInline);

	{
		cmd.begin_region("Game Rendering", glm::vec4(0.23f, 0.34f, 0.87f, 1.00f));
		terrain->on_inline_render(index, cmd);
		mesh->on_inline_render(index, cmd);
		cmd.end_region();
	}
	
	{
		cmd.begin_region("ImGui Rendering", glm::vec4(0.87f, 0.21f, 0.11f, 1.00f));
		cmd.draw_imgui(*imgui, index, ImGui::GetDrawData());
		cmd.end_region();
	}
	
	cmd.end_render_pass();
}

// =======================================================================================================================
// Terrain Renderer
TerrainRenderer::TerrainRenderer(MasterRenderer *p)  : parent(p) {
	create_const_objects();
	create_dynamic_objects();
}

void TerrainRenderer::create_const_objects() {
	descriptor_template = ovk::make_unique(parent->device->build_descriptor_template()
		.add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
		.add_uniform_buffer(1, vk::ShaderStageFlagBits::eFragment)
		.add_uniform_buffer(2, vk::ShaderStageFlagBits::eVertex)
    .add_sampler(3, vk::ShaderStageFlagBits::eFragment)
		.build());

	// Shadow Pipeline
	shadow.pipeline = ovk::make_unique(
		parent->device->build_pipeline()
		.set_render_pass(*parent->shadow.renderpass, 0)
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex, "res/shader/shadow.vert.spv")
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment, "res/shader/shadow.frag.spv")
		.set_vertex_layout<TerrainVertex>()
		.add_descriptor_set_layouts({ parent->shadow.descriptor_template->handle.get() })
		.set_rasterizer(
			vk::PipelineRasterizationStateCreateInfo(
				{},
				false,
				false,
				vk::PolygonMode::eFill,
				// TODO: needs to be redone
				vk::CullModeFlagBits::eNone,
				vk::FrontFace::eClockwise,
				false, 0, 0, 0,
				1.0f)
		)
		.set_depth_stencil()
		.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4))
		// TODO: Dynamic State (duh.)
		.add_viewport(glm::vec2(0, 0), glm::vec2(shadow_extent.width, shadow_extent.height), 0.0f, 1.0f)
		.add_scissor(vk::Offset2D(0, 0), shadow_extent)
		.build()
	);
	
}

void TerrainRenderer::create_dynamic_objects() {

	auto& swapchain = parent->swapchain;
	
	dynamic.pipeline = ovk::make_unique(
		parent->device->build_pipeline()
		.set_render_pass(*parent->render_pass, 0)
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex, "res/shader/terrain.vert.spv")
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment, "res/shader/terrain.frag.spv")
		.set_vertex_layout<TerrainVertex>()
		.add_descriptor_set_layouts({ descriptor_template->handle.get() })
		.set_rasterizer(
			vk::PipelineRasterizationStateCreateInfo(
				{},
				false,
				false,
				vk::PolygonMode::eFill,
				// TODO: needs to be redone
				vk::CullModeFlagBits::eNone,
				vk::FrontFace::eClockwise,
				false, 0, 0, 0,
				1.0f)
		)
		.set_depth_stencil()
		.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::vec2))
		// TODO: Dynamic State (duh.)
		.add_viewport(glm::vec2(0, 0), glm::vec2(swapchain->swap_extent.width, swapchain->swap_extent.height), 0.0f, 1.0f)
		.add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
		.build()
	);
	
	dynamic.descriptor_pool = ovk::make_unique(parent->device->create_descriptor_pool({ descriptor_template.get() }, { swapchain->image_count }));

	dynamic.descriptor_sets = parent->device->make_descriptor_sets(*dynamic.descriptor_pool, swapchain->image_count, *descriptor_template);
	
	for (auto i = 0; i < swapchain->image_count; i++) {
		dynamic.descriptor_sets[i].write(parent->dynamic.camera_uniform_buffers[i], 0, 0);
		dynamic.descriptor_sets[i].write(parent->dynamic.light_uniform_buffers[i], 0, 1);
		dynamic.descriptor_sets[i].write(parent->shadow.light_buffer[i], 0, 2);
		dynamic.descriptor_sets[i].write(
			parent->device->get_default_linear_sampler(),
			parent->shadow.depth_views[i],
			vk::ImageLayout::eShaderReadOnlyOptimal,
			3
		);
	}

	// Picker related stuff
	dynamic.picker_pipeline = ovk::make_unique(
		parent->device->build_pipeline()
 		.set_render_pass(*parent->picker.render_pass, 0)
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex, "res/shader/terrain_picker.vert.spv")
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment, "res/shader/terrain_picker.frag.spv")
		.set_vertex_layout<TerrainVertex>()
		.add_descriptor_set_layouts({ descriptor_template->handle.get() })
		.set_rasterizer(
			vk::PipelineRasterizationStateCreateInfo(
				{},
				false,
				false,
				vk::PolygonMode::eFill,
				// TODO: needs to be redone
				vk::CullModeFlagBits::eNone,
				vk::FrontFace::eClockwise,
				false, 0, 0, 0,
				1.0f)
		)
		.set_depth_stencil()
		.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::vec2))		
		// TODO: Dynamic State (duh.)
		.add_viewport(glm::vec2(0, 0), glm::vec2(swapchain->swap_extent.width, swapchain->swap_extent.height), 0.0f, 1.0f)
		.add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
		.build()
	);
}

void TerrainRenderer::recreate_swapchain() {
	create_dynamic_objects();
}

void TerrainRenderer::draw(Chunk* chunk) {
	jobs.push_back(chunk);
}

void TerrainRenderer::on_shadow_render(int index, ovk::RenderCommand& cmd) {

	cmd.bind_graphics_pipeline(*shadow.pipeline);
	cmd.bind_descriptor_sets(*shadow.pipeline, 0, { parent->shadow.descriptor_sets[index] });
	
	for (auto* chunk : jobs) {

		glm::mat4 model_matrix = glm::mat4(1.0f);
		model_matrix = glm::translate(model_matrix, glm::vec3(chunk->pos.x, 0.0f, chunk->pos.y));
		
		cmd.push_constant(model_matrix, *shadow.pipeline, vk::ShaderStageFlagBits::eVertex, 0);
		cmd.bind_vertex_buffers( 0, { ovk::RenderCommand::BufferDescription{ std::ref(chunk->mesh->vertex), 0}});
		// cmd.bind_index_buffer(mesh->index, 0, vk::IndexType::eUint16);
		cmd.draw(chunk->mesh->vertices_count, 1, 0, 0);
	}
	
}

void TerrainRenderer::on_inline_render(int index, ovk::RenderCommand &cmd) {
	// Bind Pipeline
	cmd.bind_graphics_pipeline(*dynamic.pipeline);
	cmd.bind_descriptor_sets(*dynamic.pipeline, 0, { dynamic.descriptor_sets[index] });

	// Terrain Rendering
	cmd.annotate("Render Terrain!", glm::vec4(0.25f, 0.67f, 0.97f, 1.00f));
	for (auto* chunk : jobs) {
		cmd.push_constant(chunk->pos, *dynamic.pipeline, vk::ShaderStageFlagBits::eVertex, 0);
		cmd.bind_vertex_buffers( 0, { ovk::RenderCommand::BufferDescription{ std::ref(chunk->mesh->vertex), 0}});
		// cmd.bind_index_buffer(mesh->index, 0, vk::IndexType::eUint16);
		cmd.draw(chunk->mesh->vertices_count, 1, 0, 0);
	}

	jobs.clear();
}

void TerrainRenderer::on_picker_render(int index, ovk::RenderCommand& cmd) {

	// Bind Pipeline
	cmd.bind_graphics_pipeline(*dynamic.picker_pipeline);
	cmd.bind_descriptor_sets(*dynamic.picker_pipeline, 0, { dynamic.descriptor_sets[index] });

	// Terrain Rendering
	for (auto* chunk : jobs) {
		cmd.push_constant(chunk->pos, *dynamic.picker_pipeline, vk::ShaderStageFlagBits::eVertex, 0);
		cmd.bind_vertex_buffers(
			0,
			{ ovk::RenderCommand::BufferDescription{ std::ref(chunk->picker_mesh->vertex), 0}}
		);
		cmd.draw(chunk->mesh->vertices_count, 1, 0, 0);
	}
	
}

// =======================================================================================================================
// Mesh Renderer


MeshRenderer::MeshRenderer(MasterRenderer *p) : parent(p) {
	create_const_objects();
	create_dynamic_objects();
}

void MeshRenderer::post_init(ovk::Buffer* mb) {
	materials_buffer = mb;
	for (auto i = 0; i < parent->swapchain->image_count; i++) {
	 	dynamic.descriptor_sets[i].write(*materials_buffer, 0, 2, true);
	}
}

void MeshRenderer::draw(Model *m, Transform transform) {
	render_jobs.push_back(Entity { m, transform });
}

void MeshRenderer::on_shadow_render(int index, ovk::RenderCommand& cmd) {

	cmd.bind_graphics_pipeline(*shadow.pipeline);
	cmd.bind_descriptor_sets(*shadow.pipeline, 0, { parent->shadow.descriptor_sets[index] });

	for (auto& entity : render_jobs) {
		glm::mat4 model_matrix = glm::mat4(1.0f);
		model_matrix = glm::translate(model_matrix, entity.transform.pos);
		model_matrix = glm::rotate(model_matrix, glm::radians(entity.transform.rotation), glm::vec3(0.0f, 1.0f, 0.0f));
		model_matrix = glm::scale(model_matrix, entity.transform.scale);

		cmd.push_constant(model_matrix, *shadow.pipeline, vk::ShaderStageFlagBits::eVertex, 0);

		for (auto& mesh : entity.model->meshes) {
			cmd.bind_vertex_buffers( 0, { ovk::RenderCommand::BufferDescription{ std::ref(mesh->vertex), 0}});
			// cmd.bind_index_buffer(mesh->index, 0, vk::IndexType::eUint16);
			cmd.draw(mesh->vertices_count, 1, 0, 0);
		}

	}

}

void MeshRenderer::on_inline_render(int index, ovk::RenderCommand &cmd) {

	if (render_jobs.empty()) return;
	
	// Bind Pipeline
	cmd.bind_graphics_pipeline(*dynamic.pipeline);
	// Terrain Rendering
	// cmd.annotate("Render Meshes!", glm::vec4(0.75f, 0.17f, 0.57f, 1.00f));
	for (auto& entity : render_jobs) {

		// dynamic.descriptor_sets[index].write<Material>(model.mesh->material, 0, 2);

		glm::mat4 model_matrix = glm::mat4(1.0f);
		model_matrix = glm::translate(model_matrix, entity.transform.pos);
		model_matrix = glm::rotate(model_matrix, glm::radians(entity.transform.rotation), glm::vec3(0.0f, 1.0f, 0.0f));
		model_matrix = glm::scale(model_matrix, entity.transform.scale);

		cmd.push_constant(model_matrix, *dynamic.pipeline, vk::ShaderStageFlagBits::eVertex, 0);
		// cmd.push_constant(model.transform.scale, *dynamic.pipeline, vk::ShaderStageFlagBits::eVertex, sizeof(glm::vec4));

		for (auto& mesh : entity.model->meshes) {

			// TODO: This will need to be a copy
			// cmd.copy(*mesh->material, mesh->dynamic_offset, dynamic.material_buffers[index], 0, sizeof(Material));
			cmd.bind_descriptor_sets(*dynamic.pipeline, 0, { dynamic.descriptor_sets[index] }, { mesh->dynamic_offset });
			
			cmd.bind_vertex_buffers( 0, { ovk::RenderCommand::BufferDescription{ std::ref(mesh->vertex), 0}});
			// cmd.bind_index_buffer(mesh->index, 0, vk::IndexType::eUint16);
			cmd.draw(mesh->vertices_count, 1, 0, 0);
		}
	}

	render_jobs.clear();
	
}

void MeshRenderer::recreate() {
	create_dynamic_objects();
}

void MeshRenderer::create_const_objects() {
	descriptor_template = ovk::make_unique(parent->device->build_descriptor_template()
		.add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
		.add_uniform_buffer(1, vk::ShaderStageFlagBits::eFragment)
		.add_dynamic_uniform_buffer(2, vk::ShaderStageFlagBits::eFragment)
		.build());

	shadow.pipeline = ovk::make_unique(
		parent->device->build_pipeline()
		.set_render_pass(*parent->shadow.renderpass, 0)
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex, "res/shader/shadow.vert.spv")
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment, "res/shader/shadow.frag.spv")
		.set_vertex_layout<MeshVertex>()
		.add_descriptor_set_layouts({ parent->shadow.descriptor_template->handle.get() })
		.set_rasterizer(
			vk::PipelineRasterizationStateCreateInfo(
				{},
				false,
				false,
				vk::PolygonMode::eFill,
				// TODO: needs to be redone
				vk::CullModeFlagBits::eBack,
				vk::FrontFace::eCounterClockwise,
				false, 0, 0, 0,
				1.0f)
		)
		.set_depth_stencil()
		.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4))
		// TODO: Dynamic State (duh.)
		.add_viewport(glm::vec2(0, 0), glm::vec2(shadow_extent.width, shadow_extent.height), 0.0f, 1.0f)
		.add_scissor(vk::Offset2D(0, 0), shadow_extent)
		.build()
	);

}

void MeshRenderer::create_dynamic_objects() {

	auto& swapchain = parent->swapchain;
	
	dynamic.pipeline = ovk::make_unique(
		parent->device->build_pipeline()
		.set_render_pass(*parent->render_pass, 0)
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eVertex, "res/shader/mesh.vert.spv")
		.add_shader_stage_from_file(vk::ShaderStageFlagBits::eFragment, "res/shader/mesh.frag.spv")
		.set_vertex_layout<MeshVertex>()
		.add_descriptor_set_layouts({ descriptor_template->handle.get() })
		.set_rasterizer(
			vk::PipelineRasterizationStateCreateInfo(
				{},
				false,
				false,
				vk::PolygonMode::eFill,
				// TODO: needs to be redone
				vk::CullModeFlagBits::eNone,
				vk::FrontFace::eCounterClockwise,
				false, 0, 0, 0,
				1.0f)
		)
		.set_depth_stencil()
		.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4))
		// TODO: Dynamic State (duh.)
		.add_viewport(glm::vec2(0, 0), glm::vec2(swapchain->swap_extent.width, swapchain->swap_extent.height), 0.0f, 1.0f)
		.add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
		.build()
	);

	// dynamic.material_buffers.clear();
	// Material m;
	// for (auto i = 0; i < swapchain->image_count; i++) {
	// 	dynamic.material_buffers.push_back(parent->device->create_uniform_buffer(m, ovk::mem::MemoryType::device_local, { ovk::QueueType::graphics }));
	// }
	
	dynamic.descriptor_pool = ovk::make_unique(parent->device->create_descriptor_pool({ descriptor_template.get() }, { swapchain->image_count }));

	dynamic.descriptor_sets = parent->device->make_descriptor_sets(*dynamic.descriptor_pool, swapchain->image_count, *descriptor_template);
	
	for (auto i = 0; i < swapchain->image_count; i++) {
		dynamic.descriptor_sets[i].write(parent->dynamic.camera_uniform_buffers[i], 0, 0);
		dynamic.descriptor_sets[i].write(parent->dynamic.light_uniform_buffers[i], 0, 1);
		if (materials_buffer) dynamic.descriptor_sets[i].write(*materials_buffer, 0, 2, true);
	}
	
}
