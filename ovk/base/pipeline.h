#pragma once

#include "handle.h"

#include <glm/glm.hpp>
#include <optional>

// Maybe we should not pull in this dependency here
#include <boost/pfr.hpp>

namespace ovk {
	class RenderPass;

	class GraphicsPipeline;

	class OVK_API GraphicsPipelineBuilder {
	public:

		GraphicsPipelineBuilder& add_shader_stage_from_file(vk::ShaderStageFlagBits stage, std::string file_path, bool compile_shader = false, const std::string &entry_point = "main");
		GraphicsPipelineBuilder& add_shader_stage_u32(vk::ShaderStageFlagBits stage, uint32_t* data, size_t size, const std::string& entry_point = "main");

		template <typename T>
		GraphicsPipelineBuilder& set_vertex_layout();
		
		GraphicsPipelineBuilder& set_vertex_layout(std::vector<vk::VertexInputBindingDescription> bindings, std::vector<vk::VertexInputAttributeDescription> attributes);
		GraphicsPipelineBuilder& set_input_assembly(vk::PrimitiveTopology topology, bool restart_enable = false);

		GraphicsPipelineBuilder& set_rasterizer(vk::PipelineRasterizationStateCreateInfo rasterization_state);

		GraphicsPipelineBuilder set_depth_stencil(bool depth_test_enabled = true, bool depth_write_enabled = true, vk::CompareOp compare = vk::CompareOp::eLess, bool depth_bounds = false, glm::vec2 bounds = { 0.0f, 0.0f });
		
		GraphicsPipelineBuilder& add_descriptor_set_layouts(std::vector<vk::DescriptorSetLayout> sets);
		GraphicsPipelineBuilder& add_push_constant(vk::ShaderStageFlagBits e_vertex, uint32_t size, uint32_t offset = 0);
		
		GraphicsPipelineBuilder& add_viewport(glm::vec2 origin, glm::vec2 extent, float min_depth, float max_depth);
		GraphicsPipelineBuilder& add_scissor(vk::Offset2D offset, vk::Extent2D extent);

		GraphicsPipelineBuilder& set_render_pass(vk::RenderPass render_pass, uint32_t subpass_index = 0);
		GraphicsPipelineBuilder& set_render_pass(ovk::RenderPass& render_pass, uint32_t subpass_index = 0);


		// TODO: Rasterizer and Multisampling and Color Blending

		GraphicsPipeline build();

	private:
		friend class Device;

		explicit GraphicsPipelineBuilder(Device* d);

		Device* device;
		std::vector<vk::ShaderModule> shaders;

		std::vector<vk::PipelineShaderStageCreateInfo> stages;
		vk::ShaderStageFlags stage_flags = {};

		std::vector<vk::VertexInputBindingDescription> bindings;
		std::vector<vk::VertexInputAttributeDescription> attributes;
		vk::PipelineVertexInputStateCreateInfo vertex_input;

		vk::PipelineInputAssemblyStateCreateInfo input_assembly;

		std::vector<vk::DescriptorSetLayout> set_layouts;
		std::vector<vk::PushConstantRange> push_constants;
		
		std::vector<vk::Viewport> viewports;
		std::vector<vk::Rect2D> scissors;

		vk::PipelineRasterizationStateCreateInfo rasterizer;

		vk::PipelineMultisampleStateCreateInfo multisampling;

		std::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
		vk::PipelineColorBlendStateCreateInfo color_blending;

		std::optional<vk::PipelineDepthStencilStateCreateInfo> depth_stencil = std::nullopt;

		std::optional<vk::RenderPass> render_pass = std::nullopt;
		uint32_t subpass = 0;

		vk::GraphicsPipelineCreateInfo info;
	};

	template <typename T>
	vk::Format internal_get_format(const T& v) {
		panic("[GraphicsPipelineBuilder] (set_vertex_layout<T>) unkndown field type: {}", typeid(T).name());
		return vk::Format::eUndefined;
	}
	
	// Ok so this function tries to provide a bit of magic to figure
	// out the vertex layout based on a struct.
	// Currently we only support one binding and nested structures will break
	// the code,
	// but for example:
	// struct MyCoolVertex {
	//	glm::vec3 pos;
	//	glm::vec2 tex_coord;
	//	glm::vec3 normal;
	// };
	// should work!
	template <typename T>
	GraphicsPipelineBuilder & GraphicsPipelineBuilder::set_vertex_layout() {
		static_assert(std::is_default_constructible_v<T>, "[GraphicsPipelineBuilder] (set_vertex_layout) T must be default constructable");
		std::vector<vk::VertexInputBindingDescription> bindings;
		
		bindings.emplace_back(0, sizeof(T), vk::VertexInputRate::eVertex);
		uint32_t current_location = 0, current_offset = 0;
		std::vector<vk::VertexInputAttributeDescription> attributes;

		boost::pfr::for_each_field(T(), [&attributes, &current_location, &current_offset](const auto& field) {
			vk::Format format = internal_get_format(field);
			uint32_t size = sizeof(decltype(field));
			
			attributes.emplace_back(current_location, 0, format, current_offset);
			current_location += 1;
			current_offset += size;
		});

		set_vertex_layout(bindings, attributes);
		return *this;
 	}

	template<>
	inline vk::Format internal_get_format<glm::vec3>(const glm::vec3& v) {
		return vk::Format::eR32G32B32Sfloat;;
	}

	template<>
	inline vk::Format internal_get_format<glm::vec2>(const glm::vec2& v) {
		return vk::Format::eR32G32Sfloat;;
	}
	
	class OVK_API GraphicsPipeline : public DeviceObject<vk::Pipeline> {
	public:
		UniqueHandle<vk::PipelineLayout> layout;
	private:
		friend GraphicsPipelineBuilder;
		GraphicsPipeline(vk::Pipeline pipeline_handle, vk::PipelineLayout layout, Device* device);

	};

}
