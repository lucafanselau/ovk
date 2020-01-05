#include "pch.h"
#include "gui_renderer.h"

#include "base/device.h"

#include "base/surface.h"

#include <imgui.h>

namespace ovk {

	struct ImGuiVertex {
		glm::vec2 pos;
		glm::vec2 tex;
		glm::u8vec4 color;
	};

	struct ImGuiPushConstant {
		glm::vec2 scale;
		glm::vec2 translate;
	};
	
	// Vertex Shader Code (compiled via compile_shaders.bat from imgui.vert)
	static uint32_t __glsl_shader_vert_spv[] = {
		0x07230203,0x00010000,0x00080007,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
		0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
		0x000b000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x0000000f,
		0x00000011,0x00000018,0x0000001b,0x00030003,0x00000002,0x000001c2,0x00090004,0x415f4c47,
		0x735f4252,0x72617065,0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00040005,
		0x00000004,0x6e69616d,0x00000000,0x00050005,0x00000009,0x73736170,0x6c6f635f,0x0000726f,
		0x00050005,0x0000000b,0x635f6e69,0x726f6c6f,0x00000000,0x00050005,0x0000000f,0x73736170,
		0x7865745f,0x00000000,0x00040005,0x00000011,0x745f6e69,0x00007865,0x00060005,0x00000016,
		0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000016,0x00000000,0x505f6c67,
		0x7469736f,0x006e6f69,0x00070006,0x00000016,0x00000001,0x505f6c67,0x746e696f,0x657a6953,
		0x00000000,0x00070006,0x00000016,0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,
		0x00070006,0x00000016,0x00000003,0x435f6c67,0x446c6c75,0x61747369,0x0065636e,0x00030005,
		0x00000018,0x00000000,0x00040005,0x0000001b,0x705f6e69,0x0000736f,0x00060005,0x0000001d,
		0x68737550,0x736e6f43,0x746e6174,0x00000000,0x00050006,0x0000001d,0x00000000,0x6c616373,
		0x00000065,0x00060006,0x0000001d,0x00000001,0x6e617274,0x74616c73,0x00000065,0x00030005,
		0x0000001f,0x00006370,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000b,
		0x0000001e,0x00000002,0x00040047,0x0000000f,0x0000001e,0x00000001,0x00040047,0x00000011,
		0x0000001e,0x00000001,0x00050048,0x00000016,0x00000000,0x0000000b,0x00000000,0x00050048,
		0x00000016,0x00000001,0x0000000b,0x00000001,0x00050048,0x00000016,0x00000002,0x0000000b,
		0x00000003,0x00050048,0x00000016,0x00000003,0x0000000b,0x00000004,0x00030047,0x00000016,
		0x00000002,0x00040047,0x0000001b,0x0000001e,0x00000000,0x00050048,0x0000001d,0x00000000,
		0x00000023,0x00000000,0x00050048,0x0000001d,0x00000001,0x00000023,0x00000008,0x00030047,
		0x0000001d,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
		0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,
		0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,0x0000000a,
		0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00040017,0x0000000d,
		0x00000006,0x00000002,0x00040020,0x0000000e,0x00000003,0x0000000d,0x0004003b,0x0000000e,
		0x0000000f,0x00000003,0x00040020,0x00000010,0x00000001,0x0000000d,0x0004003b,0x00000010,
		0x00000011,0x00000001,0x00040015,0x00000013,0x00000020,0x00000000,0x0004002b,0x00000013,
		0x00000014,0x00000001,0x0004001c,0x00000015,0x00000006,0x00000014,0x0006001e,0x00000016,
		0x00000007,0x00000006,0x00000015,0x00000015,0x00040020,0x00000017,0x00000003,0x00000016,
		0x0004003b,0x00000017,0x00000018,0x00000003,0x00040015,0x00000019,0x00000020,0x00000001,
		0x0004002b,0x00000019,0x0000001a,0x00000000,0x0004003b,0x00000010,0x0000001b,0x00000001,
		0x0004001e,0x0000001d,0x0000000d,0x0000000d,0x00040020,0x0000001e,0x00000009,0x0000001d,
		0x0004003b,0x0000001e,0x0000001f,0x00000009,0x00040020,0x00000020,0x00000009,0x0000000d,
		0x0004002b,0x00000019,0x00000024,0x00000001,0x0004002b,0x00000006,0x00000028,0x00000000,
		0x0004002b,0x00000006,0x00000029,0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,
		0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,0x0000000c,0x0000000b,0x0003003e,
		0x00000009,0x0000000c,0x0004003d,0x0000000d,0x00000012,0x00000011,0x0003003e,0x0000000f,
		0x00000012,0x0004003d,0x0000000d,0x0000001c,0x0000001b,0x00050041,0x00000020,0x00000021,
		0x0000001f,0x0000001a,0x0004003d,0x0000000d,0x00000022,0x00000021,0x00050085,0x0000000d,
		0x00000023,0x0000001c,0x00000022,0x00050041,0x00000020,0x00000025,0x0000001f,0x00000024,
		0x0004003d,0x0000000d,0x00000026,0x00000025,0x00050081,0x0000000d,0x00000027,0x00000023,
		0x00000026,0x00050051,0x00000006,0x0000002a,0x00000027,0x00000000,0x00050051,0x00000006,
		0x0000002b,0x00000027,0x00000001,0x00070050,0x00000007,0x0000002c,0x0000002a,0x0000002b,
		0x00000028,0x00000029,0x00050041,0x00000008,0x0000002d,0x00000018,0x0000001a,0x0003003e,
		0x0000002d,0x0000002c,0x000100fd,0x00010038
	};

