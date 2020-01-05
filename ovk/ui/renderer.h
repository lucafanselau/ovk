#pragma once

#include "handle.h"

#include "base/device.h"

#include "text.h"

// namespace ovk {
// 	class SwapChain;
// 	class Device;
// 	class Semaphore;
// }

namespace ovk::ui {

	struct OVK_API TexturedRect {

		TexturedRect(ovk::DescriptorSet&& set, glm::vec2 _pos, glm::vec2 _scale);
		
		ovk::DescriptorSet descriptor_set;
		glm::vec2 pos, scale;
		
	};

	struct OVK_API ColoredRect {
    glm::vec4 color;
		glm::vec2 pos, scale;
	};
	
	// This will become a whole renderer (so has its own Sync Properties and submits own CommandBuffer)
	class OVK_API Renderer {
	public:

	  Renderer(ovk::RenderPass& rp, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Device>& d);

		void update(float dt);

		void draw_rect(TexturedRect& tr);
		void draw_rect(glm::vec2 pos, glm::vec2 scale, glm::vec4 color);

		void draw_text(uint32_t x, uint32_t y, std::string text);
		
		void on_inline_render(ovk::RenderCommand& cmd, uint32_t index);

		void recreate_swapchain();

		// TODO: This should not be something the renderer should do
		TexturedRect create_textured_rect(ovk::Image& image, ovk::ImageView& view, ovk::Sampler& sampler, glm::vec2 pos, glm::vec2 scale);
		
	private:
		void create_const_objects();
		void create_dynamic_objects();
		
		std::shared_ptr<ovk::Device> device;
		std::shared_ptr<ovk::SwapChain> swapchain;
		ovk::RenderPass *renderpass;

		std::unique_ptr<TextRenderer> text_renderer;

		std::unique_ptr<ovk::DescriptorTemplate> descriptor_template, des_template_colored;
		std::unique_ptr<ovk::DescriptorPool> descriptor_pool, des_pool_colored;
		std::unique_ptr<ovk::DescriptorSet> des_set_colored;
		
		std::vector<TexturedRect*> texture_rects;
		std::vector<ColoredRect> colored_rects;
		
		std::unique_ptr<ovk::Buffer> vertex_buffer;

		// Contains [0, 0] to screen_extent orthographic-projection matrix
		std::unique_ptr<ovk::Buffer> projection_uniform_buffer;
		struct {
			std::unique_ptr<ovk::GraphicsPipeline> textured, colored;
		} pipeline;
		
	};

	

}

