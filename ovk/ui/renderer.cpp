#include "pch.h"
#include "renderer.h"

#define texture_rect_count 64

namespace ovk::ui {
	
	static uint32_t __glsl_shader_vert_spv[] = {
#include "shader/ui.vert.spv"
	};

	static uint32_t __glsl_shader_textured_frag_spv[] = {
#include "shader/ui_textured.frag.spv"																							
	};
	
	static uint32_t __glsl_shader_colored_frag_spv[] = {
#include "shader/ui_colored.frag.spv"																							
	};
	

		
	struct Vertex {
		glm::vec2 pos;
		glm::vec2 tex_coords;
	};

	struct TexturedFragmentPushConstant {
		glm::vec2 pos;
		glm::vec2 scale;
		float radius;
	};

	struct ColoredFragmentPushConstant {
		glm::vec4 color;
		glm::vec2 pos;
		glm::vec2 scale;
		float radius;
	};

	TexturedRect::TexturedRect(ovk::DescriptorSet &&set, glm::vec2 _pos, glm::vec2 _scale)
	  : descriptor_set(std::move(set)), pos(_pos), scale(_scale) {}
	
	Renderer::Renderer(ovk::RenderPass& rp, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Device>& d)
		: swapchain(sc), device(d), renderpass(&rp) {
		create_const_objects();
		create_dynamic_objects();
	}

	void Renderer::update(float dt) {

	}

	void Renderer::draw_rect(TexturedRect& tr) {
		texture_rects.push_back(&tr);
	}

	void Renderer::draw_rect(glm::vec2 pos, glm::vec2 scale, glm::vec4 color) {
		colored_rects.push_back(ColoredRect { color, pos, scale });
	}

	void Renderer::draw_text(uint32_t x, uint32_t y, std::string text) {
		text_renderer->draw_text(x, y, text);
	}

	
	void Renderer::on_inline_render(ovk::RenderCommand& cmd, uint32_t index) {
		cmd.bind_graphics_pipeline(*pipeline.textured);
		cmd.bind_vertex_buffers(0, { { std::ref(*vertex_buffer), 0 } });
		for (auto* rect : texture_rects) {

			glm::mat4 model(1.0f);

			model = glm::translate(model, glm::vec3(rect->pos.x, rect->pos.y, 0.0f));
			model = glm::scale(model, glm::vec3(rect->scale.x, rect->scale.y, 0.0f));

			cmd.push_constant(model, *pipeline.textured, vk::ShaderStageFlagBits::eVertex, 0);
			TexturedFragmentPushConstant pc{ rect->pos, rect->scale, 10.0f };
			cmd.push_constant(pc, *pipeline.textured, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4));
				
			cmd.bind_descriptor_sets(*pipeline.textured, 0, { rect->descriptor_set.set });
			cmd.draw(6, 1, 0, 0);
		}
		texture_rects.clear();

		cmd.bind_graphics_pipeline(*pipeline.colored);
		// TODO: maybe this does not need to be rebound
		cmd.bind_vertex_buffers(0, { { std::ref(*vertex_buffer), 0 } });
		for (auto& rect : colored_rects) {

			glm::mat4 model(1.0f);

			model = glm::translate(model, glm::vec3(rect.pos.x, rect.pos.y, 0.0f));
			model = glm::scale(model, glm::vec3(rect.scale.x, rect.scale.y, 0.0f));

			cmd.push_constant(model, *pipeline.colored, vk::ShaderStageFlagBits::eVertex, 0);
			ColoredFragmentPushConstant pc{ rect.color, rect.pos, rect.scale, 10.0f };
			cmd.push_constant(pc, *pipeline.colored, vk::ShaderStageFlagBits::eFragment, sizeof(glm::mat4));
				
			// TODO: NOOOO
			cmd.bind_descriptor_sets(*pipeline.colored, 0, { des_set_colored->set });
			cmd.draw(6, 1, 0, 0);
		}
		colored_rects.clear();

		


