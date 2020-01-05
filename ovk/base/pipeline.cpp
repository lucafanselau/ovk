#include "pch.h"
#include "pipeline.h"

#include "device.h"
#include "render_pass.h"

#include <fstream>

namespace ovk {

	std::vector<char> read_file(const std::string& filename) {

		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			spdlog::error("failed to read binary file {}: couldn't open filestream", filename);
		}

		const auto file_size = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(file_size);

		// Goto start of file
		file.seekg(0);

		// Read Data
		file.read(buffer.data(), file_size);

		file.close();
		return buffer;

	}

	vk::ShaderModule create_shader_module(vk::Device* device, const std::vector<char>& code) {
		vk::ShaderModuleCreateInfo create_info = {};
		create_info.codeSize = code.size();
		create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

		const auto shader_module = VK_CREATE(device->createShaderModule(create_info), "failed to create shader module");
		
		return shader_module;
	}


	GraphicsPipelineBuilder & GraphicsPipelineBuilder::add_shader_stage_from_file(vk::ShaderStageFlagBits stage, std::string file_path, bool compile_shader,
	                                                                              const std::string &entry_point) {
		// Add to Stages Flags
		stage_flags |= stage;

		const auto code = read_file(file_path);

		// Index of new Shader Module
		const auto index = shaders.size();
		shaders.push_back(create_shader_module(&device->device.get(), code));

		const vk::PipelineShaderStageCreateInfo info { {}, stage, shaders[index], entry_point.c_str() };
		stages.push_back(info);

		return *this;

	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::add_shader_stage_u32(vk::ShaderStageFlagBits stage, uint32_t *data, size_t size, const std::string &entry_point) {
		stage_flags |= stage;

		const auto index = shaders.size();
		const auto shader_module = VK_CREATE(device->device->createShaderModule({ {}, size, data }), "Failed to create shader module from u32 source");
		shaders.push_back(shader_module);

		vk::PipelineShaderStageCreateInfo info{ {}, stage, shaders[index], entry_point.c_str() };
		stages.push_back(info);

		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::set_vertex_layout(std::vector<vk::VertexInputBindingDescription> _bindings,
		std::vector<vk::VertexInputAttributeDescription> _attributes) {

		bindings = std::move(_bindings);
		attributes = std::move(_attributes);

		vertex_input = vk::PipelineVertexInputStateCreateInfo(
			{},
			bindings.size(),
			bindings.data(),
			attributes.size(),
			attributes.data()
		);

		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::set_input_assembly(vk::PrimitiveTopology topology, bool restart_enable) {
		input_assembly.topology = topology;
		input_assembly.primitiveRestartEnable = restart_enable;
		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::set_rasterizer(vk::PipelineRasterizationStateCreateInfo rasterization_state) {
		rasterizer = rasterization_state;
		return *this;
	}

	GraphicsPipelineBuilder GraphicsPipelineBuilder::set_depth_stencil(bool depth_test_enabled, bool depth_write_enabled, vk::CompareOp compare, bool depth_bounds,
		glm::vec2 bounds) {
		depth_stencil = vk::PipelineDepthStencilStateCreateInfo{
			{},
			depth_test_enabled,
			depth_write_enabled,
			compare,
			depth_bounds,
			false,
			{}, {},
			bounds.x, bounds.y
		};
		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::add_descriptor_set_layouts(std::vector<vk::DescriptorSetLayout> sets) {
		set_layouts.insert(set_layouts.end(), sets.begin(), sets.end());
		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::add_push_constant(vk::ShaderStageFlagBits stage, uint32_t size, uint32_t offset) {
		push_constants.emplace_back(stage, offset, size);
		return *this;
	}


	GraphicsPipelineBuilder & GraphicsPipelineBuilder::add_viewport(glm::vec2 origin, glm::vec2 extent, float min_depth, float max_depth) {
		viewports.emplace_back(origin.x, origin.y, extent.x, extent.y, min_depth, max_depth);
		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::add_scissor(vk::Offset2D offset, vk::Extent2D extent) {
		scissors.emplace_back(offset, extent);
		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::set_render_pass(vk::RenderPass render_pass, uint32_t subpass_index) {
		this->render_pass = render_pass;
		subpass = subpass_index;
		return *this;
	}

	GraphicsPipelineBuilder & GraphicsPipelineBuilder::set_render_pass(ovk::RenderPass &render_pass, uint32_t subpass_index) {
		return set_render_pass(render_pass.handle.get(), subpass_index);
	}

	GraphicsPipeline GraphicsPipelineBuilder::build() {


		// Pipeline Stages
		const auto vertex_and_fragment = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
		if ((stage_flags & vertex_and_fragment) != vertex_and_fragment) spdlog::error(" [GraphicsPipelineBuilder] (build) Pipeline needs to have at least a vertex and a fragment shader");
		info.stageCount = stages.size();
		info.pStages = stages.data();

		// Viewport State
		// TODO: CHECK FOR FEATURE MULTIPLE VIEWPORTS
		if (viewports.empty() || scissors.empty())
			spdlog::error("[GraphicsPipelineBuilder] (build) You need to provide at least one Viewport and one Scissor");
		vk::PipelineViewportStateCreateInfo viewport {
			{},
			static_cast<uint32_t>(viewports.size()),
			viewports.data(),
			static_cast<uint32_t>(scissors.size()),
			scissors.data()
		};

		// Create Pipeline Layout
		vk::PipelineLayoutCreateInfo layout {
			{},
			static_cast<uint32_t>(set_layouts.size()),
			set_layouts.empty() ? nullptr : set_layouts.data(),
			static_cast<uint32_t>(push_constants.size()),
			push_constants.empty() ? nullptr : push_constants.data()
		};
		
		const auto pipeline_layout = VK_CREATE(device->device->createPipelineLayout(layout), "[GraphicsPipelineBuilder] (build) Failed to create Pipeline Layout");

		// Fill out Create Info
		info.pVertexInputState = &vertex_input;
		info.pInputAssemblyState = &input_assembly;
		info.pViewportState = &viewport;
		info.pRasterizationState = &rasterizer;
		info.pMultisampleState = &multisampling;
		info.pDepthStencilState = depth_stencil.has_value() ? &depth_stencil.value() : nullptr;
		info.pColorBlendState = &color_blending;
		info.pDynamicState = nullptr;

		info.layout = pipeline_layout;

		if (!render_pass) spdlog::error("[GraphicsPipelineBuilder] (build) You need to specify a RenderPass");
		info.renderPass = render_pass.value();
		info.subpass = subpass;

		info.basePipelineHandle = vk::Pipeline();
		info.basePipelineIndex = -1;

		const auto pipeline = VK_CREATE(device->device->createGraphicsPipeline(vk::PipelineCache(), info), "[GraphicsPipelineBuilder] (build) Failed to create vk::Pipeline");

		GraphicsPipeline graphics_pipeline(pipeline, pipeline_layout, device);

		// Delete Shader Modules
		for (auto& shader : shaders) {
			device->device->destroyShaderModule(shader);
		}

		return std::move(graphics_pipeline);
	}

	GraphicsPipelineBuilder::GraphicsPipelineBuilder(Device* d)
		: device(d),
		input_assembly({}, vk::PrimitiveTopology::eTriangleList, false),
		rasterizer({}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone, vk::FrontFace::eCounterClockwise, false, 0, 0, 0, 1.0f),
		multisampling({}, vk::SampleCountFlagBits::e1, false) {

		// Color Blending (Default No Blending)
		{
				vk::PipelineColorBlendAttachmentState blend_attachment{
					VK_TRUE,
					vk::BlendFactor::eSrcAlpha,
					vk::BlendFactor::eOneMinusSrcAlpha,
					vk::BlendOp::eAdd,
					vk::BlendFactor::eOne,
					vk::BlendFactor::eZero,
					vk::BlendOp::eAdd,
			};
			using comp = vk::ColorComponentFlagBits;
			blend_attachment.colorWriteMask = comp::eR | comp::eG | comp::eB | comp::eA;
			blend_attachments.push_back(blend_attachment);

			color_blending = vk::PipelineColorBlendStateCreateInfo(
				{},
				false,
				vk::LogicOp::eCopy,
				blend_attachments.size(),
				blend_attachments.data()
			);
		}
	}

	GraphicsPipeline::GraphicsPipeline(vk::Pipeline pipeline_handle, vk::PipelineLayout layout, Device *device)
		: DeviceObject(device->device.get(), pipeline_handle),
			layout(std::forward<vk::PipelineLayout>(layout), ObjectDestroy<vk::PipelineLayout>(device->device.get())) {}

}
