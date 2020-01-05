#include "pch.h"
#include "render_command.h"

#include "gui/gui_renderer.h"

#include "device.h"
#include <map>
#include <variant>

namespace ovk {



	void RenderCommand::begin_render_pass(const RenderPass &rp, const Framebuffer &fb, vk::Extent2D render_area, std::vector<std::variant<glm::vec4, glm::vec2>> clear_colors,
		vk::SubpassContents subass_behavior) const {
		std::vector<vk::ClearValue> clear_values;
		for (auto&& variant : clear_colors) {
			vk::ClearValue clear_value;
			const auto index = variant.index();
			if (index == 0) {
				const auto clear_color = std::get<0>(variant);
				clear_value.setColor(vk::ClearColorValue(std::array<float, 4>({ clear_color.r, clear_color.g, clear_color.b, clear_color.a })));
			} else {
				const auto depth_stencil = std::get<1>(variant);
				clear_value.setDepthStencil(vk::ClearDepthStencilValue(depth_stencil.x, depth_stencil.y));
			}
			clear_values.push_back(clear_value);
		}
		cmd_handle.beginRenderPass({
			rp.handle.get(),
			fb.handle.get(),
			vk::Rect2D(vk::Offset2D(0, 0), render_area),
			static_cast<uint32_t>(clear_values.size()),
			clear_values.data()
			}, subass_behavior);

	}

	void RenderCommand::bind_graphics_pipeline(const ovk::GraphicsPipeline &pipeline) const {
		cmd_handle.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.handle.get());
	}


	void RenderCommand::bind_vertex_buffers(uint32_t first_binding, std::vector<BufferDescription> descriptions) const {
		std::vector<vk::Buffer> buffers;
		std::vector<vk::DeviceSize> offsets;

		for (auto& des : descriptions) {
			buffers.push_back(des.buffer.get().handle.get());
			offsets.push_back(des.offset);
		}

		cmd_handle.bindVertexBuffers(first_binding, buffers, offsets);
	}

	void RenderCommand::bind_index_buffer(const Buffer &buffer, vk::DeviceSize offset, vk::IndexType type) const {
		cmd_handle.bindIndexBuffer(buffer.handle.get(), offset, type);
	}

	void RenderCommand::bind_descriptor_sets(GraphicsPipeline& pipe, uint32_t first_set, std::vector<vk::DescriptorSet> sets, std::vector<uint32_t> dynamic_offsets) const {
		cmd_handle.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics, 
			pipe.layout.get(), 
			first_set, 
			static_cast<uint32_t>(sets.size()), 
			sets.data(),
			static_cast<uint32_t>(dynamic_offsets.size()),
      dynamic_offsets.data()
    );
	}

	void RenderCommand::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance) const {
		cmd_handle.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
	}

	void RenderCommand::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) const {
		cmd_handle.draw(vertex_count, instance_count, first_vertex, first_instance);
	}

	void RenderCommand::draw_imgui(ImGuiRenderer &renderer, int index, ImDrawData *draw_data) {
		renderer.cmd_render_imgui(*this, *device, index, draw_data);
	}
	
#ifdef OVK_RENDERDOC_COMPAT
	void RenderCommand::annotate(const std::string &annotation, glm::vec4 color) {
		assert(device);
		if (!device->debug_marker.insert) return;

		VkDebugMarkerMarkerInfoEXT marker_info = {};
		marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(marker_info.color, &color[0], sizeof(float) * 4);
		marker_info.pMarkerName = annotation.c_str();
		device->debug_marker.insert(cmd_handle, &marker_info);
		
	}

	void RenderCommand::begin_region(const std::string &name, glm::vec4 color) {
		assert(device);
		if (!device->debug_marker.begin) return;

		VkDebugMarkerMarkerInfoEXT marker_info = {};
		marker_info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
		memcpy(marker_info.color, &color[0], sizeof(float) * 4);
		marker_info.pMarkerName = name.c_str();
		device->debug_marker.begin(cmd_handle, &marker_info);
		
	}

	void RenderCommand::end_region() {
		assert(device);
		if (!device->debug_marker.end) return;

		device->debug_marker.end(cmd_handle);
	}
#else
	void RenderCommand::annotate(const std::string &annotation, glm::vec4 color) {
	}

	void RenderCommand::begin_region(const std::string &name, glm::vec4 color) {
	}

	void RenderCommand::end_region() {
	}
#endif

	void RenderCommand::end_render_pass() const {
		cmd_handle.endRenderPass();
	}

	void RenderCommand::copy(ovk::Buffer& src, uint32_t src_offset, ovk::Buffer& dst, uint32_t dst_offset, uint32_t size) {
		vk::BufferCopy copy {
		  src_offset, dst_offset, size												 
		};
		cmd_handle.copyBuffer(src.handle.get(), dst.handle.get(), 1, &copy);
	}


	RenderCommand::operator vk::CommandBuffer() const {
		return cmd_handle;
	}

	RenderCommand::RenderCommand(vk::CommandBuffer raw_handle, Device &d) : cmd_handle(raw_handle), device(&d) {}

	void RenderCommand::start() const {
		VK_ASSERT(cmd_handle.begin({ vk::CommandBufferUsageFlagBits::eSimultaneousUse }), "Failed to begin RenderCommand");

	}

	void RenderCommand::end() const {
		VK_ASSERT(cmd_handle.end(), "Failed to record RenderBuffer");
	}

	void RenderCommand::push_constant_impl(GraphicsPipeline &pipeline, vk::ShaderStageFlags stage, uint32_t offset, uint32_t size, void *data) const {
		cmd_handle.pushConstants(pipeline.layout.get(), stage, offset, size, data);
	}
}