		// Draw Texts
		text_renderer->on_inline_render(cmd, index);
	}

	void Renderer::recreate_swapchain() {
		create_dynamic_objects();
		text_renderer->recreate_swapchain();
	}

	TexturedRect Renderer::create_textured_rect(ovk::Image& image, ovk::ImageView& view, ovk::Sampler& sampler, glm::vec2 pos, glm::vec2 scale) {

		auto sets = device->make_descriptor_sets(*descriptor_pool, 1, *descriptor_template);
		ovk_assert(sets.size() == 1);

		auto set = std::move(sets[0]);

		set.write(*projection_uniform_buffer, 0, 0);
		set.write(sampler.handle.get(), view.handle.get(), image.layout, 1);
		
		return TexturedRect(std::move(set), pos, scale);

	}

	void Renderer::create_const_objects() {

		text_renderer = std::make_unique<TextRenderer>(*renderpass, swapchain, device);

		const auto a = 0.5f;
		
		std::vector<Vertex> vertices {
			Vertex { glm::vec2(-a, -a), glm::vec2(0.0f, 0.0f) },
			Vertex { glm::vec2( a, -a), glm::vec2(1.0f, 0.0f) },
			Vertex { glm::vec2( a,  a), glm::vec2(1.0f, 1.0f) },
			
			Vertex { glm::vec2(-a, -a), glm::vec2(0.0f, 0.0f) },
			Vertex { glm::vec2( a,  a), glm::vec2(1.0f, 1.0f) },
			Vertex { glm::vec2(-a,  a), glm::vec2(0.0f, 1.0f) },
		};
		
		vertex_buffer = ovk::make_unique(device->create_vertex_buffer(
			vertices,
			ovk::mem::MemoryType::device_local
    ));

		descriptor_template = ovk::make_unique(device->build_descriptor_template()
																					 .add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
																					 .add_sampler(1, vk::ShaderStageFlagBits::eFragment)
																					 .build());

		descriptor_pool = ovk::make_unique(device->create_descriptor_pool(
      { descriptor_template.get() },
			{ texture_rect_count }
    ));

		des_template_colored = ovk::make_unique(device->build_descriptor_template()
																						.add_uniform_buffer(0, vk::ShaderStageFlagBits::eVertex)
																						.build());

		des_pool_colored = ovk::make_unique(device->create_descriptor_pool(
																					{ des_template_colored.get() },
																					{ 1 } ));

		auto sets = device->make_descriptor_sets(*des_pool_colored, 1, *des_template_colored);
		ovk_assert(sets.size() == 1);

		des_set_colored = ovk::make_unique(std::move(sets[0]));

	}

	void Renderer::create_dynamic_objects() {

		auto &se = swapchain->swap_extent;
		auto projection_matrix = glm::ortho(0.0f, (float) se.width, 0.0f, (float) se.height, -1.0f, 1.0f);
		
		projection_uniform_buffer = ovk::make_unique(
			device->create_uniform_buffer(
				projection_matrix,
				ovk::mem::MemoryType::device_local,
				{ ovk::QueueType::graphics } ));

		des_set_colored->write(*projection_uniform_buffer, 0, 0);
		
		pipeline.textured = std::make_unique<GraphicsPipeline>(
			std::forward<GraphicsPipeline>(
				device->build_pipeline()
				.set_render_pass(*renderpass)
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eVertex, __glsl_shader_vert_spv, sizeof(__glsl_shader_vert_spv))
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eFragment, __glsl_shader_textured_frag_spv, sizeof(__glsl_shader_textured_frag_spv))
				.set_vertex_layout<Vertex>()
				.set_depth_stencil(false, false)
				.add_descriptor_set_layouts({ descriptor_template->handle.get() })
				.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4))
				.add_push_constant(vk::ShaderStageFlagBits::eFragment, sizeof(ColoredFragmentPushConstant), /*offset:*/ sizeof(glm::mat4))
				.add_viewport(glm::vec2(0, 0), glm::vec2(swapchain->swap_extent.width, swapchain->swap_extent.height), 0.0f, 1.0f)
				.add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
				.build()
			)
		);

		pipeline.colored = std::make_unique<GraphicsPipeline>(
			std::forward<GraphicsPipeline>(
				device->build_pipeline()
				.set_render_pass(*renderpass)
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eVertex, __glsl_shader_vert_spv, sizeof(__glsl_shader_vert_spv))
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eFragment, __glsl_shader_colored_frag_spv, sizeof(__glsl_shader_colored_frag_spv))
				.set_vertex_layout<Vertex>()
				.set_depth_stencil(false, false)
				.add_descriptor_set_layouts({ des_template_colored->handle.get() })
				.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(glm::mat4))
				.add_push_constant(vk::ShaderStageFlagBits::eFragment, sizeof(ColoredFragmentPushConstant), /*offset:*/ sizeof(glm::mat4))
				.add_viewport(glm::vec2(0, 0), glm::vec2(swapchain->swap_extent.width, swapchain->swap_extent.height), 0.0f, 1.0f)
				.add_scissor(vk::Offset2D(0, 0), swapchain->swap_extent)
				.build()
			)
		);

	}

	
}
