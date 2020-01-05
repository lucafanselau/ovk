#pragma once

#include "handle.h"
#include "base/device.h"

#include <ft2build.h>
#include FT_FREETYPE_H  

namespace ovk::ui {

	struct OVK_API Character {
    glm::ivec2 size;       // Size of glyph
    glm::ivec2 bearing;    // Offset from baseline to left/top of glyph
    uint32_t advance;    // Offset to advance to next glyph
	};

	struct OVK_API TextDrawable {
    std::string content;
		uint32_t x, y;
		// ref Font? 
	};

	
	struct OVK_API TextRenderer {
  public:
		TextRenderer(ovk::RenderPass& rp, std::shared_ptr<ovk::SwapChain>& sc, std::shared_ptr<ovk::Device>& d);

		void recreate_swapchain();

		void draw_text(uint32_t x, uint32_t y, std::string content);
		
		void on_inline_render(ovk::RenderCommand& cmd, uint32_t index);
		
	private:

		void create_const_objects();
		void create_dynamic_objects();

		std::vector<TextDrawable> texts;
		
		std::shared_ptr<ovk::Device> device;
		std::shared_ptr<ovk::SwapChain> swapchain;
		ovk::RenderPass *renderpass;

		// TODO: Library -> Font Manager, Face and chars Font
		std::unordered_map<char, Character> chars;
		FT_Library freetype_lib;
		FT_Face default_font;

		std::unique_ptr<ovk::Image> font_image;
		std::unique_ptr<ovk::ImageView> font_image_view;

		// Dynamic Thingys
		std::unique_ptr<ovk::GraphicsPipeline> pipeline;
		std::vector<std::unique_ptr<ovk::Buffer>> vertex_buffers;

		std::unique_ptr<ovk::Buffer> projection_uniform_buffer;

		std::unique_ptr<ovk::DescriptorTemplate> descriptor_template;
		std::unique_ptr<ovk::DescriptorPool> descriptor_pool;

		std::unique_ptr<ovk::DescriptorSet> descriptor_set;


	};
	
}
