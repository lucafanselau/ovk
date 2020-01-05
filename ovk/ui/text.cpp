#include "pch.h"
#include "text.h"

namespace ovk::ui {

  struct VertexLayout {
    glm::vec3 pos;
    glm::vec2 tex_coord;
  };

  struct Projection {
    glm::mat4 proj;
  };

  static uint32_t __glsl_shader_vert_spv[] = {
#include "shader/text.vert.spv"
  };

  static uint32_t __glsl_shader_frag_spv[] = {
#include "shader/text.frag.spv"																							
	};


	TextRenderer::TextRenderer(ovk::RenderPass& rp, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Device>& d)
	  : device(d), swapchain(sc), renderpass(&rp) {
    create_const_objects();
		create_dynamic_objects();
	}

	void TextRenderer::recreate_swapchain() {
		create_dynamic_objects();
	}

	void TextRenderer::draw_text(uint32_t x, uint32_t y, std::string content) {
		texts.push_back(TextDrawable { content, x, y });
	}

	
	void TextRenderer::on_inline_render(ovk::RenderCommand& cmd, uint32_t index) {

		// TODO for screen capture
		return;

		std::vector<VertexLayout> vertices;

		// vertices.push_back(VertexLayout { glm::vec3( 100.f, 100.f, 0.0f), glm::vec2(0.0f) });
		// vertices.push_back(VertexLayout { glm::vec3( 150.f,	200.f, 0.0f), glm::vec2(0.0f) });
		// vertices.push_back(VertexLayout { glm::vec3( 200.f, 100.f, 0.0f), glm::vec2(0.0f) });

		// TODO: Hardcode
		unsigned char range = 128;
		uint32_t mem_width = 45, mem_height = 60;

		for (auto& text : texts) {

			float x = (float) text.x;
			float y = (float) text.y;
			float scale = 0.5f;
			
			for (char c : text.content) {
				// Get the Character
				if (auto it = chars.find(c); it != chars.end()) {
					auto &character = it->second;

					float xpos = x + character.bearing.x * scale;
					float ypos = y + (character.size.y - character.bearing.y);

					float w = character.size.x * scale;
					float h = character.size.y * scale;

					float base_tex = (1.0f / (float) range) * (float) c;
					float high_tex = base_tex + (float) character.size.x / (float) (mem_width * range);

					float vertical_tex = (float) character.size.y / (float) mem_height;

					vertices.push_back(VertexLayout { glm::vec3( xpos, ypos - h, 0.0f), glm::vec2(base_tex, 0.0f) });
					vertices.push_back(VertexLayout { glm::vec3( xpos, ypos	   , 0.0f), glm::vec2(base_tex, vertical_tex) });
					vertices.push_back(VertexLayout { glm::vec3( xpos + w, ypos, 0.0f), glm::vec2(high_tex, vertical_tex) });

					vertices.push_back(VertexLayout { glm::vec3( xpos, ypos - h, 0.0f), glm::vec2(base_tex, 0.0f) });
					vertices.push_back(VertexLayout { glm::vec3( xpos + w, ypos, 0.0f), glm::vec2(high_tex, vertical_tex) });
					vertices.push_back(VertexLayout { glm::vec3( xpos + w, ypos - h, 0.0f), glm::vec2(high_tex, 0.0f) });

					x += (character.advance >> 6) * scale;
				}
			}

		}

		texts.clear();

		auto &vertex_buffer = vertex_buffers[index];
		vertex_buffer = ovk::make_unique(device->create_vertex_buffer(vertices, ovk::mem::MemoryType::cpu_coherent_and_cached));
		
		cmd.bind_graphics_pipeline(*pipeline);

		cmd.bind_descriptor_sets(*pipeline, 0, { descriptor_set->set });
		cmd.bind_vertex_buffers(0, { { std::ref(*vertex_buffer), 0 } });

		cmd.draw((uint32_t) vertices.size(), 1, 0, 0);
		
	}