	// Fragment Shader Code (compiled via compile_shaders.bat from imgui.frag)
	static uint32_t __glsl_shader_frag_spv[] = {
		0x07230203,0x00010000,0x00080007,0x00000018,0x00000000,0x00020011,0x00000001,0x0006000b,
		0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
		0x0008000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000b,0x00000014,
		0x00030010,0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00090004,0x415f4c47,
		0x735f4252,0x72617065,0x5f657461,0x64616873,0x6f5f7265,0x63656a62,0x00007374,0x00040005,
		0x00000004,0x6e69616d,0x00000000,0x00040005,0x00000009,0x6f6c6f63,0x00000072,0x00050005,
		0x0000000b,0x73736170,0x6c6f635f,0x0000726f,0x00060005,0x00000010,0x74786574,0x5f657275,
		0x706d6173,0x0072656c,0x00050005,0x00000014,0x73736170,0x7865745f,0x00000000,0x00040047,
		0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000b,0x0000001e,0x00000000,0x00040047,
		0x00000010,0x00000022,0x00000000,0x00040047,0x00000010,0x00000021,0x00000000,0x00040047,
		0x00000014,0x0000001e,0x00000001,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
		0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,
		0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040020,
		0x0000000a,0x00000001,0x00000007,0x0004003b,0x0000000a,0x0000000b,0x00000001,0x00090019,
		0x0000000d,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,
		0x0003001b,0x0000000e,0x0000000d,0x00040020,0x0000000f,0x00000000,0x0000000e,0x0004003b,
		0x0000000f,0x00000010,0x00000000,0x00040017,0x00000012,0x00000006,0x00000002,0x00040020,
		0x00000013,0x00000001,0x00000012,0x0004003b,0x00000013,0x00000014,0x00000001,0x00050036,
		0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x00000007,
		0x0000000c,0x0000000b,0x0004003d,0x0000000e,0x00000011,0x00000010,0x0004003d,0x00000012,
		0x00000015,0x00000014,0x00050057,0x00000007,0x00000016,0x00000011,0x00000015,0x00050085,
		0x00000007,0x00000017,0x0000000c,0x00000016,0x0003003e,0x00000009,0x00000017,0x000100fd,
		0x00010038
	};
	

	ImGuiRenderer::ImGuiRenderer(RenderPass& rp, SwapChain& sc, Surface& surface, Device& d) {

		// ImGui Stuff
		ImGui::CreateContext();
		
		auto& io = ImGui::GetIO();
		io.BackendRendererName = "ovk_imgui_renderer";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

		io.DisplaySize = ImVec2(sc.swap_extent.width, sc.swap_extent.height);

		io.RenderDrawListsFn = nullptr;
		
		// ovk_assert(io.Fonts->AddFontFromFileTTF("../ovk/SourceCodePro-Regular.ttf", 16.0f));
		
		// deprecated: create_device_objects(rp, sc, d);

		create_const_objects(rp, sc, d);
		create_swapchain_objects(sc, rp, d);
		
		context = ImGui::GetCurrentContext();
		window = surface.window.get();

		io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
		io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
		io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
		io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
		io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
		io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
		io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
		io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
		io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
		io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
		io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
		io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
		io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
		io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
		io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
		io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
		io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
		io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
		io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
		io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
		io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
		io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;
		
	}

	void ImGuiRenderer::render() {
		
	}

