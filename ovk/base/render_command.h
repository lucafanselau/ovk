#pragma once

#include "handle.h"
#include "buffer.h"
#include <imgui.h>
#include <variant>

namespace ovk {
	class ImGuiRenderer;
	class GraphicsPipeline;
	class Framebuffer;
	class RenderPass;
	class SwapChain;

	class Device;

	class OVK_API RenderCommand {
	public:

	vk::CommandBuffer cmd_handle;

	void begin_render_pass(const RenderPass& rp, const Framebuffer& fb, vk::Extent2D render_area, std::vector<std::variant<glm::vec4, glm::vec2>> clear_colors, vk::SubpassContents subass_behavior) const;

	void bind_graphics_pipeline(const ovk::GraphicsPipeline& pipeline) const;

	struct BufferDescription {
		std::reference_wrapper<ovk::Buffer> buffer;
		vk::DeviceSize offset;
	};
	void bind_vertex_buffers(uint32_t first_binding, std::vector<BufferDescription> descriptions) const;
	void bind_index_buffer(const Buffer& buffer, vk::DeviceSize offset, vk::IndexType type) const;

	void bind_descriptor_sets(GraphicsPipeline& pipe, uint32_t first_set, std::vector<vk::DescriptorSet> sets, std::vector<uint32_t> dynamic_offsets = {}) const;

	template<typename T>
	void push_constant(T& data, GraphicsPipeline &pipeline, vk::ShaderStageFlags stage, uint32_t offset = 0);

	void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance) const;
	void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const;

	void draw_imgui(ImGuiRenderer& renderer, int index, ImDrawData *draw_data);
		
	void end_render_pass() const;

	void annotate(const std::string& annotation, glm::vec4 color);
	void begin_region(const std::string& name, glm::vec4 color);
	void end_region();

	void copy(ovk::Buffer& src, uint32_t src_offset, ovk::Buffer& dst, uint32_t dst_offset, uint32_t size);
	
	OVK_CONVERSION operator vk::CommandBuffer() const;
		
	private:
	friend Device;
	RenderCommand(vk::CommandBuffer raw_handle, Device &d);

	void start() const;
	void end() const;

	void push_constant_impl(GraphicsPipeline& pipeline, vk::ShaderStageFlags stage, uint32_t offset, uint32_t size, void* data) const;
		
	// Only valid during recording through device
	Device* device;
	};

	template <typename T>
	void RenderCommand::push_constant(T &data, GraphicsPipeline &pipeline, vk::ShaderStageFlags stage, uint32_t offset) {
		push_constant_impl(pipeline, stage, offset, sizeof(T), &data);
	}

}