	void TextRenderer::create_const_objects() {

		if (FT_Init_FreeType(&freetype_lib))
			panic("failed to init freetype");


		if (FT_New_Face(freetype_lib, "C:\\windows\\fonts\\sourcecodepro-bold.ttf", 0, &default_font))
			panic("failed to load default font");

		FT_Set_Pixel_Sizes(default_font, 0, 48);

		// Create Image
		unsigned char range = 128;
		
		font_image = ovk::make_unique(
			device->create_image(
				vk::ImageType::e2D,
				vk::Format::eR8Unorm,
				// TODO: This part will break for diffrent font sizes
				// We expect the glyphs to be no larger than 30px in width
				vk::Extent3D(45 * (uint32_t) range, 60, 1),
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				vk::ImageTiling::eOptimal,
				ovk::mem::MemoryType::device_local));

		font_image_view = ovk::make_unique(
			device->view_from_image(
				*font_image, vk::ImageAspectFlagBits::eColor, "zzzr"
			));
		
		// Lets load some characters
		glm::ivec2 max_extent(0);

		auto cmd = device->create_single_submit_cmd(ovk::QueueType::transfer);

		font_image->transition_layout(cmd, vk::ImageLayout::eTransferDstOptimal, ovk::QueueType::transfer, *device);

		std::vector<ovk::Buffer> bitmaps;
		bitmaps.reserve((uint64_t) range);

		uint32_t marker = 0;
		
		for (unsigned char c = 0; c < range; c++) {

			if (FT_Load_Char(default_font, c, FT_LOAD_RENDER)) {
				panic("failed to load char");
				continue;
			}

			Character character {
				glm::ivec2(default_font->glyph->bitmap.width, default_font->glyph->bitmap.rows),
				glm::ivec2(default_font->glyph->bitmap_left, default_font->glyph->bitmap_top),
				static_cast<uint32_t>(default_font->glyph->advance.x)
			};

			if (character.size.x > max_extent.x) max_extent.x = character.size.x;
			if (character.size.y > max_extent.y) max_extent.y = character.size.y;

			// Not quite sure if this is actually enforced by the lib
			ovk_assert(character.size.y <= 48);

			if (character.size.x > 0 && character.size.y > 0) {
			
				bitmaps.push_back(
					std::move(
						device->create_staging_buffer(
							default_font->glyph->bitmap.buffer,
							character.size.x * character.size.y
						)
					)
				);

				vk::BufferImageCopy copy_region {
					0,
					(uint32_t) character.size.x,
					(uint32_t) character.size.y,
					vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 },
					vk::Offset3D(marker, 0, 0),
					vk::Extent3D(character.size.x, character.size.y, 1)
				};
			
				cmd.copyBufferToImage(
					bitmaps[bitmaps.size() - 1],
					*font_image,
					vk::ImageLayout::eTransferDstOptimal,
					{ copy_region }
				);
				
			}
			
			chars.insert(std::make_pair(c, character));

			marker += 45;
		}

		spdlog::info("Max Extent: [{}, {}]", max_extent.x, max_extent.y);

		font_image->transition_layout(cmd, vk::ImageLayout::eShaderReadOnlyOptimal, ovk::QueueType::transfer, *device);
		
		device->flush(cmd, ovk::QueueType::transfer, true, true);

		spdlog::info("finished font image: {}", (uint64_t) (VkImage) (font_image->handle.get()));
		
		descriptor_template = ovk::make_unique(device->build_descriptor_template()																					
			.add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
																					 .add_sampler(1, vk::ShaderStageFlagBits::eFragment)
		  .build());



																									 
																									 
		
	}

	void TextRenderer::create_dynamic_objects() {

		
		pipeline = std::make_unique<GraphicsPipeline>(
			std::forward<GraphicsPipeline>(
				device->build_pipeline()
				.set_render_pass(*renderpass)
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eVertex, __glsl_shader_vert_spv, sizeof(__glsl_shader_vert_spv))
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eFragment, __glsl_shader_frag_spv, sizeof(__glsl_shader_frag_spv))
				.set_vertex_layout<VertexLayout>()
				.set_depth_stencil(false, false)
				.add_descriptor_set_layouts({ descriptor_template->handle.get() })
				// .add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4))
				// .add_push_constant(vk::ShaderStageFlagBits::eFragment, sizeof(FragmentPushConstant), /*offset:*/ sizeof(glm::mat4))
				.add_viewport(glm::vec2(0, 0), glm::vec2(swapchain->swap_extent.width, swapchain->swap_extent.height), 0.0f, 1.0f)
				.add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
				.build()
			)
		);

		vertex_buffers.resize(swapchain->image_count);

		auto &extent = swapchain->swap_extent;
		Projection p { glm::ortho(0.0f, (float) extent.width, 0.0f, (float) extent.height, -1.0f, 1.0f) };
		
		projection_uniform_buffer = ovk::make_unique(device->create_uniform_buffer(
																									 p,
																									 ovk::mem::MemoryType::device_local,
																									 { ovk::QueueType::graphics }
																								 ));


		descriptor_pool = ovk::make_unique(device->create_descriptor_pool(
																				 { descriptor_template.get() },
																				 { 1 } ));
		
		auto sets = device->make_descriptor_sets(*descriptor_pool, 1, *descriptor_template);
		ovk_assert(sets.size() == 1);

		descriptor_set = ovk::make_unique(std::move(sets[0]));

		descriptor_set->write(*projection_uniform_buffer, 0, 0);
		descriptor_set->write(device->get_default_linear_sampler(), *font_image_view, vk::ImageLayout::eShaderReadOnlyOptimal, 1);
		
	}
	
}