	void ImGuiRenderer::update() {

		ImGuiIO& io = ImGui::GetIO();

		for (int i = 0; i < mouse_button_just_pressed.size(); i++) {
			io.MouseDown[i] = mouse_button_just_pressed[i] || glfwGetMouseButton(window, i) != 0;
			mouse_button_just_pressed[i] = false;
		}

		const ImVec2 mouse_pos_backup = io.MousePos;
		io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
		
		if (glfwGetWindowAttrib(window, GLFW_FOCUSED) != 0) {
			if (io.WantSetMousePos) {
				glfwSetCursorPos(window, (double)mouse_pos_backup.x, (double)mouse_pos_backup.y);
			} else {
				double mouse_x, mouse_y;
				glfwGetCursorPos(window, &mouse_x, &mouse_y);
				io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
			}
		}
		
	}

	void ImGuiRenderer::recreate(SwapChain& new_swapchain, RenderPass& rp, Device& device) {
		dynamic_objs = {}; // call destructors on all dynamic_objs

		auto& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(new_swapchain.swap_extent.width, new_swapchain.swap_extent.height);
		
		// Recreate Swapchain dependant objects
		create_swapchain_objects(new_swapchain, rp, device);
		
	}

	void ImGuiRenderer::cmd_render_imgui(RenderCommand& cmd, Device &device, int index, ImDrawData *draw_data) {
	
		ovk_assert(index <= dynamic_objs.vertex_buffers.size(), "OutOfRange Index (Maybe the Swaapchain changed?)");

		cmd.begin_region("ImGui Rendering", glm::vec4(0.23f, 0.76f, 0.56f, 1.00f));

		if (!draw_data->TotalVtxCount || !draw_data->TotalIdxCount) return;
		
		{
			// Create new buffers for this frame!
			auto vertex_memory = std::make_unique<std::vector<ImDrawVert>>(draw_data->TotalVtxCount);
			auto indices_memory = std::make_unique<std::vector<ImDrawIdx>>(draw_data->TotalIdxCount);
			auto vtx_offset = 0, idx_offset = 0;
			for (auto i = 0; i < draw_data->CmdListsCount; i++) {
				const ImDrawList* cmd_list = draw_data->CmdLists[i];


				for (auto n = 0; n < cmd_list->VtxBuffer.Size; n++) {
					ImDrawVert* vert = cmd_list->VtxBuffer.Data + n;
					vertex_memory->operator[](vtx_offset + n) = *(vert);
				}
				// memcpy(vertex_memory->data() + vtx_offset, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
				for (auto n = 0; n < cmd_list->IdxBuffer.Size; n++) {
					ImDrawIdx* idx = cmd_list->IdxBuffer.Data + n;
					indices_memory->operator[](idx_offset + n) = *idx;
				}
				// memcpy(indices_memory->data() + idx_offset, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

				vtx_offset += cmd_list->VtxBuffer.Size;
				idx_offset += cmd_list->IdxBuffer.Size;
			}

			// TODO: NOT EVERY FRAME A NEW BUFFER
			dynamic_objs.vertex_buffers[index] = std::make_unique<Buffer>(std::move(device.create_vertex_buffer(*vertex_memory, mem::MemoryType::cpu_coherent_and_cached)));
			dynamic_objs.index_buffers[index] = std::make_unique<Buffer>(std::move(device.create_index_buffer(*indices_memory, mem::MemoryType::cpu_coherent_and_cached)));
		}

		
		cmd.bind_graphics_pipeline(*dynamic_objs.pipeline);
		cmd.bind_descriptor_sets(*dynamic_objs.pipeline, 0, { const_objs.descriptor_set->set });
		ImGuiPushConstant pc{
			glm::vec2(2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y),
			glm::vec2(0.0f)
		};
		pc.translate = glm::vec2(-1.0f - draw_data->DisplayPos.x * pc.scale.x, -1.0f - draw_data->DisplayPos.y * pc.scale.y);
		cmd.push_constant(pc, *dynamic_objs.pipeline, vk::ShaderStageFlagBits::eVertex, 0);

		cmd.bind_vertex_buffers(0, { {std::ref(*dynamic_objs.vertex_buffers[index]), 0} });
		auto idx_type = vk::IndexType::eUint16;
		if constexpr (sizeof(ImDrawVert) == 4) idx_type = vk::IndexType::eUint32;
		cmd.bind_index_buffer(*dynamic_objs.index_buffers[index], 0, idx_type);

		int vtx_offset = 0, idx_offset = 0;
		for (int n = 0; n < draw_data->CmdListsCount; n++) {
			auto cmd_list = draw_data->CmdLists[n];
			for (int i = 0; i < cmd_list->CmdBuffer.Size; i++) {
				const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[i];
				cmd.draw_indexed(pcmd->ElemCount, 1, pcmd->IdxOffset + idx_offset, pcmd->VtxOffset + vtx_offset, 0);
			}
			idx_offset += cmd_list->IdxBuffer.Size;
			vtx_offset += cmd_list->VtxBuffer.Size;
		}
		
		
		cmd.end_region();
	}

