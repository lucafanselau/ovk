#include "pch.h"
#include "render_pass.h"


#include "device.h"
#include <optional>

namespace ovk {

	GraphicSubpass::GraphicSubpass(std::vector<vk::AttachmentDescription> _input, std::vector<vk::AttachmentDescription> _color,
		std::vector<vk::AttachmentDescription> _resolve, std::vector<vk::AttachmentDescription> _preserve, vk::AttachmentDescription _depth_stencil)
		: input(std::move(_input)),
			color(std::move(_color)),
			resolve(std::move(_resolve)),
			preserve(std::move(_preserve)),
			depth_stencil(std::move(_depth_stencil))
	{
		if (!resolve.empty() && resolve.size() != color.size()) spdlog::error("[ovk::GraphicSubpass] There need to be as many resolve attachments as there are color attachments");
	}

	template <typename T>
	std::optional<uint32_t> find_index(std::vector<T>& vec, T& e) {
		for (auto i = 0; i < vec.size(); i++) {
			if (vec[i] == e) return i;
		}
		return std::nullopt;
	}

	std::vector<vk::AttachmentReference> get_references(std::vector<vk::AttachmentDescription>& all_attachments, std::vector<vk::AttachmentDescription>& subpass_attachments, vk::ImageLayout layout, size_t i) {
		if (subpass_attachments.empty()) return {};
		std::vector<vk::AttachmentReference> refs;
		refs.reserve(subpass_attachments.size());
		for (auto&& input : subpass_attachments) {
			if (auto index = find_index(all_attachments, input); index.has_value())
				refs.emplace_back(index.value(), layout);
			else
				spdlog::error("[ovk::RenderPass] (Constructor) Subpass {}: Attachment not found", i);
		}

		return refs;

	}

	struct SubpassDescription {
		vk::SubpassDescription des;
		std::vector<vk::AttachmentReference> input, color, resolve;
		std::vector<uint32_t> preserve;
		std::optional<vk::AttachmentReference> depth;
	};

	RenderPass::RenderPass(std::vector<vk::AttachmentDescription> attachments, std::vector<GraphicSubpass> subpasses, bool add_external_dependency, Device &d) 
		: DeviceObject(d.device.get()) {

		std::vector<SubpassDescription> vk_subpasses;
		vk_subpasses.reserve(subpasses.size());

		for (auto i = 0; i < subpasses.size(); i++) {
			auto& subpass = subpasses[i];

			auto& vk_subpass = vk_subpasses.emplace_back();


			// Input attachments
			vk_subpass.input = get_references(attachments, subpass.input, vk::ImageLayout::eShaderReadOnlyOptimal, i);
			vk_subpass.color = get_references(attachments, subpass.color, vk::ImageLayout::eColorAttachmentOptimal, i);
			// TODO: WHAT LAYOUT?
			vk_subpass.resolve = get_references(attachments, subpass.resolve,  vk::ImageLayout::eUndefined, i);
			if (auto index = find_index(attachments, subpass.depth_stencil); index.has_value())
				vk_subpass.depth = vk::AttachmentReference{ index.value(), vk::ImageLayout::eDepthStencilAttachmentOptimal };

			for (auto &i : subpass.preserve) {
				if (auto index = find_index(attachments, i); index.has_value())
					vk_subpass.preserve.push_back(index.value());
				else
					spdlog::error("[RenderPass] (Constructor) Failed to find preserve Attachment");
			}

			vk_subpass.des = vk::SubpassDescription {
				{},
				vk::PipelineBindPoint::eGraphics,
				(uint32_t)vk_subpass.input.size(),
				vk_subpass.input.empty() ? nullptr : vk_subpass.input.data(),
				(uint32_t)vk_subpass.color.size(),
				vk_subpass.color.empty() ? nullptr : vk_subpass.color.data(),
				vk_subpass.resolve.empty() ? nullptr : vk_subpass.resolve.data(),
				vk_subpass.depth.has_value() ? &vk_subpass.depth.value() : nullptr,
				(uint32_t)vk_subpass.preserve.size(),
				vk_subpass.preserve.empty() ? nullptr : vk_subpass.preserve.data()
			};

		}

		// TODO: THINK ABOUT DEPENDENCIES
		std::vector<vk::SubpassDependency> dependencies;

		if (add_external_dependency) {
			dependencies.push_back(vk::SubpassDependency {
				VK_SUBPASS_EXTERNAL, 0,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{},
				vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
			});

		}

		std::vector<vk::SubpassDescription> s_passes; s_passes.reserve(vk_subpasses.size());
		for (auto& sub : vk_subpasses) s_passes.push_back(sub.des);

		vk::RenderPassCreateInfo create_info{
			{},
			(uint32_t) attachments.size(),
			attachments.data(),
			(uint32_t) s_passes.size(),
			s_passes.data(),
			(uint32_t) dependencies.size(),
			dependencies.empty() ? nullptr : dependencies.data()
		};

		handle.set(VK_CREATE(d.device->createRenderPass(create_info), "Failed to create RenderPass"));

	}
}