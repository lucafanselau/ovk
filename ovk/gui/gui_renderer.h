#pragma once

#include "def.h"
#include "base/pipeline.h"
#include "base/descriptor.h"
#include "base/image.h"
#include "app/event.h"
#include <imgui.h>

struct ImGuiContext;

namespace ovk {
	class SwapChain;

	class OVK_API ImGuiRenderer : public EventListener {
	public:

		ImGuiRenderer(RenderPass& rp, SwapChain& sc, Surface& surface, Device& device);

		// STANDALONE RENDERER: May be implemented down the road
		[[deprecated]]
		void render();

		void update();

		void recreate(SwapChain& new_swapchain, RenderPass& rp, Device& device);

		ImGuiContext* context;
		
	private:

		friend class RenderCommand;
		void cmd_render_imgui(RenderCommand& cmd, Device& device, int index, ImDrawData *draw_data);

		// void create_device_objects(RenderPass& rp, SwapChain& sc, Device& d);

		void create_const_objects(RenderPass& rp, SwapChain& sc, Device& d);
		void create_swapchain_objects(SwapChain& new_swapchain, RenderPass& rp, Device& device);
		
		GLFWwindow* window;
		
	public:
		~ImGuiRenderer() override = default;
	private:
		void on_key(int key_code, int scancode, int action, int mods) override;
		void on_char(uint32_t codepoint) override;
		void on_cursor(double xpos, double ypos) override;
		void on_cursor_enter(bool entered) override;
		void on_mouse_button(int button, int action, int mods) override;
		void on_scroll(double xoffset, double yoffset) override;
		
		std::array<bool, 5> mouse_button_just_pressed;
		struct {
			std::unique_ptr<DescriptorTemplate> descriptor_layout;
			std::unique_ptr<DescriptorPool> des_pool;
			std::unique_ptr<DescriptorSet> descriptor_set;
			std::unique_ptr<Image> font_atlas;
			std::unique_ptr<ImageView> font_atlas_view;
		} const_objs;

		struct {
			std::unique_ptr<GraphicsPipeline> pipeline;
			
			std::vector<std::unique_ptr<Buffer>> vertex_buffers;
			std::vector<std::unique_ptr<Buffer>> index_buffers;
		} dynamic_objs;
		
		
		
	};
	
}