	void ImGuiRenderer::create_const_objects(RenderPass &rp, SwapChain &sc, Device &d) {
		const_objs.descriptor_layout = std::make_unique<DescriptorTemplate>(
			std::forward<DescriptorTemplate>(
				d.build_descriptor_template()
				.add_sampler(0, vk::ShaderStageFlagBits::eFragment)
				.build()
				)
			);

		// Create Font Texture
		auto& io = ImGui::GetIO();
		uint8_t* pixels;
		int width, height;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		const_objs.font_atlas = std::make_unique<Image>(std::move(d.create_image_2d(vk::Format::eR8G8B8A8Unorm, pixels, 4, vk::Extent3D(width, height, 1), vk::ImageUsageFlagBits::eSampled)));
		const_objs.font_atlas_view = std::make_unique<ImageView>(std::move(d.view_from_image(*const_objs.font_atlas)));

		io.Fonts->TexID = reinterpret_cast<ImTextureID>(reinterpret_cast<intptr_t>(static_cast<VkImage>(const_objs.font_atlas->handle.get())));

		// Descriptor Sets
		const_objs.des_pool = std::make_unique<DescriptorPool>(d.create_descriptor_pool({ const_objs.descriptor_layout.get() }, { 1 }));
		auto descriptor_sets = d.make_descriptor_sets(*const_objs.des_pool, 1, *const_objs.descriptor_layout);
		ovk_assert(descriptor_sets.size() == 1);
		const_objs.descriptor_set = std::make_unique<DescriptorSet>(std::move(descriptor_sets[0]));

		const_objs.descriptor_set->write(d.get_default_nearest_sampler(), const_objs.font_atlas_view->handle.get(), vk::ImageLayout::eShaderReadOnlyOptimal, 0);

		
	}

	void ImGuiRenderer::create_swapchain_objects(SwapChain& new_swapchain, RenderPass& rp, Device& device) {
		dynamic_objs.pipeline = std::make_unique<GraphicsPipeline>(
			std::forward<GraphicsPipeline>(
				device.build_pipeline()
				.set_render_pass(rp)
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eVertex, __glsl_shader_vert_spv, sizeof(__glsl_shader_vert_spv))
				.add_shader_stage_u32(vk::ShaderStageFlagBits::eFragment, __glsl_shader_frag_spv, sizeof(__glsl_shader_frag_spv))
				.set_vertex_layout({ vk::VertexInputBindingDescription { 0, sizeof(ImDrawVert) } },
					{ vk::VertexInputAttributeDescription { 0, 0, vk::Format::eR32G32Sfloat, offsetof(ImGuiVertex, pos)},
						vk::VertexInputAttributeDescription { 1, 0, vk::Format::eR32G32Sfloat, offsetof(ImGuiVertex, tex)},
						vk::VertexInputAttributeDescription { 2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(ImGuiVertex, color)} }
				)
				.set_depth_stencil(false, false)
				.add_descriptor_set_layouts({ const_objs.descriptor_layout->handle.get() })
				.add_push_constant(vk::ShaderStageFlagBits::eVertex, sizeof(ImGuiPushConstant))
				.add_viewport(glm::vec2(0, 0), glm::vec2(new_swapchain.swap_extent.width, new_swapchain.swap_extent.height), 0.0f, 1.0f)
				.add_scissor(vk::Offset2D(0, 0), new_swapchain.swap_extent)
				.build()
				)
			);

		dynamic_objs.vertex_buffers.resize(new_swapchain.image_count);
		dynamic_objs.index_buffers.resize(new_swapchain.image_count);

	}



	void ImGuiRenderer::on_key(int key_code, int scancode, int action, int mods) {
		ImGuiIO& io = ImGui::GetIO();
		if (action == GLFW_PRESS) io.KeysDown[key_code] = true;
		if (action == GLFW_RELEASE || action == GLFW_REPEAT) io.KeysDown[key_code] = false;

		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
	}

	void ImGuiRenderer::on_char(uint32_t codepoint) {
		auto& io = ImGui::GetIO();
		io.AddInputCharacter(codepoint);
	}

	void ImGuiRenderer::on_cursor(double xpos, double ypos) {
		
	}

	void ImGuiRenderer::on_cursor_enter(bool entered) {
	}

	void ImGuiRenderer::on_mouse_button(int button, int action, int mods) {
		if (action == GLFW_PRESS && button < 5) {
			mouse_button_just_pressed[button] = true;
		}
	}

	void ImGuiRenderer::on_scroll(double xoffset, double yoffset) {
		ImGuiIO& io = ImGui::GetIO();
		io.MouseWheelH += (float)xoffset;
		io.MouseWheel += (float)yoffset;
	}
}
